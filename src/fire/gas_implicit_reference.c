#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gas_internal.h"
#include "ferrum/fire/gas_constants.h"

typedef struct {size_t a,b;uint32_t axis;double area,enthalpy,momentum,energy_diffusion;} face_t;
typedef struct {double pressure,capacity,enthalpy,kinetic;} cell_t;

static fr_gas_state_t advective_flux(const fr_gas_state_t *a,const fr_gas_state_t *b,
    double va,double vb,double area,uint32_t axis) {
    double ma=0,mb=0;
    for(uint32_t k=0;k<FR_GAS_SPECIES;k++) {ma+=a->species[k];mb+=b->species[k];}
    double ua=ma?a->momentum[axis]/ma:0,ub=mb?b->momentum[axis]/mb:0;
    double speed=fmax(fabs(ua),fabs(ub));
    return gas_combine(a,area*(ua+speed)*.5/va,b,area*(ub-speed)*.5/vb);
}
static bool implicit_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,bool large,fr_gas_state_t *out,char *err,uint32_t cap) {
    uint64_t wide=(uint64_t)width*height;
    if(!m||!s||!v||!out||!wide||wide>SIZE_MAX/(2*sizeof(face_t))||!isfinite(dt)||dt<0)
        return gas_error(err,cap,"Invalid implicit gas reference request");
    size_t count=(size_t)wide,face_count=0;bool okay=false;
    fr_gas_state_t *star=malloc(count*sizeof(*star)),*result=malloc(count*sizeof(*result));
    cell_t *cells=calloc(count,sizeof(*cells));face_t *faces=calloc(count*2,sizeof(*faces));
    double *p=calloc(count,sizeof(*p)),*rhs=calloc(count,sizeof(*rhs)),*diagonal=calloc(count,sizeof(*diagonal));
    if(!star||!result||!cells||!faces||!p||!rhs||!diagonal) {gas_error(err,cap,"Cannot allocate cold implicit oracle");goto done;}
    memcpy(star,s,count*sizeof(*s));double axis_rate[2]={0};
    for(size_t i=0;i<count;i++) {
        fr_gas_thermo_t t;
        if(!isfinite(v[i])||v[i]<0||v[i]>1||!fr_gas_thermodynamics(m,s+i,v[i],&t,err,cap)) goto done;
        if(!t.mass) continue;
        if(width>1) axis_rate[0]=fmax(axis_rate[0],fabs(s[i].momentum[0])/t.mass);
        if(height>1) axis_rate[1]=fmax(axis_rate[1],fabs(s[i].momentum[1])/t.mass);
        cells[i].pressure=t.pressure;cells[i].enthalpy=(s[i].energy+m->kinetic_scale*t.pressure*v[i])/t.mass;
        cells[i].kinetic=.5*m->kinetic_scale*(s[i].momentum[0]*s[i].momentum[0]+s[i].momentum[1]*s[i].momentum[1])/t.mass;
    }
    double advective_step=(axis_rate[0]+axis_rate[1])*dt;
    if(!large&&advective_step>FR_GAS_ADVECTIVE_COURANT) {gas_error(err,cap,"Implicit gas violates advective axis CFL");goto done;}
    if(large&&advective_step<=FR_GAS_ADVECTIVE_COURANT) large=false;
    if(large) for(size_t i=0;i<count;i++) {
        double mass=0;for(uint32_t k=0;k<5;k++) mass+=s[i].species[k];
        if(mass>0) cells[i].enthalpy=((1.-FR_GAS_LARGE_ENERGY_ADVECTION)*s[i].energy+m->kinetic_scale*cells[i].pressure*v[i])/mass;
    }
    if(dt==0) {memcpy(out,s,count*sizeof(*s));okay=true;goto done;}
    if(large&&!fr_gas_large_energy_transport_reference_step(m,s,v,width,height,dt,star,err,cap)) goto done;
    for(size_t i=0;i<count;i++) for(uint32_t axis=0;axis<2;axis++) {
        if((axis==0&&i%width+1>=width)||(axis==1&&i+width>=count)) continue;
        size_t j=i+(axis?width:1);double area=fmin(v[i],v[j]);if(area<=0) continue;
        fr_gas_state_t flux=advective_flux(s+i,s+j,v[i],v[j],area,axis);
        double ma=0,mb=0;for(uint32_t k=0;k<5;k++) {ma+=s[i].species[k];mb+=s[j].species[k];}
        double speed=fmax(ma?fabs(s[i].momentum[axis])/ma:0,mb?fabs(s[j].momentum[axis])/mb:0);
        face_t face={.a=i,.b=j,.axis=axis,.area=area,.enthalpy=.5*(cells[i].enthalpy+cells[j].enthalpy),
            .energy_diffusion=large?0:-.5*area*speed*(s[j].energy/v[j]-s[i].energy/v[i])};
        faces[face_count++]=face;
        if(!large) {flux.energy=0;star[i]=gas_combine(star+i,1,&flux,-dt);star[j]=gas_combine(star+j,1,&flux,dt);}
    }
    for(size_t i=0;i<count;i++) {
        double capacity=0,molecular=0;
        for(uint32_t k=0;k<5;k++) {capacity+=star[i].species[k]*m->heat_capacity_cv[k];molecular+=star[i].species[k]*m->gas_constant[k];}
        if(molecular>0) cells[i].capacity=m->kinetic_scale*v[i]*capacity/molecular;
        p[i]=cells[i].pressure;rhs[i]=(large?star[i].energy:s[i].energy)-cells[i].kinetic;diagonal[i]=cells[i].capacity;
    }
    for(size_t f=0;f<face_count;f++) {
        face_t *face=faces+f;size_t i=face->a,j=face->b;uint32_t axis=face->axis;
        face->momentum=.5*(star[i].momentum[axis]/v[i]+star[j].momentum[axis]/v[j])*face->area;
        double energy=dt*(face->enthalpy*face->momentum+face->energy_diffusion);
        rhs[i]-=energy;rhs[j]+=energy;
        double coefficient=dt*dt*face->area*face->enthalpy;
        diagonal[i]+=coefficient;diagonal[j]+=coefficient;
    }
    double *neighbor=calloc(count,sizeof(*neighbor));if(!neighbor) {gas_error(err,cap,"Cannot allocate pressure oracle");goto done;}
    double error=1.;
    uint64_t iterations=0;bool trace=getenv("DC_GAS_ORACLE_TRACE")!=NULL;
    while(error>1e-11) {
        memset(neighbor,0,count*sizeof(*neighbor));
        for(size_t f=0;f<face_count;f++) {
            face_t face=faces[f];double coefficient=dt*dt*face.area*face.enthalpy;
            neighbor[face.a]+=coefficient*p[face.b];neighbor[face.b]+=coefficient*p[face.a];
        }
        error=0;
        for(size_t i=0;i<count;i++) if(diagonal[i]>0) {
            double next=.5*p[i]+.5*((rhs[i]+neighbor[i])/diagonal[i]);
            if(!isfinite(next)||next<=0) {free(neighbor);gas_error(err,cap,"Implicit gas pressure is not positive");goto done;}
            double residual=fabs(diagonal[i]*p[i]-rhs[i]-neighbor[i])/fmax(1.,fabs(rhs[i]));
            error=fmax(error,residual);p[i]=next;
        }
        iterations++;
        if(trace&&(iterations&(iterations-1u))==0u) fprintf(stderr,"Cold pressure n%zu iteration%llu residual%.17g p0%.17g rhs0%.17g diagonal0%.17g\n",count,(unsigned long long)iterations,error,p[0],rhs[0],diagonal[0]);
    }
    free(neighbor);memcpy(result,star,count*sizeof(*star));
    if(!large) for(size_t i=0;i<count;i++) result[i].energy=s[i].energy;
    for(size_t f=0;f<face_count;f++) {
        face_t face=faces[f];size_t i=face.a,j=face.b;uint32_t axis=face.axis;
        double momentum=face.momentum-dt*face.area*(p[j]-p[i]);
        double energy=dt*(face.enthalpy*momentum+face.energy_diffusion);
        result[i].energy-=energy;result[j].energy+=energy;
        double pressure=.5*(p[i]+p[j]);
        result[i].momentum[axis]-=dt*face.area*(pressure-p[i]);
        result[j].momentum[axis]+=dt*face.area*(pressure-p[j]);
    }
    for(size_t i=0;i<count;i++) {fr_gas_thermo_t t;if(!fr_gas_thermodynamics(m,result+i,v[i],&t,err,cap)) goto done;}
    memcpy(out,result,count*sizeof(*result));okay=true;
done:
    free(star);free(result);free(cells);free(faces);free(p);free(rhs);free(diagonal);return okay;
}
bool fr_gas_implicit_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,fr_gas_state_t *out,char *err,uint32_t cap) {
    return implicit_reference_step(m,s,v,width,height,dt,false,out,err,cap);
}
bool fr_gas_large_cfl_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,fr_gas_state_t *out,char *err,uint32_t cap) {
    return implicit_reference_step(m,s,v,width,height,dt,true,out,err,cap);
}
