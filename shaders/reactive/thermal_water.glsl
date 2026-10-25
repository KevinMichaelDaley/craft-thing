#ifndef DC_THERMAL_WATER_GLSL
#define DC_THERMAL_WATER_GLSL
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#include "../../include/dungeoncraft/thermal_water_layout.h"
layout(set=0,binding=36,std430) readonly buffer ThermalControl {uint thermal_control[];};
layout(buffer_reference,std430,buffer_reference_align=4) coherent buffer WaterHeat {float excess[];};
bool thermal_water_on() {return thermal_control[0]==DC_THERMAL_WATER_MAGIC;}
WaterHeat thermal_water_field() {return WaterHeat(uvec2(thermal_control[2],thermal_control[3]));}
float thermal_water_at(uint index) {return thermal_water_on()?thermal_water_field().excess[index]:0.;}
void thermal_water_set(uint index,float value) {if(thermal_water_on()) thermal_water_field().excess[index]=value;}
void thermal_water_add(uint index,float value) {if(thermal_water_on()) thermal_water_field().excess[index]+=value;}
void thermal_water_move(uint source,uint target,uint flux,uint available) {
    if(thermal_water_on()&&flux!=0u&&available!=0u)
        thermal_water_field().excess[target]+=thermal_water_field().excess[source]*(float(flux)/float(available));
}
/* A fan-out snapshots source intensity until all recipient lanes finish.
 * The source owner withdraws once, with the authoritative mass update. */
void thermal_water_resize(uint index,uint before,uint after) {
    if(thermal_water_on()&&after<before)
        thermal_water_field().excess[index]*=float(after)/float(before);
}
#endif
