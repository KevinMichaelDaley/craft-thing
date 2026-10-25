#include <math.h>
#include <stdio.h>
#include "smoke_audit.h"
bool dc_smoke_audit_capture(dc_level_view_t *view,const char *phase,fr_gas_gpu_audit_t *out,char *err,uint32_t cap) {
    if(!dc_level_view_smoke_audit(view,out,err,cap)) return false;
    const float *v=out->metrics;
    printf("Smoke audit %s stock: species %.9g %.9g %.9g %.9g %.9g pigment %.9g %.9g %.9g energy %.9g work %.9g thermal %.9g kinetic %.9g mass %.9g volume %.9g cells %.0f invalid %.0f\n",
        phase,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15]);
    printf("Smoke audit %s motion: velocity bins(<.05,<1,<10,>=10) %.0f %.0f %.0f %.0f maxSpeed %.9g maxPressure %.9g temperature %.9g..%.9g wake(velocity,gravity,pressure,balanced,carrier,raw,net) %.0f %.0f %.0f %.0f %.0f %.0f %.0f maxAcceleration %.9g\n",
        phase,v[16],v[17],v[18],v[19],v[20],v[21],v[23],v[22],v[24],v[25],v[26],v[27],v[28],v[29],v[30],v[31]);
    return true;
}
bool dc_smoke_audit_compare(const fr_gas_gpu_audit_t *before,const fr_gas_gpu_audit_t *after,char *err,uint32_t cap) {
    const float *a=before->metrics,*b=after->metrics;bool okay=true;double species=0,pigment=0;
    for(uint32_t k=0;k<FR_GAS_AUDIT_WORDS;k++) if(!isfinite(a[k])||!isfinite(b[k])) okay=false;
    for(uint32_t k=0;k<8;k++) {
        double difference=fabs((double)b[k]-a[k]),relative=a[k]>0?difference/a[k]:difference;
        if(k<5) species=fmax(species,relative);else pigment=fmax(pigment,relative);
        if(difference>fmax(1e-9,(double)a[k]*.003)) okay=false;
    }
    double energy=(double)b[FR_GAS_AUDIT_ENERGY]-a[FR_GAS_AUDIT_ENERGY]-b[FR_GAS_AUDIT_WORK]+a[FR_GAS_AUDIT_WORK];
    if(fabs(energy)>(double)a[FR_GAS_AUDIT_ENERGY]*.03||b[FR_GAS_AUDIT_INVALID]!=0||
       b[FR_GAS_AUDIT_MIN_TEMPERATURE]<=0||b[FR_GAS_AUDIT_MAX_TEMPERATURE]>=5000||
       b[FR_GAS_AUDIT_MAX_PRESSURE]>=a[FR_GAS_AUDIT_MAX_PRESSURE]*10||b[FR_GAS_AUDIT_MAX_SPEED]>=1000) okay=false;
    printf("Smoke audit drift: maxSpecies %.9g maxPigment %.9g energyMinusWork %.9g relativeEnergy %.9g\n",
        species,pigment,energy,energy/fmax(a[FR_GAS_AUDIT_ENERGY],1e-30));
    if(!okay&&err&&cap) snprintf(err,cap,"Smoke audit exceeds practical stock or finite-state bounds");
    return okay;
}
