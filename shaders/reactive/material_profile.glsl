#ifndef FR_REACTIVE_MATERIAL_PROFILE_GLSL
#define FR_REACTIVE_MATERIAL_PROFILE_GLSL
#include "../../include/dungeoncraft/reactive_profile_layout.h"

/* The caller provides a validated readonly table accessor and material count.
 * Pigment/emissive buffers are deliberately absent from this physical contract. */
float reactive_coefficient(uint material,uint field) {
    return uintBitsToFloat(reactive_material_word(material,field));
}
float reactive_profile_rate(uint material,float temperature,float saturation,
                           float oxidation,float oxygen,float bound_oxidizer,vec2 oxygen_limits) {
    uint flags=reactive_material_word(material,FR_PROFILE_FLAGS_WORD);
    if((flags&FR_PROFILE_FLAG_FUEL)==0u||temperature<reactive_coefficient(material,FR_PROFILE_IGNITION_WORD)) return 0.;
    float demand=reactive_coefficient(material,FR_PROFILE_OXYGEN_DEMAND_WORD);
    float supply=(flags&FR_PROFILE_FLAG_BOUND_OXIDIZER)!=0u?
        (demand>0.?clamp(bound_oxidizer/demand,0.,1.):1.):
        clamp((oxygen-oxygen_limits.x)/(oxygen_limits.y-oxygen_limits.x),0.,1.);
    float dry=1.-saturation;
    float wet=(flags&FR_PROFILE_FLAG_WATER_DOUSEABLE)!=0u?
        dry*dry+saturation*reactive_coefficient(material,FR_PROFILE_WET_HAZARD_WORD):1.;
    float oxide=1.+oxidation*(reactive_coefficient(material,FR_PROFILE_OXIDIZED_HAZARD_WORD)-1.);
    return reactive_coefficient(material,FR_PROFILE_HAZARD_WORD)*wet*oxide*supply;
}
vec2 reactive_ignition_rates(uint substrate,uint coating,float temperature,float saturation,
                             float coverage,float substrate_oxide,float coating_oxide,
                             float shock,float oxygen,float bound_oxidizer,vec2 oxygen_limits) {
    if(substrate>=reactive_material_count()) return vec2(0);
    float capacity=reactive_coefficient(substrate,FR_PROFILE_HEAT_CAPACITY_WORD)+
        saturation*reactive_coefficient(substrate,FR_PROFILE_MOISTURE_CAPACITY_WORD)*
        reactive_coefficient(FR_PROFILE_WATER_MATERIAL,FR_PROFILE_HEAT_CAPACITY_WORD);
    temperature+=shock/capacity;
    vec2 rates=vec2(reactive_profile_rate(substrate,temperature,saturation,
        substrate_oxide,oxygen,bound_oxidizer,oxygen_limits),0.);
    if(coating<reactive_material_count()) {
        rates.y=coverage*reactive_profile_rate(coating,temperature,saturation,
            coating_oxide,oxygen,bound_oxidizer,oxygen_limits);
        if((reactive_material_word(coating,FR_PROFILE_FLAGS_WORD)&FR_PROFILE_FLAG_FUEL)!=0u)
            rates.x*=1.+coverage*max(0.,reactive_coefficient(coating,FR_PROFILE_OIL_HAZARD_WORD)-1.);
    }
    return rates;
}
#endif
