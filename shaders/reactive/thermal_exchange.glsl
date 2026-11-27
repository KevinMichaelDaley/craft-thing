#ifndef FR_THERMAL_EXCHANGE_GLSL
#define FR_THERMAL_EXCHANGE_GLSL
float thermal_face_exchange(vec4 a, vec4 b, float seconds, bool gameplay) {
    if(a.y<=0.||b.y<=0.||a.z<=0.||b.z<=0.) return 0.;
    float difference=a.x/a.y-b.x/b.y,conductance=2.*a.z*b.z/(a.z+b.z);
    if(gameplay) conductance*=float(FR_THERMAL_GAMEPLAY_FACE_METERS);
    float requested=conductance*difference*seconds;
    float bound=min(a.y,b.y)*abs(difference)/float(FR_THERMAL_NEIGHBORS);
    return sign(requested)*min(abs(requested),bound);
}
#endif
