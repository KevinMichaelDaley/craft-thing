#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/gas.h"
#include "gas_internal.h"

static bool dimensions(uint32_t width,uint32_t height,double dx,size_t *count) {
    uint64_t n=(uint64_t)width*height;
    if(!n||!isfinite(dx)||dx<=0||n>SIZE_MAX/sizeof(fr_gas_state_t)) return false;
    *count=(size_t)n;return true;
}
static double volume_unit(uint32_t width,uint32_t height,double dx) {return width>1&&height>1?dx*dx:dx;}
bool fr_gas_reference_max_dt(const fr_gas_model_t *m,const fr_gas_state_t *s,const double *v,
    uint32_t width,uint32_t height,double dx,double courant,double *out,char *err,uint32_t cap) {
    size_t count=0;
    if(!s||!v||!out||!dimensions(width,height,dx,&count)||!isfinite(courant)||courant<=0||courant>.4)
        return gas_error(err,cap,"Invalid gas reference CFL request");
    double rate=0,unit=volume_unit(width,height,dx);
    for(size_t i=0;i<count;i++) {
        fr_gas_thermo_t t;
        if(!isfinite(v[i])||v[i]<0||v[i]>1||!fr_gas_thermodynamics(m,s+i,v[i]*unit,&t,err,cap)) return false;
        if(t.mass==0) continue;
        double local=0;
        if(width>1||height==1) local+=(fabs(s[i].momentum[0])/t.mass+t.sound_speed)/dx;
        if(height>1) local+=(fabs(s[i].momentum[1])/t.mass+t.sound_speed)/dx;
        if(local>rate) rate=local;
    }
    *out=rate>0?courant/rate:DBL_MAX;return true;
}
static void physical_flux(const fr_gas_model_t *m,const fr_gas_state_t *density,
    double pressure,double velocity,uint32_t axis,fr_gas_state_t *out) {
    *out=gas_combine(density,velocity,density,0);
    out->momentum[axis]+=pressure;out->energy+=m->kinetic_scale*pressure*velocity;
}
static bool star(const fr_gas_model_t *m,const fr_gas_state_t *q,double rho,double u,
    double wave,double contact,double pressure,uint32_t axis,fr_gas_state_t *out) {
    double factor=(wave-u)/(wave-contact);
    if(!isfinite(factor)||factor<=0||rho<=0) return false;
    *out=gas_combine(q,factor,q,0);out->momentum[axis]=rho*factor*contact;
    out->energy=((wave-u)*q->energy-m->kinetic_scale*pressure*u+
        m->kinetic_scale*(pressure+rho*(wave-u)*(contact-u))*contact)/(wave-contact);
    double kinetic=.5*m->kinetic_scale*(out->momentum[0]*out->momentum[0]+out->momentum[1]*out->momentum[1])/(rho*factor);
    return isfinite(out->energy)&&out->energy>kinetic;
}
static bool riemann(const fr_gas_model_t *m,const fr_gas_state_t *left,double vl,
    const fr_gas_state_t *right,double vr,uint32_t axis,uint32_t solver,
    fr_gas_state_t *out,char *err,uint32_t cap) {
    fr_gas_thermo_t a,b;
    if(!fr_gas_thermodynamics(m,left,vl,&a,err,cap)||!fr_gas_thermodynamics(m,right,vr,&b,err,cap)) return false;
    fr_gas_state_t ql=gas_combine(left,1./vl,left,0),qr=gas_combine(right,1./vr,right,0),fl,fr;
    double rho_l=a.mass/vl,rho_r=b.mass/vr,u=a.mass?left->momentum[axis]/a.mass:0,v=b.mass?right->momentum[axis]/b.mass:0;
    physical_flux(m,&ql,a.pressure,u,axis,&fl);physical_flux(m,&qr,b.pressure,v,axis,&fr);
    double lo=fmin(u-a.sound_speed,v-b.sound_speed),hi=fmax(u+a.sound_speed,v+b.sound_speed);
    if(lo>=0) {*out=fl;return true;}
    if(hi<=0) {*out=fr;return true;}
    if(solver==FR_GAS_FLUX_HLLC&&rho_l>0&&rho_r>0) {
        double divisor=rho_l*(lo-u)-rho_r*(hi-v);
        double contact=(b.pressure-a.pressure+rho_l*u*(lo-u)-rho_r*v*(hi-v))/divisor;
        fr_gas_state_t selected;
        bool okay=isfinite(contact)&&contact>=lo&&contact<=hi;
        if(okay&&contact>=0) {
            okay=star(m,&ql,rho_l,u,lo,contact,a.pressure,axis,&selected);
            if(okay) {*out=gas_combine(&fl,1.,&selected,lo);*out=gas_combine(out,1.,&ql,-lo);return true;}
        } else if(okay) {
            okay=star(m,&qr,rho_r,v,hi,contact,b.pressure,axis,&selected);
            if(okay) {*out=gas_combine(&fr,1.,&selected,hi);*out=gas_combine(out,1.,&qr,-hi);return true;}
        }
    }
    fr_gas_state_t jump=gas_combine(&qr,1.,&ql,-1.);
    *out=gas_combine(&fl,hi/(hi-lo),&fr,-lo/(hi-lo));
    *out=gas_combine(out,1.,&jump,lo*hi/(hi-lo));return true;
}
static bool face(const fr_gas_model_t *m,const fr_gas_state_t *left,double a,
    const fr_gas_state_t *right,double b,double unit,double area,uint32_t axis,uint32_t solver,
    fr_gas_state_t *out,char *err,uint32_t cap) {
    if(a==0&&b==0) {*out=(fr_gas_state_t){0};return true;}
    fr_gas_state_t l=*left,r=*right;
    double aperture=fmin(a,b);
    if(a==0) {l=r;l.momentum[axis]*=-1;a=b;aperture=b;}
    if(b==0) {r=l;r.momentum[axis]*=-1;b=a;aperture=a;}
    if(!riemann(m,&l,a*unit,&r,b*unit,axis,solver,out,err,cap)) return false;
    *out=gas_combine(out,aperture*area,out,0);return true;
}
bool fr_gas_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *source,const double *volumes,
    uint32_t width,uint32_t height,double dx,double dt,uint32_t solver,
    fr_gas_state_t *out,char *err,uint32_t cap) {
    size_t count=0;double allowed=0;
    if(!out||!dimensions(width,height,dx,&count)||!isfinite(dt)||dt<0||solver>FR_GAS_FLUX_HLLC||
       !fr_gas_reference_max_dt(m,source,volumes,width,height,dx,.4,&allowed,err,cap)||dt>allowed*(1.+8.*DBL_EPSILON))
        return gas_error(err,cap,"Gas reference step violates its validated CFL bound");
    if(dt==0) {memmove(out,source,count*sizeof(*source));return true;}
    fr_gas_state_t *result=malloc(count*sizeof(*result)),*horizontal=malloc(count*sizeof(*horizontal)),*vertical=malloc(count*sizeof(*vertical));
    if(!result||!horizontal||!vertical) {free(result);free(horizontal);free(vertical);return gas_error(err,cap,"Cannot stage gas reference fields");}
    double unit=volume_unit(width,height,dx),area=width>1&&height>1?dx:1.;bool okay=true;
    fr_gas_state_t empty={0};
    for(uint32_t y=0;okay&&y<height;y++) for(uint32_t x=0;okay&&x<width;x++) {
        size_t i=(size_t)y*width+x;
        okay=face(m,source+i,volumes[i],x+1u<width?source+i+1:&empty,x+1u<width?volumes[i+1]:0,
            unit,area,0,solver,horizontal+i,err,cap);
        if(okay&&height>1) okay=face(m,source+i,volumes[i],y+1u<height?source+i+width:&empty,y+1u<height?volumes[i+width]:0,
            unit,area,1,solver,vertical+i,err,cap);
    }
    for(uint32_t y=0;okay&&y<height;y++) for(uint32_t x=0;okay&&x<width;x++) {
        size_t i=(size_t)y*width+x;result[i]=source[i];if(volumes[i]==0) continue;
        fr_gas_state_t l,u;
        if(x>0) l=horizontal[i-1];
        else okay=face(m,&empty,0,source+i,volumes[i],unit,area,0,solver,&l,err,cap);
        if(!okay) break;
        result[i]=gas_combine(result+i,1.,horizontal+i,-dt);result[i]=gas_combine(result+i,1.,&l,dt);
        if(height>1) {
            if(y>0) u=vertical[i-width];
            else okay=face(m,&empty,0,source+i,volumes[i],unit,area,1,solver,&u,err,cap);
            if(!okay) break;
            result[i]=gas_combine(result+i,1.,vertical+i,-dt);result[i]=gas_combine(result+i,1.,&u,dt);
        }
        fr_gas_thermo_t t;if(okay) okay=fr_gas_thermodynamics(m,result+i,volumes[i]*unit,&t,err,cap);
    }
    if(okay) memcpy(out,result,count*sizeof(*out));
    free(result);free(horizontal);free(vertical);return okay;
}
