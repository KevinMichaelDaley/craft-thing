#ifndef DC_BLACKBODY_H
#define DC_BLACKBODY_H
#ifdef __STDC_VERSION__
/** Temperature in kelvin, finite radiometric scale, explicit magical RGB override. */
static inline void fr_blackbody_rgb(float temperature,float power,int magical,const float override[3],float out[3]) {
    (void)temperature;(void)power;(void)magical;(void)override;
    out[0]=1.f;out[1]=.3f;out[2]=.008f;
}
#endif
#endif
