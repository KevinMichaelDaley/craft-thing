#include <math.h>
#include <stdlib.h>
#include "gas_internal.h"
#include "ferrum/fire/gas_constants.h"
typedef struct {double mass,energy;} rates_t;
static rates_t rates(const double *volume,const double *velocity,size_t index,
    uint32_t width,uint32_t height,uint32_t axis,int direction,double energy_fraction) {
    uint32_t coordinate=axis?(uint32_t)(index/width):(uint32_t)(index%width),length=axis?height:width;
    if(volume[index]<=0||(direction>0?coordinate+1u>=length:coordinate==0u)) return (rates_t){0};
    size_t neighbor=direction>0?index+(axis?width:1u):index-(axis?width:1u);
    if(volume[neighbor]<=0) return (rates_t){0};
    double own=velocity[index*2u+axis],other=velocity[neighbor*2u+axis];
    double speed=fmax(fabs(own),fabs(other)),area=.5*fmin(volume[index],volume[neighbor])/volume[index];
    return (rates_t){area*(speed+direction*own),area*(speed+energy_fraction*direction*own)};
}
static fr_gas_state_t face_flux(const fr_gas_state_t *state,const double *v,const double *velocity,
    size_t index,uint32_t width,uint32_t height,uint32_t axis,double energy_fraction) {
    uint32_t coordinate=axis?(uint32_t)(index/width):(uint32_t)(index%width),length=axis?height:width;
    if(coordinate+1u>=length) return (fr_gas_state_t){0};
    size_t other=index+(axis?width:1u);rates_t plus=rates(v,velocity,index,width,height,axis,1,energy_fraction),minus=rates(v,velocity,other,width,height,axis,-1,energy_fraction);
    fr_gas_state_t result=gas_combine(state+index,plus.mass,state+other,-minus.mass);
    result.energy=state[index].energy*plus.energy-state[other].energy*minus.energy;return result;
}
static bool transport_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,double energy_fraction,fr_gas_state_t *out,char *err,uint32_t cap) {
    uint64_t wide=(uint64_t)width*height;
    if(!m||!s||!v||!out||!wide||wide>SIZE_MAX/(4u*sizeof(*s))||!isfinite(dt)||dt<0)
        return gas_error(err,cap,"Invalid large-CFL transport request");
    size_t count=(size_t)wide;bool okay=false;
    fr_gas_state_t *source=malloc(count*sizeof(*source)),*next=calloc(count,sizeof(*next)),*flux=calloc(count,sizeof(*flux));
    double *velocity=calloc(count*2u,sizeof(*velocity)),*gamma=calloc(count*2u,sizeof(*gamma));
    if(!source||!next||!flux||!velocity||!gamma) {gas_error(err,cap,"Cannot allocate cold transport oracle");goto done;}
    memcpy(source,s,count*sizeof(*s));
    for(size_t i=0;i<count;i++) {
        fr_gas_thermo_t t;
        if(!isfinite(v[i])||v[i]<0||v[i]>1||!fr_gas_thermodynamics(m,s+i,v[i],&t,err,cap)) goto done;
        if(t.mass>0) for(uint32_t axis=0;axis<2;axis++) velocity[i*2u+axis]=s[i].momentum[axis]/t.mass;
    }
    if(dt==0) {memcpy(out,s,count*sizeof(*s));okay=true;goto done;}
    for(uint32_t axis=0;axis<2;axis++) {
        uint32_t length=axis?height:width,lines=axis?width:height;
        for(uint32_t line=0;line<lines;line++) for(uint32_t cell=0;cell<length;cell++) {
            size_t index=axis?(size_t)cell*width+line:(size_t)line*width+cell;
            size_t previous=cell?index-(axis?width:1u):index,next_index=cell+1u<length?index+(axis?width:1u):index;
            rates_t plus=rates(v,velocity,index,width,height,axis,1,energy_fraction),minus=rates(v,velocity,index,width,height,axis,-1,energy_fraction);
            rates_t left=cell?rates(v,velocity,previous,width,height,axis,1,energy_fraction):(rates_t){0};
            rates_t right=cell+1u<length?rates(v,velocity,next_index,width,height,axis,-1,energy_fraction):(rates_t){0};
            double diagonal[2]={1.+dt*(plus.mass+minus.mass),1.+dt*(plus.energy+minus.energy)};
            diagonal[0]-=cell?dt*left.mass*gamma[previous*2u]:0.;diagonal[1]-=cell?dt*left.energy*gamma[previous*2u+1u]:0.;
            if(diagonal[0]<=0||diagonal[1]<=0||!isfinite(diagonal[0])||!isfinite(diagonal[1])) {gas_error(err,cap,"Invalid cold transport factorization");goto done;}
            gamma[index*2u]=dt*right.mass/diagonal[0];gamma[index*2u+1u]=dt*right.energy/diagonal[1];
            double original[GAS_WORDS],upstream[GAS_WORDS]={0},solved[GAS_WORDS];
            memcpy(original,source+index,sizeof(original));if(cell) memcpy(upstream,next+previous,sizeof(upstream));
            for(uint32_t word=0;word<GAS_WORDS;word++) {
                uint32_t mode=word==GAS_ENERGY?1u:0u;double incoming=mode?left.energy:left.mass;
                solved[word]=(original[word]+dt*incoming*upstream[word])/diagonal[mode];
            }
            memcpy(next+index,solved,sizeof(solved));
        }
        for(uint32_t line=0;line<lines;line++) for(uint32_t remaining=length;remaining>0;remaining--) {
            uint32_t cell=remaining-1u;size_t index=axis?(size_t)cell*width+line:(size_t)line*width+cell;
            double value[GAS_WORDS],downstream[GAS_WORDS]={0};memcpy(value,next+index,sizeof(value));
            if(cell+1u<length) memcpy(downstream,flux+index+(axis?width:1u),sizeof(downstream));
            for(uint32_t word=0;word<GAS_WORDS;word++) value[word]+=gamma[index*2u+(word==GAS_ENERGY?1u:0u)]*downstream[word];
            memcpy(flux+index,value,sizeof(value));
        }
        for(uint32_t line=0;line<lines;line++) for(uint32_t cell=0;cell<length;cell++) {
            size_t index=axis?(size_t)cell*width+line:(size_t)line*width+cell;
            fr_gas_state_t incoming=cell?face_flux(flux,v,velocity,index-(axis?width:1u),width,height,axis,energy_fraction):(fr_gas_state_t){0};
            fr_gas_state_t outgoing=face_flux(flux,v,velocity,index,width,height,axis,energy_fraction);
            fr_gas_state_t change=gas_combine(&incoming,1,&outgoing,-1);next[index]=gas_combine(source+index,1,&change,dt);
        }
        fr_gas_state_t *swap=source;source=next;next=swap;
    }
    for(size_t i=0;i<count;i++) {fr_gas_thermo_t t;if(!fr_gas_thermodynamics(m,source+i,v[i],&t,err,cap)) goto done;}
    memcpy(out,source,count*sizeof(*out));okay=true;
done:
    free(source);free(next);free(flux);free(velocity);free(gamma);return okay;
}
bool fr_gas_transport_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,fr_gas_state_t *out,char *err,uint32_t cap) {
    return transport_reference_step(m,s,v,width,height,dt,0,out,err,cap);
}
bool fr_gas_large_energy_transport_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,fr_gas_state_t *out,char *err,uint32_t cap) {
    return transport_reference_step(m,s,v,width,height,dt,FR_GAS_LARGE_ENERGY_ADVECTION,out,err,cap);
}
