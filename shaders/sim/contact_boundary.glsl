#ifndef DC_CONTACT_BOUNDARY_GLSL
#define DC_CONTACT_BOUNDARY_GLSL
#ifndef DC_CONTACT_COOPERATIVE_BOUNDARY
#define DC_CONTACT_COOPERATIVE_BOUNDARY 0
#endif
/* Every active invocation must query the same body; pieces may differ. */
bool contact_pixel_boundary(Body body,uint piece,bool cooperative) {
    return pixel_boundary(body,piece);
}
#endif
