#ifndef DC_WATER_COLOR_GLSL
#define DC_WATER_COLOR_GLSL
layout(constant_id=2) const bool WATER_COLOR_ENABLED=false;
layout(set=0,binding=32,std430) coherent buffer WaterColors {vec4 water_colors[];};
const vec3 DEFAULT_WATER_COLOR=vec3(.035,.193,.666);
vec3 water_color_at(uint index) {
    return water_colors[index].w>0.?water_colors[index].rgb:DEFAULT_WATER_COLOR;
}
void water_color_move(uint source,uint target,uint receiving,uint flux) {
    if(!WATER_COLOR_ENABLED||flux==0u) return;
    vec3 color=(water_color_at(target)*float(receiving)+water_color_at(source)*float(flux))/
        (float(receiving)+float(flux));
    water_colors[target]=vec4(color,1.);
}
void water_color_clear(uint index,uint remaining) {
    if(WATER_COLOR_ENABLED&&remaining==0u) water_colors[index]=vec4(0);
}
#endif
