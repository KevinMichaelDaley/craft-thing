#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#include "../../src/app/level.h"
#include "../../src/app/view_config.h"
#include "dungeoncraft/stream.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
enum { SOURCE_X=80,SOURCE_Y=60,SOURCE_WIDTH=80,SOURCE_HEIGHT=40,SOURCE_PIXELS=3200 };

/* Disk initialization makes the environment reproducible without introducing
   bodies or bypassing the streamed level's ownership/paint/extraction path. */
static bool seed_mainland_pond(const char *directory,uint16_t material) {
    dc_streamer_t *stream=dc_stream_create(directory,314,2);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    bool okay=stream&&chunk;
    for(int cy=INITIAL_CHUNK_Y-1;okay&&cy<INITIAL_CHUNK_Y+(int)SIM_CHUNKS_Y-1;cy++)
        for(int cx=-1;okay&&cx<(int)SIM_CHUNKS_X-1;cx++) {
            memset(chunk,0,sizeof(*chunk));chunk->coord=(dc_chunk_coord_t){cx,cy};
            for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
                int wx=cx*64+(int)x,wy=cy*64+(int)y;
                dc_cell_t *cell=&chunk->cells[y*64+x];
                bool source=wx>=SOURCE_X&&wx<SOURCE_X+SOURCE_WIDTH&&
                    wy>=INITIAL_CHUNK_Y*64+SOURCE_Y&&wy<INITIAL_CHUNK_Y*64+SOURCE_Y+SOURCE_HEIGHT;
                bool mainland=wy>=20||wx==-64||(wy==INITIAL_CHUNK_Y*64+80&&wx<SOURCE_X);
                if(source) cell->material=material;
                else if(mainland) cell->material=DC_MATERIAL_STONE;
                else if(wy>=-80&&wy<20) cell->fluid_mass=DC_FLUID_FULL;
            }
            okay=dc_stream_request_save(stream,chunk,1);
            bool completed=false;struct timespec delay={0,1000000};
            for(unsigned wait=0;okay&&!completed&&wait<10000;wait++) {
                dc_stream_result_t result;
                if(dc_stream_poll(stream,&result)) {
                    okay=result.kind==DC_STREAM_SAVED;completed=true;
                    dc_stream_result_release(&result);
                } else thrd_sleep(&delay,NULL);
            }
            okay=okay&&completed;
        }
    free(chunk);dc_stream_destroy(stream);return okay;
}
static void cut_material(uint16_t material) {
    char directory[]="build/ui_material_cut_XXXXXX",err[256]={0};
    ASSERT_TRUE(mkdtemp(directory));ASSERT_TRUE(seed_mainland_pond(directory,material));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));
    ASSERT_TRUE(view);ASSERT_TRUE(dc_level_view_set_spring_enabled(view,false));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    dc_level_view_status_t residency;
    ASSERT_TRUE(dc_level_view_status(view,&residency));
    ASSERT_EQ(residency.ready_chunks,residency.total_chunks);
    ASSERT_TRUE(dc_level_view_paint(view,SOURCE_X,SOURCE_Y,0,material,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    dc_gpu_world_body_t body;dc_gpu_body_motion_t motion;
    ASSERT_TRUE(!dc_level_view_body_state(view,0x80000001u,&body,&motion,err,sizeof(err)));
    /* This is the same eraser and water brush used by the interactive sandbox. */
    ASSERT_TRUE(dc_level_view_paint(view,SOURCE_X-1,80,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view,SOURCE_X-1,80,0,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_body_state(view,0x80000001u,&body,&motion,err,sizeof(err)));
    ASSERT_EQ(body.body.width,SOURCE_WIDTH);ASSERT_EQ(body.body.height,SOURCE_HEIGHT);
    float density=material==DC_MATERIAL_STONE?2.7f:.6f;
    ASSERT_TRUE(fabsf(motion.mass-density*SOURCE_PIXELS)<.02f);
    ASSERT_TRUE(fabsf(motion.density-density)<1e-5f);
    ASSERT_TRUE((motion.flags&DC_GPU_BODY_KINEMATIC)==0);
    unsigned remaining=0;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(int cy=-2;cy<=-1;cy++) for(int cx=1;cx<=2;cx++) {
        ASSERT_TRUE(dc_level_view_chunk(view,(dc_chunk_coord_t){cx,cy},chunk,err,sizeof(err)));
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            int wx=cx*64+(int)x,wy=cy*64+(int)y;
            if(wx>=SOURCE_X&&wx<SOURCE_X+SOURCE_WIDTH&&wy>=-68&&wy<-28)
                remaining+=chunk->cells[y*64+x].material!=DC_MATERIAL_AIR;
        }
    }
    free(chunk);ASSERT_EQ(remaining,0u);
    double initial_y=body.chunk.y*64.+body.body.y_fp/65536.+motion.center_y;
    for(unsigned tick=0;tick<20;tick++) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_body_state(view,0x80000001u,&body,&motion,err,sizeof(err)));
    double final_y=body.chunk.y*64.+body.body.y_fp/65536.+motion.center_y;
    dc_gpu_rigid_solver_stats_t stats;
    ASSERT_TRUE(dc_level_view_rigid_solver_stats(view,&stats));
    ASSERT_EQ(stats.overflow,0u);ASSERT_EQ(stats.solved_contacts,0u);
    printf("Paint cut material%u mass%.3f density%.3f centerY%.6f→%.6f\n",material,motion.mass,motion.density,initial_y,final_y);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    ASSERT_TRUE(material==DC_MATERIAL_STONE?final_y>initial_y+1:final_y<initial_y-1);PASS();
}
static void test_large_painted_stone_cut_sinks_across_known_seams(void) {cut_material(DC_MATERIAL_STONE);}
static void test_matched_painted_wood_cut_floats_across_known_seams(void) {cut_material(DC_MATERIAL_WOOD);}
int main(void) {
    RUN(test_large_painted_stone_cut_sinks_across_known_seams);
    RUN(test_matched_painted_wood_cut_floats_across_known_seams);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
