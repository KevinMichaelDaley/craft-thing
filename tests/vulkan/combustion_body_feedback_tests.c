#define main body_feedback_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <sys/stat.h>
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "dungeoncraft/reactive_state_layout.h"
#include "../../src/vulkan/reactive_storage_gpu.h"

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
    dc_gpu_body_motion_t mass_before,mass_after;
    ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&mass_before,err,sizeof(err)));
    uint32_t *before=malloc(480u*270u*4u),*after=malloc(480u*270u*4u),*paused=malloc(480u*270u*4u);ASSERT_TRUE(before&&after&&paused);
    ASSERT_TRUE(capture_body_feedback(g,before,"before",err,sizeof(err)));
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.25f,err,sizeof(err)));
    fr_reactive_record_t dry,wet,inert;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&dry,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000002u,1,0,&inert,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000003u,1,0,&wet,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(dry.state.fuel_mass_fp<39322&&dry.state.char_mass_fp>0);
    ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&mass_after,err,sizeof(err)));
    ASSERT_TRUE(mass_after.mass<mass_before.mass);
    ASSERT_TRUE(mass_after.center_x>mass_before.center_x);
    ASSERT_TRUE(mass_after.inertia<mass_before.inertia);
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
static void hot_face_case(bool draft,bool diagonal,bool corner_oxygen) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[20u*64u+20u].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,450,4100,&state,err,sizeof(err)));
    state.ignition_threshold=.001f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,20u*64u+20u,&state,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.01f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(gas&&volume);
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=chunk->cells[i].material?0.:1.;if(volume[i]==0.) continue;
        gas[i].species[FR_GAS_OXYGEN]=.0023;gas[i].species[FR_GAS_NITROGEN]=.0077;
        gas[i].energy=(.0023*model.heat_capacity_cv[0]+.0077*model.heat_capacity_cv[1])*293.15;
    }
    uint32_t hot=(diagonal?19u:20u)*64u+19u;
    gas[hot].species[FR_GAS_OXYGEN]=corner_oxygen?.0023:.0005;
    gas[hot].species[FR_GAS_NITROGEN]=corner_oxygen?.0077:.0095;
    if(corner_oxygen) {
        const uint32_t faces[]={19u*64u+20u,20u*64u+19u,20u*64u+21u,21u*64u+20u};
        for(uint32_t i=0;i<4;i++) {
            gas[faces[i]].species[FR_GAS_OXYGEN]=0.;
            gas[faces[i]].species[FR_GAS_NITROGEN]=.01;
            gas[faces[i]].energy=.01*model.heat_capacity_cv[FR_GAS_NITROGEN]*293.15;
        }
    }
    if(draft) {
        gas[hot].momentum[1]=-.2;
        gas[hot].energy=(.0005*model.heat_capacity_cv[0]+.0095*model.heat_capacity_cv[1])*1800.+.5*model.kinetic_scale*.01*400.;
    } else {
        gas[hot].species[FR_GAS_NITROGEN]-=.0001;gas[hot].species[FR_GAS_SOOT]=.0001;
        gas[hot].energy=((corner_oxygen?.0023:.0005)*model.heat_capacity_cv[0]+(corner_oxygen?.0076:.0094)*model.heat_capacity_cv[1]+.0001*model.heat_capacity_cv[4])*1800.;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err))&&fr_combustion_gpu_step(g,.25f,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp<state.fuel_mass_fp&&(result.state.flags&FR_REACTIVE_FLAMING));
    free(gas);free(volume);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_hot_face_ignites_when_another_face_has_more_oxygen(void) {hot_face_case(true,false,false);}
static void test_radiating_flame_heats_a_surface_even_without_a_draft(void) {hot_face_case(false,false,false);}
static void test_diagonal_hot_gas_heats_a_body_corner(void) {hot_face_case(false,true,false);}
static void test_diagonal_oxygen_reaches_an_exposed_body_corner(void) {hot_face_case(false,true,true);}
static void hot_skin_budget_case(bool rotated) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t cell=20u*64u+20u;chunk->cells[cell].material=DC_MATERIAL_STONE;
    if(rotated) chunk->cells[cell+1u].material=DC_MATERIAL_STONE;
    fr_reactive_state_t stone;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,rotated?1.f:300.f,6100,&stone,err,sizeof(err)));
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,0,cell,&stone,err,sizeof(err)));
    if(rotated) {
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,1000,6101,&stone,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,0,cell+1u,&stone,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_mark_bonfire_world(g));
    if(rotated) ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,.78539816f,0,0,err,sizeof(err))&&dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_step(g,.001f,err,sizeof(err)));
    fr_reactive_record_t before,after;bool found=false;ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&before,&found,err,sizeof(err))&&found);
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(gas&&volume);
    for(uint32_t i=0;i<4096;i++) volume[i]=i==cell?0.:1.;
    const uint32_t neighbors[4]={cell-1u,cell+1u,cell-64u,cell+64u};double initial=fr_reactive_state_energy(&before.state);
    if(rotated) {fr_reactive_record_t other;ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,0,&other,&found,err,sizeof(err))&&found);initial+=fr_reactive_state_energy(&other.state);}
    for(uint32_t k=0;k<4;k++) {
        fr_gas_state_t *air=gas+neighbors[k];air->species[0]=.23;air->species[1]=.77;air->momentum[0]=20.;
        air->energy=(.23*model.heat_capacity_cv[0]+.77*model.heat_capacity_cv[1])*293.15+.5*model.kinetic_scale*400.;initial+=air->energy;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err)));
    const uint32_t *entry=g->reactive->arena.entries.mapped;
    uint32_t skin[4]={entry[FR_ENTRY_GENERATION],0,0,0};float hot=100000.f;memcpy(skin+1,&hot,4);
    ASSERT_EQ(vkResetCommandBuffer(g->command,0),VK_SUCCESS);
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};ASSERT_EQ(vkBeginCommandBuffer(g->command,&begin),VK_SUCCESS);
    vkCmdUpdateBuffer(g->command,g->reactive->arena.thermal_scratch.buffer,32,sizeof(skin),skin);
    if(rotated) {skin[0]=entry[FR_ENTRY_WORDS+FR_ENTRY_GENERATION];vkCmdUpdateBuffer(g->command,g->reactive->arena.thermal_scratch.buffer,80,sizeof(skin),skin);}
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    ASSERT_EQ(vkEndCommandBuffer(g->command),VK_SUCCESS);VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    ASSERT_EQ(vkQueueSubmit(g->queue,1,&submit,VK_NULL_HANDLE),VK_SUCCESS);ASSERT_EQ(vkQueueWaitIdle(g->queue),VK_SUCCESS);
    ASSERT_TRUE(fr_thermal_gpu_step(g,.25f,err,sizeof(err))&&fr_gpu_reactive_read_body(g,0x80000001u,0,0,&after,&found,err,sizeof(err))&&found&&fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    double final=fr_reactive_state_energy(&after.state);
    if(rotated) {fr_reactive_record_t other;ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,0,&other,&found,err,sizeof(err))&&found);ASSERT_TRUE(fr_reactive_state_energy(&other.state)>=0.);final+=fr_reactive_state_energy(&other.state);}
    for(uint32_t i=0;i<4096;i++) final+=gas[i].energy;
    printf("Finite skin exchange body energy%.6fJ total drift%.9gJ\n",fr_reactive_state_energy(&after.state),final-initial);
    ASSERT_TRUE(fr_reactive_state_energy(&after.state)>=0.&&fabs(final-initial)<.02);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(gas);free(volume);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_thin_oxygen_is_reserved_without_creating_heat_or_consuming_unfunded_fuel(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t cell=20u*64u+20u;chunk->cells[cell].material=DC_MATERIAL_WOOD;
    fr_reactive_state_t wood;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,1000,9100,&wood,err,sizeof(err)));wood.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,0,cell,&wood,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(gas&&volume);
    for(uint32_t i=0;i<4096;i++) volume[i]=i==cell?0.:1.;
    const uint32_t neighbors[4]={cell-1u,cell+1u,cell-64u,cell+64u};double initial=fr_reactive_state_energy(&wood),oxygen=0.;
    for(uint32_t k=0;k<4;k++) {
        fr_gas_state_t *air=gas+neighbors[k];air->species[0]=.1/65536.;air->species[1]=.000006;
        air->energy=(air->species[0]*model.heat_capacity_cv[0]+air->species[1]*model.heat_capacity_cv[1])*293.15;
        initial+=air->energy;oxygen+=air->species[0]*65536.;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err)));
    for(uint32_t step=0;step<8;step++) ASSERT_TRUE(fr_combustion_gpu_step(g,.01f,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&result,&found,err,sizeof(err))&&found&&fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    double remaining=0.,final=fr_reactive_state_energy(&result.state);
    for(uint32_t i=0;i<4096;i++) {remaining+=gas[i].species[0]*65536.;final+=gas[i].energy;}
    double reserved=fr_reactive_state_pending_oxygen_fp(&result.state);
    printf("Thin oxygen gas%.9g pending%.9g initial%.9g; energy drift%.9gJ\n",remaining,reserved,oxygen,final-initial);
    ASSERT_TRUE(reserved>.3&&fabs(remaining+reserved-oxygen)<.00001&&result.state.fuel_mass_fp==wood.fuel_mass_fp&&fabs(final-initial)<.001);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(gas);free(volume);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_hot_skin_cannot_overdraw_the_actual_body_heat_inventory(void) {hot_skin_budget_case(false);}
static void test_rotated_mixed_temperature_heat_cannot_overdraw_a_cold_parcel(void) {hot_skin_budget_case(true);}
static void test_reacting_wood_does_not_overheat_thin_adjacent_air(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));
    double *volumes=calloc(4096,sizeof(*volumes));
    ASSERT_TRUE(chunk&&gas&&volumes);
    const uint32_t cell=20u*64u+20u;
    chunk->cells[cell].material=DC_MATERIAL_WOOD;
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,1000.f,
        9200,&wood,err,sizeof(err)));
    wood.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,cell,&wood,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err)));
    const fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},
        .kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) volumes[i]=i==cell?0.:1.;
    const uint32_t neighbors[4]={cell-1u,cell+1u,cell-64u,cell+64u};
    for(uint32_t k=0;k<4;k++) {
        fr_gas_state_t *air=gas+neighbors[k];
        air->species[FR_GAS_OXYGEN]=.000021;
        air->species[FR_GAS_NITROGEN]=.000079;
        air->energy=(.000021*model.heat_capacity_cv[FR_GAS_OXYGEN]+
            .000079*model.heat_capacity_cv[FR_GAS_NITROGEN])*1700.;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volumes,4096,err,sizeof(err)));
    for(uint32_t step=0;step<4;step++)
        ASSERT_TRUE(fr_combustion_gpu_step(g,.25f,err,sizeof(err)));
    fr_reactive_record_t after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&after,&found,
        err,sizeof(err))&&found&&
        fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    double peak=0.;
    for(uint32_t k=0;k<4;k++) {
        fr_gas_thermo_t thermo;
        ASSERT_TRUE(fr_gas_thermodynamics(&model,gas+neighbors[k],1.,
            &thermo,err,sizeof(err)));
        if(thermo.temperature_K>peak) peak=thermo.temperature_K;
    }
    printf("Thin-air reaction consumed%u peak%.1fK\n",
        wood.fuel_mass_fp-after.state.fuel_mass_fp,peak);
    ASSERT_TRUE(after.state.fuel_mass_fp<wood.fuel_mass_fp);
    ASSERT_TRUE(peak<=2300.);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    free(volumes);free(gas);free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_reacting_wood_does_not_overheat_thin_adjacent_air);RUN(test_thin_oxygen_is_reserved_without_creating_heat_or_consuming_unfunded_fuel);RUN(test_rotated_mixed_temperature_heat_cannot_overdraw_a_cold_parcel);RUN(test_hot_skin_cannot_overdraw_the_actual_body_heat_inventory);RUN(test_radiating_flame_heats_a_surface_even_without_a_draft);RUN(test_hot_face_ignites_when_another_face_has_more_oxygen);RUN(test_diagonal_hot_gas_heats_a_body_corner);RUN(test_diagonal_oxygen_reaches_an_exposed_body_corner);RUN(test_body_surface_feedback_uses_real_reaction_and_preserves_unburnt_authored_grain);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
