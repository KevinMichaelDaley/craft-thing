#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "../../src/app/level.h"
#include "../../src/app/prop_scene.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static dc_gpu_t *observed_gpu;
static unsigned wood_body_samples;
static bool moving_wood_seen;
static uint32_t tracked_id[DC_GPU_BODY_CAPACITY];
static float initial_center_y[DC_GPU_BODY_CAPACITY],upward_wood_motion;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

bool __real_dc_gpu_read_contacts(dc_gpu_t *,dc_gpu_contact_stats_t *,dc_gpu_contact_t *,uint32_t,char *,uint32_t);
bool __wrap_dc_gpu_read_contacts(dc_gpu_t *g,dc_gpu_contact_stats_t *s,dc_gpu_contact_t *c,uint32_t n,char *err,uint32_t cap) {
    observed_gpu=g;return __real_dc_gpu_read_contacts(g,s,c,n,err,cap);
}
static bool screenshot(dc_level_view_t *view,const char *path,char *err) {
    uint32_t *pixels=calloc((size_t)VIEW_WIDTH*VIEW_HEIGHT,4);
    bool okay=pixels&&dc_level_view_pixels(view,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,256);
    SDL_Surface *surface=okay?SDL_CreateRGBSurfaceWithFormatFrom(pixels,VIEW_WIDTH,VIEW_HEIGHT,32,VIEW_WIDTH*4,SDL_PIXELFORMAT_ABGR8888):NULL;
    okay=surface&&SDL_SaveBMP(surface,path)==0;
    if(surface) SDL_FreeSurface(surface);
    free(pixels);return okay;
}
static unsigned stone_overlap(dc_gpu_t *g,char *err) {
    if(!dc_gpu_copy_chunk_state(g,UINT32_MAX,false,false,err,256)) return UINT32_MAX;
    dc_gpu_body_pixel_t pixels[8192];unsigned overlaps=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        dc_gpu_body_record_t b=((dc_gpu_body_record_t*)g->body_mapped)[slot];
        if(!b.body.active||!(b.piece_count&DC_GPU_PIXEL_BODY_FLAG)) continue;
        unsigned count=0;
        if(!dc_gpu_read_pixel_shape(g,b.body.id,pixels,8192,&count,err,256)) return UINT32_MAX;
        bool wood=false;for(unsigned i=0;i<count;i++) wood|=pixels[i].material==DC_GPU_BODY_WOOD;
        if(!wood) continue;
        float center_y=b.body.y_fp/65536.f+b.center[1];
        if(tracked_id[slot]!=b.body.id) {tracked_id[slot]=b.body.id;initial_center_y[slot]=center_y;}
        upward_wood_motion=fmaxf(upward_wood_motion,initial_center_y[slot]-center_y);
        wood_body_samples++;moving_wood_seen|=b.body.vy_fp>0;
        for(unsigned i=0;i<count;i++) {
            float x=pixels[i].x+.5f-b.center[0],y=pixels[i].y+.5f-b.center[1];
            int cx=(int)floorf(b.body.x_fp/65536.f+b.center[0]+cosf(b.angle)*x-sinf(b.angle)*y);
            int cy=(int)floorf(b.body.y_fp/65536.f+b.center[1]+sinf(b.angle)*x+cosf(b.angle)*y);
            if(cx<0||cy<0||cx>=(int)g->width||cy>=(int)g->height) continue;
            unsigned page=((uint32_t*)g->page_mapped)[(cy/64)*g->page_width+cx/64];
            if(page&&((dc_cell_t*)g->chunk_mapped)[(page-1)*4096+(cy%64)*64+cx%64].material==DC_MATERIAL_STONE) overlaps++;
        }
    }
    return overlaps;
}
static unsigned attached_material_pixels(dc_gpu_t *g,unsigned material,char *err) {
    dc_gpu_body_pixel_t pixels[8192];unsigned total=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        dc_gpu_body_record_t body=((dc_gpu_body_record_t*)g->body_mapped)[slot];
        if(!body.body.active||!(body.piece_count&DC_GPU_PIXEL_BODY_FLAG)) continue;
        unsigned count=0;
        if(!dc_gpu_read_pixel_shape(g,body.body.id,pixels,8192,&count,err,256)) return UINT32_MAX;
        for(unsigned i=0;i<count;i++) total+=pixels[i].material==material;
    }
    return total;
}
static void test_level03_stays_packed_and_buried_trunk_cut_stays_supported(void) {
    char directory[]="build/ui_level03_support_XXXXXX",err[256]={0};
    const char *asset="assets/props/level_piece03.prop/asset.cprop";
    ASSERT_TRUE(mkdtemp(directory));printf("scene checkpoint %s\n",directory);
    dc_level_view_t *view=dc_prop_scene_create(directory,314,asset,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_prop_scene_load(view,asset,err,sizeof(err)));
    dc_gpu_contact_stats_t contacts;
    ASSERT_TRUE(dc_level_view_contacts(view,&contacts,NULL,0,err,sizeof(err)));
    dc_gpu_t *g=observed_gpu;
    for(unsigned tick=0;tick<90;tick++) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    }
    unsigned wood=0,doubled=0;uint64_t soil_mass=0;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned page=0;page<70;page++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g,page,chunk,err,sizeof(err)));
        for(unsigned cell=0;cell<4096;cell++) {
            wood+=chunk->cells[cell].material==DC_MATERIAL_WOOD;
            for(unsigned layer=0;layer<2;layer++) {
                dc_mpm_particle_t *p=chunk->particles+cell+layer*4096;
                if(p->material==DC_MATERIAL_DIRT||p->material==DC_MATERIAL_GRAVEL) soil_mass+=p->mass_fp;
            }
            doubled+=(chunk->cells[cell].material==DC_MATERIAL_DIRT||chunk->cells[cell].material==DC_MATERIAL_GRAVEL)&&chunk->particles[cell+4096].mass_fp!=0;
        }
    }
    free(chunk);printf("after90 ticks terrain wood%u doubled soil/gravel%u\n",wood,doubled);
    ASSERT_TRUE(screenshot(view,"build/level03_packed.bmp",err));
    if(wood!=704u||doubled!=0u) {
        for(unsigned i=0;i<DC_GPU_BODY_CAPACITY;i++) {
            dc_gpu_body_record_t *b=(dc_gpu_body_record_t*)g->body_mapped+i;
            if(b->body.active) printf("premature body%u size%ux%u pos%g,%g\n",b->body.id,b->body.width,b->body.height,b->body.x_fp/65536.,b->body.y_fp/65536.);
        }
        ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
        ASSERT_EQ(wood,704u);ASSERT_EQ(doubled,0u);
    }
    chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_level_view_chunk(view,(dc_chunk_coord_t){0,0},chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[2*64+8].material,DC_MATERIAL_DIRT);free(chunk);
    ASSERT_TRUE(dc_level_view_paint(view,VIEW_WIDTH/2+8-32,VIEW_HEIGHT/2+2-32,0,DC_MATERIAL_AIR,err,sizeof(err)));
    for(unsigned tick=0;tick<120;tick++) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    }
    wood=0;doubled=0;uint64_t remaining_mass=0;chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned page=0;page<70;page++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g,page,chunk,err,sizeof(err)));
        for(unsigned cell=0;cell<4096;cell++) {
            wood+=chunk->cells[cell].material==DC_MATERIAL_WOOD;
            for(unsigned layer=0;layer<2;layer++) {
                dc_mpm_particle_t *p=chunk->particles+cell+layer*4096;
                if(p->material==DC_MATERIAL_DIRT||p->material==DC_MATERIAL_GRAVEL) remaining_mass+=p->mass_fp;
            }
            doubled+=(chunk->cells[cell].material==DC_MATERIAL_DIRT||chunk->cells[cell].material==DC_MATERIAL_GRAVEL)&&chunk->particles[cell+4096].mass_fp!=0;
        }
    }
    free(chunk);printf("single dirt erase: terrain wood%u doubled soil/gravel%u\n",wood,doubled);
    ASSERT_TRUE(screenshot(view,"build/level03_single_dirt_cut.bmp",err));
    ASSERT_EQ(wood,704u);ASSERT_EQ(doubled,0u);
    ASSERT_EQ(remaining_mass,soil_mass-78643u);
    for(int x=0;x<=34;x+=2) ASSERT_TRUE(dc_level_view_paint(view,VIEW_WIDTH/2+x-32,VIEW_HEIGHT/2-8-32,2,DC_MATERIAL_AIR,err,sizeof(err)));
    unsigned worst=0;
    for(unsigned tick=0;tick<240;tick++) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
        if(tick%8==7) {
            unsigned overlap=stone_overlap(g,err);if(overlap>worst) worst=overlap;
            printf("cut tick%u wood-body centers in stone%u\n",tick+1,overlap);
        }
    }
    ASSERT_TRUE(screenshot(view,"build/level03_tree_stopped.bmp",err));
    printf("worst wood-body centers in stone%u\n",worst);
    printf("largest upward wood center displacement %.4f\n",upward_wood_motion);
    ASSERT_EQ(stone_overlap(g,err),0u);
    ASSERT_EQ(attached_material_pixels(g,DC_GPU_BODY_WOOD,err),0u);
    ASSERT_EQ(attached_material_pixels(g,DC_GPU_BODY_LEAVES,err),0u);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    ASSERT_EQ(wood_body_samples,0u);ASSERT_TRUE(!moving_wood_seen);
    ASSERT_TRUE(worst<=2u);ASSERT_TRUE(upward_wood_motion<.5f);PASS();
}
int main(void) {
    RUN(test_level03_stays_packed_and_buried_trunk_cut_stays_supported);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
