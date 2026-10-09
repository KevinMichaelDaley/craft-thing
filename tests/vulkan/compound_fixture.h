#ifndef DC_TEST_COMPOUND_FIXTURE_H
#define DC_TEST_COMPOUND_FIXTURE_H
#include "dungeoncraft/gpu.h"
static inline dc_gpu_body_shape_t fixture_rectangle(uint32_t material, int x, int y, int w, int h) {
    return (dc_gpu_body_shape_t){.count=4,.material=material,
        .vertices={{x<<16,y<<16},{(x+w)<<16,y<<16},{(x+w)<<16,(y+h)<<16},{x<<16,(y+h)<<16}}};
}
static inline dc_gpu_compound_shape_t fixture_l(void) {
    dc_gpu_compound_shape_t s={.count=2};
    s.pieces[0]=fixture_rectangle(DC_GPU_BODY_STONE,0,0,4,16);
    s.pieces[1]=fixture_rectangle(DC_GPU_BODY_STONE,4,12,12,4);
    return s;
}
static inline dc_gpu_compound_shape_t fixture_u(void) {
    dc_gpu_compound_shape_t s={.count=3};
    s.pieces[0]=fixture_rectangle(DC_GPU_BODY_WOOD,0,0,4,16);
    s.pieces[1]=fixture_rectangle(DC_GPU_BODY_WOOD,12,0,4,16);
    s.pieces[2]=fixture_rectangle(DC_GPU_BODY_WOOD,4,12,8,4);
    return s;
}
static inline dc_gpu_world_body_t fixture_body(uint32_t id,int x,int y) {
    return (dc_gpu_world_body_t){.chunk={x/64,y/64},.body={.x_fp=(x%64)<<16,
        .y_fp=(y%64)<<16,.width=16,.height=16,.id=id,.active=1}};
}
#endif
