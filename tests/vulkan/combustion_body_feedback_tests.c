#define main body_feedback_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <sys/stat.h>
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"

static bool capture_body_feedback(dc_gpu_t *g,uint32_t *pixels,const char *name,char *err,uint32_t cap) {
    if(!dc_gpu_present_chunks(g,err,cap)||!dc_gpu_readback(g,pixels,480u*270u,err,cap)) return false;
    mkdir("build/screenshots",0777);char path[160];snprintf(path,sizeof(path),"build/screenshots/body_surface_%s.bmp",name);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    bool okay=image&&SDL_SaveBMP(image,path)==0;SDL_FreeSurface(image);return okay;
}
static void test_body_surface_feedback_uses_real_reaction_and_preserves_unburnt_authored_grain(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(chunk&&colors);
    uint64_t parcel=3000;
    for(uint32_t object=0;object<3;object++) for(uint32_t y=20;y<22;y++) for(uint32_t x=20+object*10;x<23+object*10;x++) {
        uint32_t i=y*64u+x;chunk->cells[i].material=DC_MATERIAL_WOOD;
        colors->terrain[i][0]=.32f+.04f*(x%3u);colors->terrain[i][1]=.16f+.02f*(x%3u);colors->terrain[i][2]=.08f;colors->terrain[i][3]=1;
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_renderer_enable(g,err,sizeof(err))&&fr_renderer_upload_chunk_colors(g,0,colors,err,sizeof(err))&&fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    for(uint32_t object=0;object<3;object++) for(uint32_t y=20;y<22;y++) for(uint32_t x=20+object*10;x<23+object*10;x++) {
        bool burning_patch=object==0&&x==20u&&y==20u;
        fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,object==2?19661u:0u,
            object==0&&!burning_patch?293.15f:1000.f,parcel++,&state,err,sizeof(err)));if(burning_patch) state.flags|=FR_REACTIVE_FLAMING;
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,y*64u+x,&state,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_overlay(g,0));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.01f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));double *volumes=calloc(4096,sizeof(*volumes));ASSERT_TRUE(gas&&volumes);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) {
        uint32_t x=i%64u,y=i/64u;volumes[i]=chunk->cells[i].material?0:1;
        if(x<27u||x>36u||y<17u||y>24u||volumes[i]==0) continue;
        gas[i]=(fr_gas_state_t){0};gas[i].species[FR_GAS_NITROGEN]=.01;
        gas[i].energy=.01*model.heat_capacity_cv[FR_GAS_NITROGEN]*1100;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volumes,4096,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_presentation_focus(g,(dc_presentation_focus_t){16,14,32,20}));
    uint32_t *before=malloc(480u*270u*4u),*after=malloc(480u*270u*4u),*paused=malloc(480u*270u*4u);ASSERT_TRUE(before&&after&&paused);
    ASSERT_TRUE(capture_body_feedback(g,before,"before",err,sizeof(err)));
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.25f,err,sizeof(err)));
    fr_reactive_record_t dry,wet,inert;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&dry,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000002u,1,0,&inert,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000003u,1,0,&wet,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(dry.state.fuel_mass_fp<39322&&dry.state.char_mass_fp>0);
    ASSERT_TRUE(wet.state.fuel_mass_fp==39322&&wet.state.char_mass_fp==0&&inert.state.fuel_mass_fp==39322&&inert.state.char_mass_fp==0);
    ASSERT_TRUE(capture_body_feedback(g,after,"after",err,sizeof(err)));
    uint32_t changed=0;for(uint32_t y=20;y<22;y++) for(uint32_t x=20;x<23;x++) changed+=before[y*480u+x]!=after[y*480u+x];
    printf("BODY surface: dry reacted%u char%u changed%u/6; inert/wet fuel unchanged\n",39322-dry.state.fuel_mass_fp,dry.state.char_mass_fp,changed);
    ASSERT_TRUE(changed>0&&changed<6);
    ASSERT_EQ(before[21*480+22],after[21*480+22]);
    for(uint32_t y=20;y<22;y++) for(uint32_t x=30;x<33;x++) ASSERT_EQ(before[y*480u+x],after[y*480u+x]);
    for(uint32_t y=20;y<22;y++) for(uint32_t x=40;x<43;x++) ASSERT_EQ(before[y*480u+x],after[y*480u+x]);
    ASSERT_TRUE(before[20*480+20]!=before[20*480+21]);
    ASSERT_TRUE(capture_body_feedback(g,paused,"paused",err,sizeof(err))&&memcmp(after,paused,480u*270u*4u)==0);
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,.4f,0,0,err,sizeof(err))&&dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(capture_body_feedback(g,paused,"rotated",err,sizeof(err)));
    fr_reactive_record_t moved;ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&moved,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&moved.state,&dry.state,sizeof(dry.state)),0);
    free(before);free(after);free(paused);free(gas);free(volumes);free(colors);free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_body_surface_feedback_uses_real_reaction_and_preserves_unburnt_authored_grain);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
