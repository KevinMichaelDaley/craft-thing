#define main solver_type_original_pixel_main
#include "pixel_component_tests.c"
#undef main

static dc_gpu_t *type_grid(void) {
    dc_gpu_t *gpu=NULL;char err[256];
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));if(!chunk) return NULL;
    for(unsigned y=10;y<14;y++) for(unsigned x=10;x<18;x++)
        chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
    bool okay=dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err));
    free(chunk);
    if(!okay) {printf("%s\n",err);dc_gpu_destroy(gpu);return NULL;}
    return gpu;
}
static void test_pixel_only_solver_pipelines_are_available_without_legacy_spawn(void) {
    dc_gpu_t *gpu=type_grid();ASSERT_TRUE(gpu);bool ready=true;
    for(unsigned phase=0;phase<DC_GPU_SOLVER_PHASE_COUNT;phase++)
        ready=ready&&gpu->solver_pixel_pipelines[phase]!=VK_NULL_HANDLE;
    bool pixel_only=!gpu->legacy_body_possible;dc_gpu_destroy(gpu);
    ASSERT_TRUE(ready);ASSERT_TRUE(pixel_only);PASS();
}
static void test_explicit_box_spawn_marks_general_solver_required(void) {
    dc_gpu_t *gpu=type_grid();ASSERT_TRUE(gpu);char err[256];
    dc_gpu_body_t box={.x_fp=30<<16,.y_fp=20<<16,.width=2,.height=3,.id=77,.active=1};
    bool spawned=dc_gpu_spawn_body(gpu,box,err,sizeof(err));
    bool general=gpu->legacy_body_possible;dc_gpu_destroy(gpu);
    ASSERT_TRUE(spawned);ASSERT_TRUE(general);PASS();
}
static void test_checkpoint_reclassifies_pixel_and_mixed_bodies_exactly(void) {
    dc_gpu_t *gpu=type_grid();ASSERT_TRUE(gpu);char err[256];
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu,true));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t pixel;ASSERT_TRUE(dc_gpu_read_body_id(gpu,0x80000001u,&pixel,err,sizeof(err)));
    ASSERT_TRUE(!gpu->legacy_body_possible&&pixel.vy_fp>0);
    const char *pixel_path="build/solver_type_pixel.dcs",*mixed_path="build/solver_type_mixed.dcs";
    ASSERT_TRUE(fr_gpu_save_structure(gpu,pixel_path,err,sizeof(err)));
    dc_gpu_body_t box={.x_fp=30<<16,.y_fp=20<<16,.width=2,.height=3,.id=77,.active=1};
    ASSERT_TRUE(dc_gpu_spawn_body(gpu,box,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(gpu,mixed_path,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(gpu,pixel_path,err,sizeof(err)));
    ASSERT_TRUE(!gpu->legacy_body_possible);
    dc_gpu_body_t restored;ASSERT_TRUE(dc_gpu_read_body_id(gpu,pixel.id,&restored,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&pixel,&restored,sizeof(pixel))==0);
    ASSERT_TRUE(fr_gpu_load_structure(gpu,mixed_path,err,sizeof(err)));
    bool general=gpu->legacy_body_possible;
    bool box_present=dc_gpu_read_body_id(gpu,77,&restored,err,sizeof(err));
    dc_gpu_destroy(gpu);remove(pixel_path);remove(mixed_path);
    ASSERT_TRUE(general);ASSERT_TRUE(box_present);PASS();
}
int main(void) {
    RUN(test_pixel_only_solver_pipelines_are_available_without_legacy_spawn);
    RUN(test_explicit_box_spawn_marks_general_solver_required);
    RUN(test_checkpoint_reclassifies_pixel_and_mixed_bodies_exactly);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
