#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/structural/gpu.h"

#include "catapult_scene.h"

enum {
    CATAPULT_ARM_ID = 0x80000001u,
    CATAPULT_PIVOT_ID = 1u,
    CATAPULT_LATCH_ID = 2u,
    CATAPULT_COAL_ID = 0x80000002u,
    CATAPULT_COAL_SLOT = 3u
};

static bool scene_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static fr_structural_gpu_graph_t support_graph(void) {
    fr_structural_gpu_graph_t graph = {.node_count = 4, .member_count = 2,
                                       .anchor_count = 2};
    graph.nodes[0] = (fr_structural_gpu_node_t){.id = 1, .generation = 1,
        .chunk = {1, 1}, .x_fp = 0, .y_fp = 32 * 65536};
    graph.nodes[1] = (fr_structural_gpu_node_t){.id = 2, .generation = 1,
        .chunk = {1, 1}, .x_fp = 0, .y_fp = 16 * 65536};
    graph.nodes[2] = (fr_structural_gpu_node_t){.id = 3, .generation = 1,
        .chunk = {1, 1}, .x_fp = 14 * 65536, .y_fp = 32 * 65536};
    graph.nodes[3] = (fr_structural_gpu_node_t){.id = 4, .generation = 1,
        .chunk = {1, 1}, .x_fp = 14 * 65536, .y_fp = 18 * 65536};
    graph.members[0] = (fr_structural_gpu_member_t){.id = 1, .generation = 1,
        .a = 1, .b = 2, .material = FR_STRUCTURAL_WOOD, .depth_fp = 4 * 65536};
    graph.members[1] = (fr_structural_gpu_member_t){.id = 2, .generation = 1,
        .a = 3, .b = 4, .material = FR_STRUCTURAL_STONE,
        .depth_fp = 4 * 65536};
    graph.anchors[0] = (fr_structural_gpu_anchor_t){
        .id = 1, .generation = 1, .node = 1, .kind = FR_JOINT_MOMENT,
        .chunk = {1, 1}, .x = 0, .y = 32, .width = 2, .height = 1,
        .terrain_material = DC_MATERIAL_STONE,
        .normal_stiffness = 1e8f, .shear_stiffness = 1e8f,
        .rotational_stiffness = 1e8f, .bearing_capacity = 1e7f,
        .pullout_capacity = 1e7f, .shear_capacity = 1e7f,
        .moment_capacity = 1e7f, .friction = .6f,
        .normal_angle = -1.57079632679f};
    graph.anchors[1] = graph.anchors[0];
    graph.anchors[1].id = 2;
    graph.anchors[1].node = 3;
    graph.anchors[1].x = 14;
    return graph;
}

static bool coal_load(dc_gpu_t *gpu, bool hot, char *err, uint32_t cap) {
    if (!dc_gpu_paint_material(gpu, 70, 76, 1, DC_MATERIAL_COAL, err, cap))
        return false;
    const fr_reactive_profile_t *profile = fr_reactive_profile(DC_MATERIAL_COAL);
    if (!profile) return scene_error(err, cap, "Missing coal profile");
    uint32_t mass = (uint32_t)lroundf(profile->density * 65536.f);
    for (uint32_t y = 11; y < 14; y++)
        for (uint32_t x = 5; x < 8; x++) {
            if ((x == 5 || x == 7) && y != 12) continue;
            if (y != 12 && x != 6) continue;
            fr_reactive_state_t state;
            uint32_t local = y * DC_CHUNK_SIDE + x;
            if (!fr_reactive_state_default(DC_MATERIAL_COAL, mass, 0,
                    hot ? 1300.f : 293.15f, 0xC0000000u + local,
                    &state, err, cap)) return false;
            if (hot) state.flags |= FR_REACTIVE_FLAMING;
            if (!fr_gpu_reactive_seed_cell(gpu, CATAPULT_COAL_SLOT, local,
                                            &state, err, cap)) return false;
        }
    return true;
}

static bool wooden_arm(dc_gpu_t *gpu, char *err, uint32_t cap) {
    const fr_reactive_profile_t *profile = fr_reactive_profile(DC_MATERIAL_WOOD);
    if (!profile) return scene_error(err, cap, "Missing wood profile");
    uint32_t mass = (uint32_t)lroundf(profile->density * 65536.f);
    for (uint32_t y = 78; y < 80; y++)
        for (uint32_t x = 56; x < 72; x++) {
            uint32_t slot = 2u + x / DC_CHUNK_SIDE;
            uint32_t local = (y - DC_CHUNK_SIDE) * DC_CHUNK_SIDE +
                             x % DC_CHUNK_SIDE;
            fr_reactive_state_t state;
            if (!dc_gpu_paint_material(gpu, x, y, 0, DC_MATERIAL_WOOD,
                                       err, cap) ||
                !fr_reactive_state_default(DC_MATERIAL_WOOD, mass, 0,
                    293.15f, 0xA0000000u + ((uint64_t)y << 16) + x,
                    &state, err, cap) ||
                !fr_gpu_reactive_seed_cell(gpu, slot, local, &state,
                                            err, cap)) return false;
        }
    dc_chunk_colors_t *colors = calloc(1, sizeof(*colors));
    if (!colors) return scene_error(err, cap, "Cannot allocate arm color map");
    bool colored = true;
    for (uint32_t slot = 2; slot <= 3 && colored; slot++) {
        colored = fr_renderer_download_chunk_colors(gpu, slot, colors,
                                                    err, cap);
        for (uint32_t y = 78; y < 80 && colored; y++)
            for (uint32_t x = slot == 2 ? 56u : 64u;
                 x < (slot == 2 ? 64u : 72u); x++) {
                uint32_t local = (y - 64u) * DC_CHUNK_SIDE + x % 64u;
                colors->terrain[local][0] = .58f;
                colors->terrain[local][1] = .33f;
                colors->terrain[local][2] = .14f;
                colors->terrain[local][3] = 1.f;
            }
        if (colored) colored = fr_renderer_upload_chunk_colors(gpu, slot,
                                                               colors, err, cap);
    }
    free(colors);
    if (!colored) return false;
    return dc_gpu_admit_authored_rigid_cells(gpu, err, cap);
}

bool dc_catapult_scene_build(dc_gpu_t *gpu, bool hot,
                             dc_catapult_scene_t *scene,
                             char *err_buf, uint32_t err_cap) {
    if (!gpu || !scene) return scene_error(err_buf, err_cap,
                                           "Missing catapult context or output");
    fr_structural_gpu_graph_t graph = support_graph();
    if (!fr_thermal_gpu_enable(gpu, err_buf, err_cap) ||
        !fr_gpu_place_structure(gpu, &graph, err_buf, err_cap) ||
        !dc_gpu_set_rigid_solver(gpu, true)) return false;
    for (uint32_t tick = 0; tick < 5; tick++)
        if (!dc_gpu_rigid_step(gpu, err_buf, err_cap)) return false;
    if (!wooden_arm(gpu, err_buf, err_cap) ||
        !coal_load(gpu, hot, err_buf, err_cap) ||
        !dc_gpu_admit_authored_rigid_cells(gpu, err_buf, err_cap)) return false;
    if (!dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0},
                                 err_buf, err_cap)) return false;
    fr_revolute_joint_t pivot = {.id = CATAPULT_PIVOT_ID,
        .body_a = FR_STRUCTURAL_BODY_ID_BASE + 1u, .body_b = CATAPULT_ARM_ID,
        .anchor_a = {8.f, 0.f}, .anchor_b = {0.f, 0.f},
        .flags = FR_REVOLUTE_SPRING | FR_REVOLUTE_LIMITS,
        .lower_angle = .3f, .upper_angle = 1.9f,
        .spring_stiffness = 200.f, .spring_rest_angle = .5f};
    fr_revolute_joint_t latch = {.id = CATAPULT_LATCH_ID,
        .body_a = CATAPULT_ARM_ID, .body_b = CATAPULT_COAL_ID,
        .anchor_a = {13.f, -3.f}, .anchor_b = {0.f, 0.f}};
    if (!fr_gpu_revolute_add(gpu, &pivot, err_buf, err_cap) ||
        !fr_gpu_revolute_add(gpu, &latch, err_buf, err_cap)) return false;
    fr_gas_model_t gas = {.gas_constant = {.25984, .29680, .18892,
                                           .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
        .kinetic_scale = 1. / 512.};
    if (!fr_gas_gpu_enable(gpu, &gas, .0012f, 293.15f, err_buf, err_cap) ||
        !fr_combustion_gpu_enable(gpu, err_buf, err_cap)) return false;
    *scene = (dc_catapult_scene_t){.frame_body = pivot.body_a,
        .wall_body = FR_STRUCTURAL_BODY_ID_BASE + 2u,
        .arm_body = pivot.body_b, .coal_body = latch.body_b,
        .pivot_joint = pivot.id, .latch_joint = latch.id};
    return true;
}

bool dc_catapult_scene_release(dc_gpu_t *gpu, dc_catapult_scene_t *scene,
                               char *err_buf, uint32_t err_cap) {
    if (!gpu || !scene || scene->released || !scene->latch_joint)
        return scene_error(err_buf, err_cap, "Invalid catapult latch release");
    if (!fr_gpu_revolute_remove(gpu, scene->latch_joint, err_buf, err_cap))
        return false;
    scene->released = true;
    return true;
}

bool dc_catapult_scene_release_projectile(dc_gpu_t *gpu,
                                          dc_catapult_scene_t *scene,
                                          char *err_buf, uint32_t err_cap) {
    (void)gpu;
    (void)scene;
    return scene_error(err_buf, err_cap, "Projectile release not implemented");
}
