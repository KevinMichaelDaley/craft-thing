#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "compound_fixture.h"
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/runtime_body_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/structural/gpu.h"

static int g_pass,g_fail;
static bool reject_allocations;
static bool count_submits;
static unsigned submissions;
VKAPI_ATTR VkResult VKAPI_CALL __real_vkQueueSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo *info,VkFence fence);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo *info,VkFence fence) {
    if(count_submits) submissions++;
    return __real_vkQueueSubmit(queue,count,info,fence);
}
VKAPI_ATTR VkResult VKAPI_CALL __real_vkAllocateMemory(VkDevice device,const VkMemoryAllocateInfo *info,const VkAllocationCallbacks *callbacks,VkDeviceMemory *memory);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkAllocateMemory(VkDevice device,const VkMemoryAllocateInfo *info,const VkAllocationCallbacks *callbacks,VkDeviceMemory *memory) {
    if(reject_allocations) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    return __real_vkAllocateMemory(device,info,callbacks,memory);
}
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_gpu_water_source_releases_newly_isolated_terrain(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=32;y<64;y++) for(unsigned x=0;x<64;x++) {
        chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
        chunk->cells[y*64+x].flags=DC_CELL_LOADED_TERRAIN;
    }
    for(unsigned y=10;y<32;y++) {
        chunk->cells[y*64+20].material=DC_MATERIAL_WOOD;
        chunk->cells[y*64+20].flags=DC_CELL_LOADED_TERRAIN;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    for(unsigned i=0;i<2;i++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_tick_water_source(g,true,20,25));
    ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    unsigned count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,NULL,0,&count,err,sizeof(err)));
    ASSERT_EQ(count,13u);
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_cut_and_physics_tick_share_the_occupancy_refresh(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_world_body_t b={.body={.id=1,.active=1,.width=2,.height=2,.x_fp=20<<16,.y_fp=20<<16}};
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,b,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    uint64_t revision=g->descriptor_revision;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(g->descriptor_revision>revision);
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    uint32_t pixels[64*64];ASSERT_TRUE(dc_gpu_readback(g,pixels,64*64,err,sizeof(err)));
    ASSERT_TRUE(pixels[20*64+21]!=pixels[63*64+63]);
    ASSERT_TRUE(g->body_refresh_pending);
    submissions=0;count_submits=true;
    bool prepared=dc_gpu_prepare_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err));
    count_submits=false;ASSERT_TRUE(prepared);ASSERT_EQ(submissions,0u);
    ASSERT_TRUE(g->body_refresh_pending);
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(!g->body_refresh_pending);
    unsigned occupied;
    ASSERT_TRUE(dc_gpu_read_occupancy(g,20,20,&occupied,NULL,err,sizeof(err)));ASSERT_EQ(occupied,0u);
    ASSERT_TRUE(dc_gpu_read_occupancy(g,21,20,&occupied,NULL,err,sizeof(err)));ASSERT_EQ(occupied,1u);
    submissions=0;count_submits=true;
    prepared=dc_gpu_prepare_body_origin(g,(dc_chunk_coord_t){1,0},err,sizeof(err));
    count_submits=false;ASSERT_TRUE(prepared);ASSERT_EQ(submissions,0u);
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_occupancy(g,21,20,&occupied,NULL,err,sizeof(err)));ASSERT_EQ(occupied,0u);
    ASSERT_TRUE(dc_gpu_prepare_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,1,&b,err,sizeof(err)));
    ASSERT_EQ(b.chunk.x,0);ASSERT_EQ(b.chunk.y,0);ASSERT_EQ(b.body.x_fp,20<<16);
    ASSERT_TRUE(b.body.y_fp>20<<16);
    ASSERT_TRUE(dc_gpu_read_occupancy(g,21,(unsigned)(b.body.y_fp>>16)+1u,&occupied,NULL,err,sizeof(err)));
    ASSERT_EQ(occupied,1u);
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_brush_invalidates_only_the_state_it_changes(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_world_body_t b={.body={.id=1,.active=1,.width=1,.height=1,.x_fp=4<<16,.y_fp=4<<16}};
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,b,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]=0;
    ASSERT_TRUE(!g->body_refresh_pending);
    ASSERT_TRUE(dc_gpu_paint_material(g,63,63,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(!g->body_refresh_pending);
    ASSERT_EQ(dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD],0u);
    ASSERT_TRUE(dc_gpu_paint_material(g,15,15,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(!g->body_refresh_pending);
    ASSERT_EQ(dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD],1u);
    ASSERT_TRUE(dc_gpu_paint_material(g,4,4,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(g->body_refresh_pending);
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_missed_brush_does_not_serialize_all_body_slots(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned id=1;id<=320;id++) {
        dc_gpu_world_body_t b={.body={.id=id,.active=1,.width=1,.height=1}};
        ASSERT_TRUE(dc_gpu_spawn_world_body(g,b,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_body_capacity(g)>=320u);
    ASSERT_TRUE(dc_gpu_paint_material(g,63,63,0,DC_MATERIAL_AIR,err,sizeof(err)));
    struct timespec before,after;clock_gettime(CLOCK_MONOTONIC,&before);
    submissions=0;count_submits=true;
    ASSERT_TRUE(dc_gpu_paint_material(g,63,63,0,DC_MATERIAL_AIR,err,sizeof(err)));
    count_submits=false;
    clock_gettime(CLOCK_MONOTONIC,&after);
    double ms=(after.tv_sec-before.tv_sec)*1000.+(after.tv_nsec-before.tv_nsec)/1e6;
    for(unsigned id=1;id<=320;id++) {
        dc_gpu_body_t b;ASSERT_TRUE(dc_gpu_read_body_id(g,id,&b,err,sizeof(err)));
        ASSERT_TRUE(b.active);ASSERT_EQ(b.x_fp,0);ASSERT_EQ(b.y_fp,0);
    }
    printf("Missed air brush with 320 bodies: %.3f ms\n",ms);
    printf("Warm missed brush GPU submissions: %u\n",submissions);
    dc_gpu_destroy(g);ASSERT_EQ(submissions,1u);ASSERT_TRUE(ms<3.);PASS();
}

static void test_large_body_brush_preserves_mass_with_bounded_latency(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,192,192,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned cy=0;cy<3;cy++) for(unsigned cx=0;cx<3;cx++) {
        memset(chunk,0,sizeof(*chunk));chunk->coord=(dc_chunk_coord_t){cx,cy};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            unsigned wx=cx*64+x,wy=cy*64+y;
            if(wx>0&&wx<191&&wy>0&&wy<191) chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,cy*3+cx,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,cx,cy,cy*3+cx,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    for(unsigned i=0;i<4;i++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_runtime_prepare_fragments(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,0,0,0,DC_MATERIAL_AIR,err,sizeof(err)));
    struct timespec before,after;clock_gettime(CLOCK_MONOTONIC,&before);
    ASSERT_TRUE(dc_gpu_paint_material(g,96,96,4,DC_MATERIAL_AIR,err,sizeof(err)));
    clock_gettime(CLOCK_MONOTONIC,&after);
    double ms=(after.tv_sec-before.tv_sec)*1000.+(after.tv_nsec-before.tv_nsec)/1e6;
    unsigned removed=0;for(int y=-4;y<=4;y++) for(int x=-4;x<=4;x++) removed+=x*x+y*y<=16;
    dc_gpu_body_pixel_t *pixels=calloc(190u*190u,sizeof(*pixels));unsigned count=0;ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,190u*190u,&count,err,sizeof(err)));
    printf("Large body brush output: %u pixels, expected %u, %.3f ms\n",count,190u*190u-removed,ms);
    ASSERT_EQ(count,190u*190u-removed);
    dc_gpu_body_motion_t motion;ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&motion,err,sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass-count*.6f)<.01f);
    printf("Large body air brush: %.3f ms, %u remaining pixels\n",ms,count);
    dc_gpu_destroy(g);free(chunk);free(pixels);
    ASSERT_TRUE(ms<30.);
    PASS();
}

static void test_air_carves_colored_body_and_splits_remaining_components(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(c&&colors);
    for(unsigned y=20;y<23;y++) for(unsigned x=20;x<29;x++) {
        unsigned i=y*64+x;c->cells[i].material=DC_MATERIAL_WOOD;
        colors->terrain[i][0]=x/64.f;colors->terrain[i][1]=.25f;colors->terrain[i][3]=1;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,0,colors,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_record_t *b=g->body_mapped;
    b->body.x_fp=b->local_x_fp=20<<16;b->body.y_fp=b->local_y_fp=20<<16;
    b->body.vx_fp=1<<16;b->body.vy_fp=0;b->angular_velocity=0;
    ASSERT_TRUE(dc_gpu_runtime_prepare_fragments(g,err,sizeof(err)));
    for(unsigned slot=1;slot<DC_GPU_BODY_CAPACITY;slot++)
        ((dc_gpu_body_record_t*)g->body_mapped)[slot].reserved[0]=0x53545232u;
    reject_allocations=true;
    bool accepted=dc_gpu_paint_material(g,24,21,1,DC_MATERIAL_AIR,err,sizeof(err));
    reject_allocations=false;ASSERT_TRUE(!accepted);
    dc_gpu_body_pixel_t unchanged[27];unsigned unchanged_count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,b->body.id,unchanged,27,&unchanged_count,err,sizeof(err)));
    ASSERT_EQ(unchanged_count,27u);
    for(unsigned slot=1;slot<DC_GPU_BODY_CAPACITY;slot++)
        ((dc_gpu_body_record_t*)g->body_mapped)[slot].reserved[0]=0u;
    ASSERT_TRUE(dc_gpu_paint_material(g,24,21,1,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    unsigned total=0,live=0;float mass=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        b=(dc_gpu_body_record_t*)g->body_mapped+slot;if(!b->body.active) continue;
        live++;dc_gpu_body_pixel_t pixels[27];unsigned n=0;dc_gpu_body_motion_t motion;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g,b->body.id,pixels,27,&n,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,b->body.id,&motion,err,sizeof(err)));
        ASSERT_EQ(b->body.vx_fp,1<<16);ASSERT_EQ(b->body.vy_fp,0);
        const uint32_t *shape=(uint32_t*)g->body_mapped+b->compound_reserved;
        for(unsigned i=0;i<n;i++) {
            unsigned cell=(pixels[i].y%16)*16+pixels[i].x%16;
            unsigned tile=(pixels[i].y/16)*shape[2]+pixels[i].x/16;
            unsigned word=b->compound_reserved+shape[8+tile]+1+cell/8;
            const float *rgb=dc_gpu_shape_colors(g,word)+(cell%8)*4;
            ASSERT_TRUE(fabsf(rgb[0]-(20+pixels[i].x)/64.f)<1e-6f);
            ASSERT_EQ(rgb[1],.25f);ASSERT_EQ(rgb[3],1.f);
        }
        total+=n;mass+=motion.mass;
    }
    ASSERT_EQ(live,2u);ASSERT_EQ(total,22u);ASSERT_TRUE(fabsf(mass-13.2f)<.001f);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/carved_body.dcs",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,24,21,20,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++)
        ASSERT_TRUE(!((dc_gpu_body_record_t*)g->body_mapped)[slot].body.active);
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/carved_body.dcs",err,sizeof(err)));
    total=0;for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++)
        total+=((dc_gpu_body_record_t*)g->body_mapped)[slot].body.active!=0;
    ASSERT_EQ(total,2u);dc_gpu_destroy(g);free(c);free(colors);PASS();
}
static void test_air_carves_compound_wall_without_filling_its_cavity(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_compound_shape_t shape=fixture_u();
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(7,20,20),&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,21,24,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    uint32_t id;
    ASSERT_TRUE(dc_gpu_read_occupancy(g,21,24,&id,NULL,err,sizeof(err)));ASSERT_EQ(id,0u);
    ASSERT_TRUE(dc_gpu_read_occupancy(g,28,24,&id,NULL,err,sizeof(err)));ASSERT_EQ(id,0u);
    dc_gpu_body_motion_t motion;ASSERT_TRUE(dc_gpu_read_body_motion(g,7,&motion,err,sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass-159*.6f)<.001f);
    dc_gpu_destroy(g);free(c);PASS();
}
static void test_rotated_body_uses_world_brush_coordinates(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_compound_shape_t shape=fixture_u();
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(7,20,20),&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,7,1.57079632679f,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,33,22,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    dc_gpu_body_motion_t motion;ASSERT_TRUE(dc_gpu_read_body_motion(g,7,&motion,err,sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass-159*.6f)<.001f);
    ASSERT_TRUE(fabsf(motion.angle-1.57079632679f)<1e-6f);
    dc_gpu_destroy(g);free(c);PASS();
}
static void test_air_carves_an_anchored_structural_member(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    c->cells[41*64+20].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_structural_gpu_graph_t graph={.node_count=2,.member_count=1,.anchor_count=1};
    graph.nodes[0]=(fr_structural_gpu_node_t){.id=1,.generation=1,.x_fp=20<<16,.y_fp=40<<16};
    graph.nodes[1]=graph.nodes[0];graph.nodes[1].id=2;graph.nodes[1].x_fp=32<<16;
    graph.members[0]=(fr_structural_gpu_member_t){1,1,1,2,FR_STRUCTURAL_WOOD,2<<16,0,0};
    graph.anchors[0]=(fr_structural_gpu_anchor_t){.id=1,.generation=1,.node=1,
        .kind=FR_JOINT_MOMENT,.x=20,.y=41,.width=1,.height=1,
        .terrain_material=DC_MATERIAL_STONE,.normal_stiffness=1e8,.shear_stiffness=1e8,
        .rotational_stiffness=1e8,.bearing_capacity=1e7,.pullout_capacity=1e7,
        .shear_capacity=1e7,.moment_capacity=1e7,.friction=.5};
    ASSERT_TRUE(fr_gpu_place_structure(g,&graph,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,fixture_body(19,40,40),err,sizeof(err)));
    dc_gpu_body_record_t *records=g->body_mapped;
    uint32_t *bond=dc_gpu_structural_data(g)+dc_gpu_structural_graph_offset()+30000u;
    bond[0]=1;bond[1]=0;bond[2]=1;bond[3]=records[0].body.id;bond[4]=19;
    ASSERT_TRUE(dc_gpu_paint_material(g,26,40,1,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    unsigned live=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        dc_gpu_body_record_t *b=(dc_gpu_body_record_t*)g->body_mapped+slot;
        if(!b->body.active) continue;
        if(b->body.id==19) continue;
        live++;ASSERT_TRUE(b->piece_count&DC_GPU_PIXEL_BODY_FLAG);ASSERT_TRUE(b->inverse_mass>0);
    }
    ASSERT_EQ(live,2u);ASSERT_EQ(bond[0],0u);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/carved_structure.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/carved_structure.dcs",err,sizeof(err)));
    dc_gpu_destroy(g);free(c);PASS();
}
static void test_trunk_only_carve_preserves_attached_canopy(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    for(unsigned x=20;x<28;x++) {
        c->cells[20*64+x].material=DC_MATERIAL_LEAVES;
        c->cells[21*64+x].material=DC_MATERIAL_WOOD;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(colors);
    for(unsigned x=20;x<28;x++) {
        colors->terrain[20*64+x][0]=.12f;colors->terrain[20*64+x][1]=.5f;
        colors->terrain[20*64+x][2]=.08f;colors->terrain[20*64+x][3]=1;
    }
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,0,colors,err,sizeof(err)));free(colors);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_record_t *body=g->body_mapped;
    body->body.x_fp=body->local_x_fp=20*65536;body->body.y_fp=body->local_y_fp=20*65536;
    body->body.vx_fp=body->body.vy_fp=0;body->angle=body->angular_velocity=0;
    ASSERT_TRUE(dc_gpu_paint_material(g,23,21,0,DC_MATERIAL_AIR,err,sizeof(err)));
    dc_gpu_body_pixel_t pixels[16];unsigned n=0,leaves=0,wood=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,16,&n,err,sizeof(err)));
    const uint32_t *shape=(uint32_t*)g->body_mapped+body->compound_reserved;
    for(unsigned i=0;i<n;i++) {
        wood+=pixels[i].material==DC_GPU_BODY_WOOD;
        if(pixels[i].material!=DC_GPU_BODY_LEAVES) continue;
        leaves++;
        unsigned cell=(pixels[i].y%16)*16+pixels[i].x%16;
        unsigned tile=(pixels[i].y/16)*shape[2]+pixels[i].x/16;
        unsigned word=body->compound_reserved+shape[8+tile]+1+cell/8;
        const float *rgb=dc_gpu_shape_colors(g,word)+(cell%8)*4;
        ASSERT_EQ(rgb[0],.12f);ASSERT_EQ(rgb[1],.5f);ASSERT_EQ(rgb[2],.08f);ASSERT_EQ(rgb[3],1.f);
    }
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    unsigned debris=c->particle_count;
    dc_gpu_destroy(g);free(c);
    ASSERT_EQ(leaves,8u);ASSERT_EQ(wood,7u);ASSERT_EQ(debris,0u);PASS();
}

static void test_broken_leaf_canopy_crumbles_without_losing_wood(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    ASSERT_TRUE(c);
    for(unsigned x=20;x<28;x++) {
        c->cells[20*64+x].material=9;
        c->cells[21*64+x].material=DC_MATERIAL_WOOD;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_chunk_colors_t *leaf_colors=calloc(1,sizeof(*leaf_colors));ASSERT_TRUE(leaf_colors);
    for(unsigned x=20;x<28;x++) {
        unsigned at=20*64+x;
        leaf_colors->terrain[at][0]=.02f;leaf_colors->terrain[at][1]=.5f;
        leaf_colors->terrain[at][2]=.08f;leaf_colors->terrain[at][3]=1.f;
    }
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,0,leaf_colors,err,sizeof(err)));free(leaf_colors);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixels[16];unsigned n;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,16,&n,err,sizeof(err)));
    ASSERT_EQ(n,16u);
    unsigned leaves=0;
    for(unsigned i=0;i<n;i++) leaves+=pixels[i].material==5;
    ASSERT_EQ(leaves,8u);
    dc_gpu_body_motion_t leaf_motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&leaf_motion,err,sizeof(err)));
    ASSERT_TRUE(fabs(leaf_motion.mass-6.4)<.001);
    ASSERT_TRUE(fabs(leaf_motion.density-.4)<.001);
    dc_gpu_body_record_t *b=g->body_mapped;
    b->body.x_fp=b->local_x_fp=20<<16;b->body.y_fp=b->local_y_fp=20<<16;
    b->body.vx_fp=b->body.vy_fp=0;b->angular_velocity=0;
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/leaf_canopy.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/leaf_canopy.dcs",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_runtime_prepare_fragments(g,err,sizeof(err)));
    for(unsigned slot=1;slot<DC_GPU_BODY_CAPACITY;slot++)
        ((dc_gpu_body_record_t*)g->body_mapped)[slot].reserved[0]=0x53545232u;
    reject_allocations=true;
    bool accepted=dc_gpu_paint_material(g,23,20,1,DC_MATERIAL_AIR,err,sizeof(err));
    reject_allocations=false;ASSERT_TRUE(!accepted);
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,16,&n,err,sizeof(err)));
    ASSERT_EQ(n,16u);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    ASSERT_EQ(c->particle_count,0u);
    for(unsigned slot=1;slot<DC_GPU_BODY_CAPACITY;slot++)
        ((dc_gpu_body_record_t*)g->body_mapped)[slot].reserved[0]=0u;
    ASSERT_TRUE(dc_gpu_paint_material(g,23,20,1,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    unsigned debris=0;uint64_t mass=0;
    for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(c->particles[i].mass_fp) {
        ASSERT_EQ(c->particles[i].material,10u);
        ASSERT_TRUE(c->particles[i].flags&0x20000000u);
        ASSERT_EQ(c->particles[i].grain_fp,DC_FLUID_FULL/2);
        debris++;mass+=c->particles[i].mass_fp;
    }
    ASSERT_EQ(debris,5u);
    ASSERT_TRUE(llabs((long long)mass-(long long)(.2*5*65536))<5);
    unsigned wood=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        b=(dc_gpu_body_record_t*)g->body_mapped+slot;if(!b->body.active) continue;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g,b->body.id,pixels,16,&n,err,sizeof(err)));
        for(unsigned i=0;i<n;i++) {ASSERT_EQ(pixels[i].material,DC_GPU_BODY_WOOD);wood++;}
    }
    ASSERT_EQ(wood,7u);
    unsigned source_particle=UINT32_MAX,destination=10*64+10;
    for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(c->particles[i].mass_fp) {source_particle=i;break;}
    ASSERT_TRUE(source_particle!=UINT32_MAX);
    uint32_t leaf_rgb=c->particles[source_particle].flags&0x00ffffffu;
    c->particles[destination]=c->particles[source_particle];
    c->particles[destination].x_fp=c->particles[destination].y_fp=10*65536+32768;
    c->particles[source_particle].mass_fp=0;
    c->cells[source_particle%4096].material=DC_MATERIAL_AIR;
    c->cells[destination].material=DC_MATERIAL_LEAF_DEBRIS;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t*)g->mapped)[destination],0xff000000u|leaf_rgb);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/leaf_canopy.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/leaf_canopy.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t*)g->mapped)[destination],0xff000000u|leaf_rgb);
    remove("build/leaf_canopy.dcs");dc_gpu_destroy(g);free(c);PASS();
}

static void test_leaf_impact_crumbles_canopy(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    for(unsigned x=20;x<28;x++) c->cells[20*64+x].material=9;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(g));
    uint32_t *pending=dc_gpu_body_solver_state(g,0)+52u;
    pending[0]=pending[1]=10000;pending[2]=257;pending[3]=0x80000001u;
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    unsigned debris=0;
    for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(c->particles[i].mass_fp) {
        ASSERT_EQ(c->particles[i].material,10u);debris++;
    }
    ASSERT_EQ(debris,8u);dc_gpu_destroy(g);free(c);PASS();
}

int main(void) {
    if(getenv("DC_TEST_WATER_SOURCE_RELEASE_ONLY")) {
        RUN(test_gpu_water_source_releases_newly_isolated_terrain);
        return g_fail!=0;
    }
    if(getenv("DC_TEST_BRUSH_TICK_ONLY")) {
        RUN(test_cut_and_physics_tick_share_the_occupancy_refresh);
        return g_fail!=0;
    }
    if(getenv("DC_TEST_BRUSH_INVALIDATION_ONLY")) {
        RUN(test_brush_invalidates_only_the_state_it_changes);
        return g_fail!=0;
    }
    if(getenv("DC_TEST_MISSED_BRUSH_ONLY")) {
        RUN(test_missed_brush_does_not_serialize_all_body_slots);
        return g_fail!=0;
    }
    if(getenv("DC_TEST_LARGE_BRUSH_ONLY")) {
        RUN(test_large_body_brush_preserves_mass_with_bounded_latency);
        return g_fail!=0;
    }
    RUN(test_large_body_brush_preserves_mass_with_bounded_latency);
    RUN(test_gpu_water_source_releases_newly_isolated_terrain);
    RUN(test_cut_and_physics_tick_share_the_occupancy_refresh);
    RUN(test_brush_invalidates_only_the_state_it_changes);
    RUN(test_missed_brush_does_not_serialize_all_body_slots);
    RUN(test_trunk_only_carve_preserves_attached_canopy);
    RUN(test_leaf_impact_crumbles_canopy);
    RUN(test_broken_leaf_canopy_crumbles_without_losing_wood);
    RUN(test_air_carves_colored_body_and_splits_remaining_components);
    RUN(test_air_carves_compound_wall_without_filling_its_cavity);
    RUN(test_rotated_body_uses_world_brush_coordinates);
    RUN(test_air_carves_an_anchored_structural_member);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
