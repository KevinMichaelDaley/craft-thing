#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/vulkan/runtime_body_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s (%s)\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_growth_preserves_geometry_colors_and_gravity_across_pages(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=54;y<56;y++) for(unsigned x=54;x<56;x++) chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(g->runtime_body_consumers);
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    for(unsigned y=54;y<56;y++) for(unsigned x=54;x<56;x++) {
        float *color=(float *)g->shape_color_mapped+(y*64+x)*4u;
        color[0]=.125f;color[1]=.25f;color[2]=.5f;color[3]=1.f;
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    for(unsigned i=1;i<=140;i++) {
        dc_gpu_world_body_t b={.body={.id=i,.active=1,.width=1,.height=1,
            .x_fp=(4+(int)(i%12)*4)*65536,.y_fp=(4+(int)(i/12)*4)*65536}};
        ASSERT_TRUE(dc_gpu_spawn_world_body(g,b,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    for(unsigned i=1;i<=140;i++) {
        dc_gpu_world_body_t b;ASSERT_TRUE(dc_gpu_read_world_body(g,i,&b,err,sizeof(err)));
        ASSERT_TRUE(b.body.y_fp>(4+(int)(i/12)*4)*65536);
    }
    uint32_t words=dc_gpu_pixel_pool_capacity(g);
    ASSERT_TRUE(dc_gpu_runtime_geometry_reserve(g,words+1,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_runtime_queues_reserve(g,8193,16385,err,sizeof(err)));
    ASSERT_TRUE(g->runtime_pair_capacity>=8193&&g->runtime_contact_capacity>=16385);
    dc_gpu_body_pixel_t pixels[4];uint32_t count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,4,&count,err,sizeof(err)));
    ASSERT_EQ(count,4u);
    const dc_gpu_body_record_t *body=dc_gpu_body_record(g,0);
    const uint32_t *shape=(const uint32_t *)g->body_mapped+body->compound_reserved;
    uint32_t word=body->compound_reserved+8u+shape[2]*shape[3]+1u;
    float *color=dc_gpu_shape_colors(g,word);
    ASSERT_TRUE(color[0]==.125f&&color[1]==.25f&&color[2]==.5f&&color[3]==1.f);
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,4,&count,err,sizeof(err)));
    ASSERT_EQ(count,4u);
    dc_gpu_destroy(g);free(chunk);PASS();
}
static void test_gpu_queue_exhaustion_grows_and_retries_without_losing_bodies(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_compound_shape_t shape={.count=4};
    for(unsigned i=0;i<4;i++) {
        int x=(int)(i%2)*65536,y=(int)(i/2)*65536;
        shape.pieces[i]=(dc_gpu_body_shape_t){.count=4,.material=DC_GPU_BODY_STONE,
            .vertices={{x,y},{x+65536,y},{x+65536,y+65536},{x,y+65536}}};
    }
    for(unsigned i=1;i<=100;i++) {
        dc_gpu_world_body_t body={.body={.id=i,.active=1,.width=2,.height=2,.x_fp=30*65536,.y_fp=30*65536}};
        ASSERT_TRUE(dc_gpu_spawn_compound_body(g,body,&shape,err,sizeof(err)));
    }
    bool complete=false;
    for(unsigned attempt=0;attempt<6;attempt++) {
        g->body_refresh_pending=true;
        ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
        dc_gpu_broadphase_stats_t pairs;dc_gpu_contact_stats_t contacts;
        ASSERT_TRUE(dc_gpu_read_broadphase(g,&pairs,NULL,0,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_contacts(g,&contacts,NULL,0,err,sizeof(err)));
        if(!pairs.overflow&&!contacts.overflow) {complete=true;break;}
    }
    ASSERT_TRUE(complete);
    ASSERT_TRUE(g->runtime_pair_capacity>DC_GPU_BROADPHASE_PAIR_CAPACITY);
    ASSERT_TRUE(g->runtime_contact_capacity>DC_GPU_CONTACT_CAPACITY);
    for(unsigned i=1;i<=100;i++) {
        dc_gpu_world_body_t body;ASSERT_TRUE(dc_gpu_read_world_body(g,i,&body,err,sizeof(err)));
        ASSERT_EQ(body.body.id,i);ASSERT_EQ(body.body.x_fp,30*65536);ASSERT_EQ(body.body.y_fp,30*65536);
    }
    dc_gpu_destroy(g);free(chunk);PASS();
}
static void test_one_concave_component_fractures_into_more_than_a_page_of_bodies(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,256,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned page=0;page<4;page++) {
        memset(chunk,0,sizeof(*chunk));chunk->coord.x=page;
        for(unsigned y=10;y<23;y++) for(unsigned x=0;x<64;x++) {
            unsigned world=page*64+x;
            if(world>=5&&world<215&&(y==10||(world-5)%3==0))
                chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,page,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,page,0,page,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(g));
    uint32_t *pending=dc_gpu_body_solver_state(g,0)+52u;
    pending[0]=pending[1]=1000000;pending[2]=2;pending[3]=0x80000001u;
    for(unsigned retry=0;retry<3;retry++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    unsigned bodies=0,pixels=0;double mass=0;
    for(unsigned serial=1;serial<=71;serial++) {
        uint32_t count=0;dc_gpu_body_motion_t motion;
        if(!dc_gpu_read_pixel_shape(g,0x80000000u+serial,NULL,0,&count,err,sizeof(err))) {
            uint32_t *state=dc_gpu_body_solver_state(g,0);
            printf("Missing fragment %u; capacity%u source pixels%u pending %u,%u,%u,%x mailbox slots%u fracture%u geometry%u\n",
                serial,dc_gpu_body_capacity(g),dc_gpu_body_record(g,0)->piece_count&~DC_GPU_PIXEL_BODY_FLAG,
                state[52],state[53],state[54],state[55],((uint32_t *)g->runtime_bodies->mailbox.mapped)[2],
                ((uint32_t *)g->runtime_bodies->mailbox.mapped)[9],((uint32_t *)g->runtime_bodies->mailbox.mapped)[11]);
            uint32_t *mail=g->runtime_bodies->mailbox.mapped;float mass_error,inertia_error;
            memcpy(&mass_error,mail+14,4);memcpy(&inertia_error,mail+15,4);
            printf("Conservation rejection bits%u outputs%u mass error%g inertia error%g\n",mail[12],mail[13],mass_error,inertia_error);
        }
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000000u+serial,NULL,0,&count,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000000u+serial,&motion,err,sizeof(err)));
        bodies++;pixels+=count;mass+=motion.mass;
    }
    ASSERT_EQ(bodies,71u);ASSERT_EQ(pixels,1050u);ASSERT_TRUE(fabs(mass-1050*2.7)<.02);
    for(unsigned slot=0;slot<71;slot++) ASSERT_EQ(dc_gpu_body_solver_state(g,slot)[60],0x80000001u);
    dc_gpu_destroy(g);free(chunk);PASS();
}
int main(void) {
    if(getenv("DC_TEST_FRACTURE_ONLY")) {
        RUN(test_one_concave_component_fractures_into_more_than_a_page_of_bodies);
        return g_fail!=0;
    }
    RUN(test_growth_preserves_geometry_colors_and_gravity_across_pages);
    RUN(test_gpu_queue_exhaustion_grows_and_retries_without_losing_bodies);
    RUN(test_one_concave_component_fractures_into_more_than_a_page_of_bodies);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
