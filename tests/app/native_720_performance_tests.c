#define _POSIX_C_SOURCE 200809L
#define main pixel_component_fixture_main
#include "../vulkan/pixel_component_tests.c"
#undef main

#include <SDL.h>
#include <time.h>
#include "../vulkan/native_scene.h"

enum { WIDTH=1280, HEIGHT=720, SAMPLES=30, PHYSICS_STEPS=2, FLUID_CADENCE=6 };
static double seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1e9;
}
static int compare_sample(const void *a, const void *b) {
    double x=*(const double *)a,y=*(const double *)b;
    return (x>y)-(x<y);
}
static void native_scene(unsigned load) {
    char err[256]={0};dc_gpu_t *g=NULL;
    double began=seconds();
    ASSERT_TRUE(dc_gpu_create_window(&g,WIDTH,HEIGHT,WIDTH,HEIGHT,
                                     "build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_EQ(g->width,WIDTH);ASSERT_EQ(g->height,HEIGHT);
    ASSERT_EQ(g->swap_extent.width,WIDTH);ASSERT_EQ(g->swap_extent.height,HEIGHT);
    ASSERT_TRUE(dc_gpu_set_viewport(g,0,0,WIDTH,HEIGHT));
    ASSERT_TRUE(dc_gpu_set_display_zoom(g,1));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(g,FLUID_CADENCE));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(g,1.f/60.f));
    ASSERT_TRUE(dc_gpu_set_window_title(g,"Native720 benchmark: 1280x720 physics and display, scale1"));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned slot=0;slot<240;slot++) {
        native_scene_chunk(chunk,slot,WIDTH,HEIGHT);
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            unsigned px=(unsigned)chunk->coord.x*64+x,py=(unsigned)chunk->coord.y*64+y;
            dc_cell_t *cell=&chunk->cells[y*64+x];
            if(load==0) {cell->fluid_mass=0;if(cell->material==DC_MATERIAL_SAND) cell->material=DC_MATERIAL_AIR;}
            if(load==2) {
                if(px>=520&&px<600&&py>=120&&py<184&&(px<526||px>=594||py>=178)) cell->material=DC_MATERIAL_WOOD;
                if(px>=538&&px<610&&py>=215&&py<263&&(px<544||(py-215)%6<3)) cell->material=px<550?DC_MATERIAL_STONE:DC_MATERIAL_DENSE_WOOD;
            }
        }
        dc_chunk_seed_particles(chunk);
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,slot%20,slot/20,slot,err,sizeof(err)));
    }
    free(chunk);
    if(load==2) ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    double loaded=seconds();
    ASSERT_TRUE(dc_gpu_present_chunks_steps(g,PHYSICS_STEPS,err,sizeof(err)));
    ASSERT_EQ(vkQueueWaitIdle(g->queue),VK_SUCCESS);
    double cold_ms=(seconds()-loaded)*1000.;
    if(load==2) {
        uint32_t a=0,b=0;
        bool exact=dc_gpu_read_pixel_shape(g,0x80000001u,NULL,0,&a,err,sizeof(err))&&
                   dc_gpu_read_pixel_shape(g,0x80000002u,NULL,0,&b,err,sizeof(err))&&a==1176u&&b==1872u;
        if(!exact) {printf("native complex birth unavailable: %s\n",err);dc_gpu_destroy(g);ASSERT_TRUE(exact);}
        ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,0,.005f,0,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000002u,0,-.003f,0,err,sizeof(err)));
    }
    for(unsigned i=0;i<8;i++) ASSERT_TRUE(dc_gpu_present_chunks_steps(g,PHYSICS_STEPS,err,sizeof(err)));
    double samples[SAMPLES],start=seconds();
    for(unsigned i=0;i<SAMPLES;i++) {
        SDL_Event event;while(SDL_PollEvent(&event)) ASSERT_TRUE(event.type!=SDL_QUIT);
        double tick=seconds();ASSERT_TRUE(dc_gpu_present_chunks_steps(g,PHYSICS_STEPS,err,sizeof(err)));
        samples[i]=(seconds()-tick)*1000.;
    }
    ASSERT_EQ(vkQueueWaitIdle(g->queue),VK_SUCCESS);
    double wall_ms=(seconds()-start)*1000./SAMPLES;
    qsort(samples,SAMPLES,sizeof(*samples),compare_sample);
    uint64_t stage_ns[3]={0};
    for(unsigned i=0;i<6;i++) {
        dc_gpu_tick_capture_t capture;ASSERT_TRUE(dc_gpu_tick_capture(g,&capture,err,sizeof(err)));
        for(unsigned s=0;s<3;s++) stage_ns[s]+=capture.stages[s].gpu_ns;
    }
    VkPhysicalDeviceProperties device;vkGetPhysicalDeviceProperties(g->physical,&device);
    printf("native720 load%u: physics=%ux%u display=%ux%u scale1 slots%u cadence%u,2fixed60Hzsteps/frame device=%s\n",
           load,g->width,g->height,g->swap_extent.width,g->swap_extent.height,DC_GPU_CHUNK_SLOTS,FLUID_CADENCE,device.deviceName);
    printf("setup %.3fs cold %.3fms frame mean %.3fms p95 %.3fms max %.3fms; offscreen1tick GPU rigid %.3f fluid %.3f MPM %.3fms\n",
           loaded-began,cold_ms,wall_ms,samples[(SAMPLES*95+99)/100-1],samples[SAMPLES-1],stage_ns[0]/6e6,stage_ns[1]/6e6,stage_ns[2]/6e6);
    dc_gpu_destroy(g);
    ASSERT_TRUE(wall_ms<=1000./30.);
    ASSERT_TRUE(samples[(SAMPLES*95+99)/100-1]<=1000./30.);
    PASS();
}
static void test_native_dry_presented_frame_meets_30fps(void) {native_scene(0);}
static void test_native_water_and_grains_presented_frame_meets_30fps(void) {native_scene(1);}
static void test_native_exact_concave_bodies_presented_frame_meets_30fps(void) {native_scene(2);}
int main(void) {
    RUN(test_native_dry_presented_frame_meets_30fps);
    RUN(test_native_water_and_grains_presented_frame_meets_30fps);
    RUN(test_native_exact_concave_bodies_presented_frame_meets_30fps);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
