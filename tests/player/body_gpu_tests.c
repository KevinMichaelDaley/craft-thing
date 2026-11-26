#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/blackbody.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
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
    enum { FUR_X = 26, FUR_Y = 30, FUR = FUR_Y * FR_BEAVER_WIDTH + FUR_X + 1u,
           HAT = 2u * FR_BEAVER_WIDTH + 32u,
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
    const fr_renderer_sprite_pixel_t atlas[2u * POSE_WIDTH] = {
        [0] = {0xffc08040u, FUR}, [POSE_WIDTH - 1] = {0xff8040c0u, HAT},
        [2u * POSE_WIDTH - 1u] = {0xff8040c0u, HAT}
    };
    fr_renderer_sprite_t pose = {
        .body_id = body.binding.body_id, .generation = body.binding.generation,
        .width = POSE_WIDTH, .height = 1,
        .rest_origin_x = body.rest_origin_x, .rest_origin_y = body.rest_origin_y,
        .origin_x = 8 - WATER_X, .origin_y = 8 - WATER_Y
    };
    ASSERT_TRUE(fr_renderer_sprite_atlas(g, atlas, 2u * POSE_WIDTH, err, sizeof(err)));
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
    pose.offset = POSE_WIDTH;
    pose.flip = 0u;
    ASSERT_TRUE(fr_renderer_sprite_poses(g, &pose, 1, err, sizeof(err)));
    const uint32_t rest_x = 8u + FUR_X - body.rest_origin_x;
    const uint32_t rest_y = 8u + FUR_Y - body.rest_origin_y;
    ASSERT_TRUE(dc_gpu_paint_material(g, WATER_X, WATER_Y, 0, DC_MATERIAL_WATER, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g, rest_x, rest_y, 0, DC_MATERIAL_WATER, err, sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_step(g, .25f, err, sizeof(err)));
    fr_reactive_record_t fur;
    ASSERT_TRUE(read_parcel(g, &body, FUR, &fur, err, sizeof(err)));
    ASSERT_EQ(fur.state.bound_water_fp, wet_fur);
    ASSERT_TRUE(fr_renderer_sprite_poses(g, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_step(g, .25f, err, sizeof(err)));
    ASSERT_TRUE(read_parcel(g, &body, FUR, &fur, err, sizeof(err)));
    ASSERT_TRUE(fur.state.bound_water_fp > wet_fur);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_terrain_heat_reaches_visible_owner_and_balances_source(void) {
    enum { FUR = 30u * FR_BEAVER_WIDTH + 27u, HAT = 2u * FR_BEAVER_WIDTH + 32u,
           CONTACT_X = 52, CONTACT_Y = 6, POSE_WIDTH = 9,
           STONE_X = CONTACT_X + 1, STONE_CELL = CONTACT_Y * DC_CHUNK_SIDE + STONE_X };
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(dc_gpu_create(&g, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    bool uploaded = dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err));
    free(chunk);
    ASSERT_TRUE(uploaded && dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
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
        .origin_x = 8 - CONTACT_X, .origin_y = 8 - CONTACT_Y
    };
    ASSERT_TRUE(fr_renderer_sprite_atlas(g, atlas, POSE_WIDTH, err, sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g, STONE_X, CONTACT_Y, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    const fr_reactive_profile_t *stone = fr_reactive_profile(DC_MATERIAL_STONE);
    const uint32_t mass = (uint32_t)lroundf(stone->density * DC_FLUID_FULL);
    fr_reactive_state_t hot;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE, mass, 0, 900.f, 10000u, &hot, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g, 0, STONE_CELL, &hot, err, sizeof(err)));
    for (uint32_t flip = 0; flip < 2u; ++flip) {
        pose.flip = flip;
        ASSERT_TRUE(fr_renderer_sprite_poses(g, &pose, 1, err, sizeof(err)));
        const uint32_t target = flip ? HAT : FUR;
        fr_reactive_record_t before, after, source_before, source_after;
        bool found;
        ASSERT_TRUE(read_parcel(g, &body, target, &before, err, sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g, 0, STONE_CELL, &source_before, &found, err, sizeof(err)) && found);
        ASSERT_TRUE(fr_thermal_gpu_step(g, .25f, err, sizeof(err)));
        ASSERT_TRUE(read_parcel(g, &body, target, &after, err, sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g, 0, STONE_CELL, &source_after, &found, err, sizeof(err)) && found);
        const double gained = fr_reactive_state_energy(&after.state) - fr_reactive_state_energy(&before.state);
        const double spent = fr_reactive_state_energy(&source_before.state) - fr_reactive_state_energy(&source_after.state);
        printf("Posed contact %u transferred %.6f heat from debit %.6f\n", flip, gained, spent);
        ASSERT_TRUE(gained > .01 && fabs(gained - spent) < .001);
        ASSERT_EQ(after.state.parcel_id_low, before.state.parcel_id_low);
        ASSERT_EQ(after.state.parcel_id_high, before.state.parcel_id_high);
    }
    dc_gpu_destroy(g);
    PASS();
}

static void test_visible_combustion_uses_local_oxygen_and_leaves_hidden_fuel(void) {
    enum {
        FUR = 30u * FR_BEAVER_WIDTH + 27u, HAT = 2u * FR_BEAVER_WIDTH + 32u,
        CONTACT_X = 52, CONTACT_Y = 6, POSE_WIDTH = 9,
        OXYGEN_CELL = CONTACT_Y * DC_CHUNK_SIDE + CONTACT_X + 1u
    };
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(dc_gpu_create(&g, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    bool uploaded = dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err));
    free(chunk);
    ASSERT_TRUE(uploaded && dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    fr_beaver_body_t body;
    ASSERT_TRUE(fr_beaver_body_admit(g, 41, 71, 0, (dc_chunk_coord_t){0, 0},
                                    8 * DC_FLUID_FULL, 8 * DC_FLUID_FULL, &body, err, sizeof(err)));
    const fr_renderer_sprite_pixel_t atlas[2u * POSE_WIDTH] = {
        [0] = {0xffc08040u, FUR}, [POSE_WIDTH - 1] = {0xff8040c0u, HAT},
        [2u * POSE_WIDTH - 1] = {0xff8040c0u, HAT}
    };
    fr_renderer_sprite_t pose = {
        .body_id = body.binding.body_id, .generation = body.binding.generation,
        .width = POSE_WIDTH, .height = 1,
        .rest_origin_x = body.rest_origin_x, .rest_origin_y = body.rest_origin_y,
        .origin_x = 8 - CONTACT_X, .origin_y = 8 - CONTACT_Y
    };
    const fr_gas_model_t model = {
        .gas_constant = {.25984, .29680, .18892, .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
        .kinetic_scale = 1. / 512.
    };
    ASSERT_TRUE(fr_renderer_sprite_atlas(g, atlas, 2u * POSE_WIDTH, err, sizeof(err)) &&
                fr_thermal_gpu_enable(g, err, sizeof(err)) &&
                fr_gas_gpu_enable(g, &model, .0012f, 293.15f, err, sizeof(err)) &&
                fr_combustion_gpu_enable(g, err, sizeof(err)));
    fr_gas_state_t *gas = calloc(DC_CHUNK_CELLS, sizeof(*gas));
    double *volume = calloc(DC_CHUNK_CELLS, sizeof(*volume));
    fr_combustion_emission_t *emission = calloc(DC_CHUNK_CELLS, sizeof(*emission));
    ASSERT_TRUE(gas && volume && emission);
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) volume[i] = 1.;
    const double oxygen = .002;
    const fr_gas_state_t air = {
        .species = {[FR_GAS_OXYGEN] = oxygen, [FR_GAS_NITROGEN] = .006},
        .energy = (oxygen * model.heat_capacity_cv[FR_GAS_OXYGEN] +
                   .006 * model.heat_capacity_cv[FR_GAS_NITROGEN]) * 293.15
    };
    const uint32_t sources[] = {FUR, HAT};
    for (uint32_t i = 0; i < 2u; ++i) {
        fr_reactive_record_t record;
        ASSERT_TRUE(read_parcel(g, &body, sources[i], &record, err, sizeof(err)));
        fr_reactive_state_t hot = record.state;
        hot.ignition_threshold = .001f;
        double heat = fr_reactive_state_energy_at_temperature(&hot, 1150.f) - fr_reactive_state_energy(&hot);
        ASSERT_TRUE(fr_reactive_state_add_heat(&hot, heat, err, sizeof(err)) &&
                    fr_gpu_reactive_seed_body(g, body.binding.body_id, record.owner.key[2],
                                              record.owner.key[3], &hot, err, sizeof(err)));
    }
    for (uint32_t flip = 0; flip < 2u; ++flip) {
        pose.flip = flip;
        ASSERT_TRUE(fr_renderer_sprite_poses(g, &pose, 1, err, sizeof(err)));
        memset(gas, 0, DC_CHUNK_CELLS * sizeof(*gas));
        gas[OXYGEN_CELL] = air;
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g, 0, gas, volume, DC_CHUNK_CELLS, err, sizeof(err)));
        fr_reactive_record_t before, after, control_before, control_after;
        ASSERT_TRUE(read_parcel(g, &body, sources[flip], &before, err, sizeof(err)) &&
                    read_parcel(g, &body, sources[1u - flip], &control_before, err, sizeof(err)) &&
                    fr_combustion_gpu_step(g, .25f, err, sizeof(err)) &&
                    read_parcel(g, &body, sources[flip], &after, err, sizeof(err)) &&
                    read_parcel(g, &body, sources[1u - flip], &control_after, err, sizeof(err)) &&
                    fr_gas_gpu_read_chunk(g, 0, gas, DC_CHUNK_CELLS, err, sizeof(err)) &&
                    fr_combustion_gpu_read_emission_chunk(g, 0, emission, DC_CHUNK_CELLS, err, sizeof(err)));
        const uint32_t reacted = before.state.fuel_mass_fp - after.state.fuel_mass_fp;
        const double used_fp = (oxygen - gas[OXYGEN_CELL].species[FR_GAS_OXYGEN]) * DC_FLUID_FULL;
        const fr_reactive_profile_t *profile = fr_reactive_profile(before.state.material);
        const double reserved_fp = fr_reactive_state_pending_oxygen_fp(&after.state) -
                                   fr_reactive_state_pending_oxygen_fp(&before.state);
        printf("Posed reaction %u reacted %u fuel, used %.6f oxygen, emitted %.6f power\n",
               flip, reacted, used_fp, emission[OXYGEN_CELL].power);
        ASSERT_TRUE(reacted > 0u && after.state.char_mass_fp > before.state.char_mass_fp);
        ASSERT_TRUE(fabs(used_fp - reacted * profile->oxygen_per_fuel - reserved_fp) < .001);
        ASSERT_TRUE(emission[OXYGEN_CELL].power > 0.f &&
                    (emission[OXYGEN_CELL].sources & FR_FIRE_EMISSION_SURFACE) != 0u);
        ASSERT_EQ(before.state.parcel_id_low, after.state.parcel_id_low);
        ASSERT_EQ(before.state.parcel_id_high, after.state.parcel_id_high);
        ASSERT_EQ(control_before.state.fuel_mass_fp, control_after.state.fuel_mass_fp);
        ASSERT_EQ(control_before.state.char_mass_fp, control_after.state.char_mass_fp);
    }
    pose.offset = POSE_WIDTH;
    pose.flip = 0u;
    ASSERT_TRUE(fr_renderer_sprite_poses(g, &pose, 1, err, sizeof(err)));
    fr_reactive_record_t before, after;
    ASSERT_TRUE(read_parcel(g, &body, FUR, &before, err, sizeof(err)));
    memset(gas, 0, DC_CHUNK_CELLS * sizeof(*gas));
    gas[OXYGEN_CELL] = air;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g, 0, gas, volume, DC_CHUNK_CELLS, err, sizeof(err)) &&
                fr_combustion_gpu_step(g, .25f, err, sizeof(err)) &&
                read_parcel(g, &body, FUR, &after, err, sizeof(err)));
    ASSERT_EQ(before.state.fuel_mass_fp, after.state.fuel_mass_fp);
    ASSERT_EQ(before.state.char_mass_fp, after.state.char_mass_fp);
    ASSERT_EQ(before.state.ignition_integral, after.state.ignition_integral);
    free(emission);
    free(volume);
    free(gas);
    dc_gpu_destroy(g);
    PASS();
}

int main(void) {
    RUN(test_canonical_body_admits_hidden_owners_once);
    RUN(test_wetting_follows_visible_owner_when_a_sprite_is_mirrored);
    RUN(test_terrain_heat_reaches_visible_owner_and_balances_source);
    RUN(test_visible_combustion_uses_local_oxygen_and_leaves_hidden_fuel);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
