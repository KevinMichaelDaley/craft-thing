#include "acoustic.glsl"
struct CoarseRow {vec4 coefficients;vec4 data;};
layout(set=0,binding=24,std430) restrict buffer CoarseWork {CoarseRow coarse[];};
uint coarse_width() {return (params.width+7u)/8u;}
