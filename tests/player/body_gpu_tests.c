#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/player/beaver.h"
#include "ferrum/player/body_gpu.h"
#include "ferrum/renderer/sprite_gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_canonical_body_admits_hidden_owners_once(void) {
    dc_gpu_t *g=NULL;char err[256]={0};
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->coord=(dc_chunk_coord_t){-8,11};
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));free(chunk);
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){-8,11},err,sizeof(err)));
    fr_beaver_body_t body;
    ASSERT_TRUE(fr_beaver_body_admit(g,41,71,0,(dc_chunk_coord_t){0,0},8*65536,9*65536,&body,err,sizeof(err)));
    ASSERT_EQ(body.binding.identity,41);ASSERT_TRUE(body.binding.generation);
    uint32_t expected=0,source=0;
    for(uint32_t s=1;s<=FR_BEAVER_PIXELS;s++) {fr_beaver_source_t meta;if(fr_beaver_source(s,&meta)) {expected++;source=s;}}
    uint32_t count=0;ASSERT_TRUE(dc_gpu_read_pixel_shape(g,body.binding.body_id,NULL,0,&count,err,sizeof(err)));ASSERT_EQ(count,expected);
    fr_gpu_reactive_stats_t stats;ASSERT_TRUE(fr_gpu_reactive_read_stats(g,&stats,err,sizeof(err)));ASSERT_EQ(stats.live,expected);
    fr_reactive_record_t parcel;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,body.binding.body_id,(source-1)%40-body.rest_origin_x,
        (source-1)/40-body.rest_origin_y,&parcel,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_EQ(parcel.owner.key[0],body.binding.body_id);ASSERT_EQ(parcel.owner.key[1],body.binding.generation);
    dc_gpu_world_body_t pose;ASSERT_TRUE(dc_gpu_read_world_body(g,body.binding.body_id,&pose,err,sizeof(err)));
    ASSERT_EQ(pose.body.x_fp,8*65536);ASSERT_EQ(pose.body.y_fp,9*65536);
    dc_gpu_destroy(g);PASS();
}

static bool read_parcel(dc_gpu_t *g, const fr_beaver_body_t *body, uint32_t source,
                        fr_reactive_record_t *parcel, char *err, uint32_t cap) {
    uint32_t x = (source - 1u) % FR_BEAVER_WIDTH - body->rest_origin_x;
    uint32_t y = (source - 1u) / FR_BEAVER_WIDTH - body->rest_origin_y;
    bool found;
    return fr_gpu_reactive_read_body(g, body->binding.body_id, x, y, parcel, &found, err, cap) && found;
}

static void test_wetting_follows_visible_owner_when_a_sprite_is_mirrored(void) {
    enum { FUR = 30u * FR_BEAVER_WIDTH + 27u, HAT = 2u * FR_BEAVER_WIDTH + 32u,
           HIDDEN = 20u * FR_BEAVER_WIDTH + 21u, WATER_X = 52, WATER_Y = 6,
           POSE_WIDTH = 9 };
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(dc_gpu_create(&g, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk && dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    fr_beaver_body_t body;
    ASSERT_TRUE(fr_beaver_body_admit(g, 41, 71, 0, (dc_chunk_coord_t){0, 0},
                                    8 * DC_FLUID_FULL, 8 * DC_FLUID_FULL, &body, err, sizeof(err)));
    const fr_renderer_sprite_pixel_t atlas[POSE_WIDTH] = {
        [0] = {0xffc08040u, FUR}, [POSE_WIDTH - 1] = {0xff8040c0u, HAT}
    };
    fr_renderer_sprite_t pose = {
        .body_id = body.binding.body_id, .generation = body.binding.generation,
        .width = POSE_WIDTH, .height = 1,
        .rest_origin_x = body.rest_origin_x, .rest_origin_y = body.rest_origin_y,
        .origin_x = 8 - WATER_X, .origin_y = 8 - WATER_Y
    };
    ASSERT_TRUE(fr_renderer_sprite_atlas(g, atlas, POSE_WIDTH, err, sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g, err, sizeof(err)));
    const uint32_t water_cell = WATER_Y * DC_CHUNK_SIDE + WATER_X;
    uint32_t wet_fur = 0;
    for (uint32_t flip = 0; flip < 2u; ++flip) {
        pose.flip = flip;
        ASSERT_TRUE(fr_renderer_sprite_poses(g, &pose, 1, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_paint_material(g, WATER_X, WATER_Y, 0, DC_MATERIAL_WATER, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
        const uint32_t water_before = chunk->cells[water_cell].fluid_mass;
        ASSERT_TRUE(water_before > 0);
        ASSERT_TRUE(fr_thermal_gpu_step(g, .25f, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
        fr_reactive_record_t fur, hat, hidden;
        ASSERT_TRUE(read_parcel(g, &body, FUR, &fur, err, sizeof(err)));
        ASSERT_TRUE(read_parcel(g, &body, HAT, &hat, err, sizeof(err)));
        ASSERT_TRUE(read_parcel(g, &body, HIDDEN, &hidden, err, sizeof(err)));
        ASSERT_EQ(hidden.state.bound_water_fp, 0u);
        if (!flip) {
            wet_fur = fur.state.bound_water_fp;
            ASSERT_TRUE(wet_fur > 0);
            ASSERT_EQ(hat.state.bound_water_fp, 0u);
            ASSERT_EQ(water_before - chunk->cells[water_cell].fluid_mass, wet_fur);
        } else {
            ASSERT_EQ(fur.state.bound_water_fp, wet_fur);
            ASSERT_TRUE(hat.state.bound_water_fp > 0);
            ASSERT_EQ(water_before - chunk->cells[water_cell].fluid_mass, hat.state.bound_water_fp);
        }
        ASSERT_TRUE(dc_gpu_paint_material(g, WATER_X, WATER_Y, 0, DC_MATERIAL_AIR, err, sizeof(err)));
    }
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

int main(void) {
    RUN(test_canonical_body_admits_hidden_owners_once);
    RUN(test_wetting_follows_visible_owner_when_a_sprite_is_mirrored);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
