#include <math.h>
#include <stdlib.h>
#include "gas_internal.h"
typedef struct {double mass,energy;} rates_t;
static rates_t rates(const double *volume,const double *velocity,size_t index,
    uint32_t width,uint32_t height,uint32_t axis,int direction) {
    uint32_t coordinate=axis?(uint32_t)(index/width):(uint32_t)(index%width),length=axis?height:width;
    if(volume[index]<=0||(direction>0?coordinate+1u>=length:coordinate==0u)) return (rates_t){0};
    size_t neighbor=direction>0?index+(axis?width:1u):index-(axis?width:1u);
    if(volume[neighbor]<=0) return (rates_t){0};
    double own=velocity[index*2u+axis],other=velocity[neighbor*2u+axis];
    double speed=fmax(fabs(own),fabs(other)),area=.5*fmin(volume[index],volume[neighbor])/volume[index];
    return (rates_t){area*(speed+direction*own),area*speed};
}
bool fr_gas_transport_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,fr_gas_state_t *out,char *err,uint32_t cap) {
    uint64_t wide=(uint64_t)width*height;
    if(!m||!s||!v||!out||!wide||wide>SIZE_MAX/(4u*sizeof(*s))||!isfinite(dt)||dt<0)
        return gas_error(err,cap,"Invalid large-CFL transport request");
    size_t count=(size_t)wide;bool okay=false;
    fr_gas_state_t *source=malloc(count*sizeof(*source)),*next=calloc(count,sizeof(*next)),*flux=calloc(count,sizeof(*flux));
    double *velocity=calloc(count*2u,sizeof(*velocity));
    if(!source||!next||!flux||!velocity) {gas_error(err,cap,"Cannot allocate cold transport oracle");goto done;}
    memcpy(source,s,count*sizeof(*s));
    for(size_t i=0;i<count;i++) {
        fr_gas_thermo_t t;
        if(!isfinite(v[i])||v[i]<0||v[i]>1||!fr_gas_thermodynamics(m,s+i,v[i],&t,err,cap)) goto done;
        if(t.mass>0) for(uint32_t axis=0;axis<2;axis++) velocity[i*2u+axis]=s[i].momentum[axis]/t.mass;
    }
    if(dt==0) {memcpy(out,s,count*sizeof(*s));okay=true;goto done;}
    for(uint32_t axis=0;axis<2;axis++) for(uint32_t reverse=0;reverse<2;reverse++) {
        uint32_t length=axis?height:width,lines=axis?width:height;int direction=reverse?-1:1;
        for(uint32_t line=0;line<lines;line++) for(uint32_t cell=0;cell<length;cell++) {
            uint32_t at=reverse?length-1u-cell:cell;
            size_t index=axis?(size_t)at*width+line:(size_t)line*width+at;
            size_t previous=cell?(direction>0?index-(axis?width:1u):index+(axis?width:1u)):index;
            rates_t outgoing=rates(v,velocity,index,width,height,axis,direction);
            rates_t incoming=cell?rates(v,velocity,previous,width,height,axis,direction):(rates_t){0};
            double original[GAS_WORDS],upstream[GAS_WORDS]={0},solved[GAS_WORDS],flow[GAS_WORDS];
            memcpy(original,source+index,sizeof(original));if(cell) memcpy(upstream,next+previous,sizeof(upstream));
            for(uint32_t word=0;word<GAS_WORDS;word++) {
                double own=word==GAS_ENERGY?outgoing.energy:outgoing.mass;
                double from=word==GAS_ENERGY?incoming.energy:incoming.mass;
                solved[word]=(original[word]+dt*from*upstream[word])/(1.+dt*own);
                flow[word]=dt*own*solved[word];
            }
            memcpy(next+index,solved,sizeof(solved));memcpy(flux+index,flow,sizeof(flow));
        }
        for(uint32_t line=0;line<lines;line++) for(uint32_t at=0;at<length;at++) {
            size_t index=axis?(size_t)at*width+line:(size_t)line*width+at;
            bool has_previous=direction>0?at>0:at+1u<length;
            size_t previous=has_previous?(direction>0?index-(axis?width:1u):index+(axis?width:1u)):index;
            fr_gas_state_t zero={0},incoming=has_previous?flux[previous]:zero;
            fr_gas_state_t change=gas_combine(&incoming,1,flux+index,-1);
            next[index]=gas_combine(source+index,1,&change,1);
        }
        fr_gas_state_t *swap=source;source=next;next=swap;
    }
    for(size_t i=0;i<count;i++) {fr_gas_thermo_t t;if(!fr_gas_thermodynamics(m,source+i,v[i],&t,err,cap)) goto done;}
    memcpy(out,source,count*sizeof(*out));okay=true;
done:
    free(source);free(next);free(flux);free(velocity);return okay;
}
