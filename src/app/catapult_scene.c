#include <math.h>
#include <stdio.h>

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/structural/gpu.h"

#include "catapult_scene.h"

enum {
    CATAPULT_ARM_ID = 0x80000001u,
    CATAPULT_PIVOT_ID = 1u,
    CATAPULT_LATCH_ID = 2u,
    CATAPULT_POUCH_ID = 3u,
    CATAPULT_COAL_ID = 0x80000002u,
    CATAPULT_BASE_ID = 0x80000003u,
    CATAPULT_COAL_SLOT = 5u,
    CATAPULT_WALL_X = 180u,
    CATAPULT_FLOOR_Y = 96u,
    CATAPULT_BASE_HEIGHT = 16u,
    CATAPULT_WALL_TOP_Y = 68u,
    CATAPULT_BATTLEMENT_LEFT = 198u,
    CATAPULT_BATTLEMENT_RIGHT = 240u,
    CATAPULT_BATTLEMENT_TOP_Y = 87u,
    CATAPULT_BATTLEMENT_DECK_Y = 95u,
    CATAPULT_BATTLEMENT_WOOD_MASS_DIVISOR = 10u
};

static const float CATAPULT_COCKED_ANGLE = -2.45f;
static const float CATAPULT_FRAME_ANGLE = -1.57079632679f;
static const float CATAPULT_BATTLEMENT_IGNITION_THRESHOLD = .005f;

static bool scene_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static fr_structural_gpu_node_t scene_node(uint32_t id, uint32_t x,
                                           uint32_t y) {
    return (fr_structural_gpu_node_t){.id = id, .generation = 1,
        .chunk = {(int32_t)(x / DC_CHUNK_SIDE), (int32_t)(y / DC_CHUNK_SIDE)},
        .x_fp = (int32_t)(x % DC_CHUNK_SIDE) * 65536,
        .y_fp = (int32_t)(y % DC_CHUNK_SIDE) * 65536};
}

static fr_structural_gpu_member_t scene_member(uint32_t id, uint32_t a,
                                                uint32_t b, uint32_t material,
                                                uint32_t depth) {
    return (fr_structural_gpu_member_t){.id = id, .generation = 1,
        .a = a, .b = b, .material = material, .depth_fp = depth * 65536};
}

static fr_structural_gpu_graph_t support_graph(void) {
    fr_structural_gpu_graph_t graph = {.node_count = 21, .member_count = 12,
                                       .anchor_count = 9};
    graph.nodes[0] = scene_node(1, 72, CATAPULT_FLOOR_Y);
    graph.nodes[1] = scene_node(2, 72, 80);
    graph.nodes[2] = scene_node(3, CATAPULT_WALL_X, CATAPULT_FLOOR_Y);
    graph.nodes[3] = scene_node(4, CATAPULT_WALL_X,
                                CATAPULT_WALL_TOP_Y + 16u);
    graph.nodes[4] = scene_node(5, CATAPULT_WALL_X, CATAPULT_WALL_TOP_Y);
    graph.nodes[5] = scene_node(6, 72, 64);
    graph.nodes[6] = scene_node(7, 72, 56);
    const uint32_t deck_x[] = {198u, 207u, 209u, 215u,
                               217u, 233u, 235u, 240u};
    for (uint32_t i = 0; i < 8u; i++)
        graph.nodes[7u + i] = scene_node(8u + i, deck_x[i],
                                          CATAPULT_BATTLEMENT_DECK_Y);
    const uint32_t post_x[] = {208u, 216u, 234u};
    for (uint32_t i = 0; i < 3u; i++) {
        graph.nodes[15u + i * 2u] = scene_node(16u + i * 2u, post_x[i],
                                                CATAPULT_BATTLEMENT_DECK_Y);
        graph.nodes[16u + i * 2u] = scene_node(17u + i * 2u, post_x[i],
                                                CATAPULT_BATTLEMENT_TOP_Y);
    }
    graph.members[0] = scene_member(1, 1, 2, FR_STRUCTURAL_WOOD, 4);
    graph.members[1] = scene_member(2, 3, 4, FR_STRUCTURAL_STONE, 4);
    graph.members[2] = scene_member(3, 4, 5, FR_STRUCTURAL_STONE, 4);
    graph.members[3] = scene_member(4, 2, 6, FR_STRUCTURAL_WOOD, 4);
    graph.members[4] = scene_member(5, 6, 7, FR_STRUCTURAL_WOOD, 4);
    for (uint32_t i = 0; i < 4u; i++)
        graph.members[5u + i] = scene_member(6u + i, 8u + i * 2u,
                                              9u + i * 2u,
                                              FR_STRUCTURAL_WOOD, 2);
    for (uint32_t i = 0; i < 3u; i++)
        graph.members[9u + i] = scene_member(10u + i, 16u + i * 2u,
                                              17u + i * 2u,
                                              FR_STRUCTURAL_WOOD, 2);
    graph.anchors[0] = (fr_structural_gpu_anchor_t){
        .id = 1, .generation = 1, .node = 1, .kind = FR_JOINT_MOMENT,
        .chunk = {1, 1}, .x = 8, .y = 32, .width = 2, .height = 1,
        .terrain_material = DC_MATERIAL_STONE,
        .normal_stiffness = 1e8f, .shear_stiffness = 1e8f,
        .rotational_stiffness = 1e8f, .bearing_capacity = 1e7f,
        .pullout_capacity = 1e7f, .shear_capacity = 1e7f,
        .moment_capacity = 1e7f, .friction = .6f,
        .normal_angle = -1.57079632679f};
    graph.anchors[1] = graph.anchors[0];
    graph.anchors[1].id = 2;
    graph.anchors[1].node = 3;
    graph.anchors[1].chunk.x = CATAPULT_WALL_X / DC_CHUNK_SIDE;
    graph.anchors[1].x = CATAPULT_WALL_X % DC_CHUNK_SIDE;
    const uint32_t anchor_nodes[] = {8u, 10u, 12u, 14u,
                                     16u, 18u, 20u};
    const uint32_t anchor_x[] = {198u, 209u, 217u, 235u,
                                 208u, 216u, 234u};
    for (uint32_t i = 0; i < 7u; i++) {
        graph.anchors[2u + i] = graph.anchors[0];
        graph.anchors[2u + i].id = 3u + i;
        graph.anchors[2u + i].node = anchor_nodes[i];
        graph.anchors[2u + i].chunk.x = anchor_x[i] / DC_CHUNK_SIDE;
        graph.anchors[2u + i].x = anchor_x[i] % DC_CHUNK_SIDE;
    }
    return graph;
}

static bool triangular_base(dc_gpu_t *gpu, char *err, uint32_t cap) {
    dc_gpu_world_body_t base = {.chunk = {1, 1}, .body = {
        .id = CATAPULT_BASE_ID, .x_fp = 0,
        .y_fp = (CATAPULT_FLOOR_Y - DC_CHUNK_SIDE - CATAPULT_BASE_HEIGHT) * 65536,
        .width = 16, .height = CATAPULT_BASE_HEIGHT, .active = 1}};
    dc_gpu_body_shape_t shape = {.count = 3, .material = DC_GPU_BODY_WOOD,
        .vertices = {{.x_fp = 0, .y_fp = CATAPULT_BASE_HEIGHT * 65536},
                     {.x_fp = 8 * 65536, .y_fp = 0},
                     {.x_fp = 16 * 65536,
                      .y_fp = CATAPULT_BASE_HEIGHT * 65536}}};
    return dc_gpu_spawn_convex_body(gpu, base, &shape, err, cap) &&
           dc_gpu_set_body_motion(gpu, CATAPULT_BASE_ID, 0.f, 0.f,
                                  DC_GPU_BODY_KINEMATIC, err, cap);
}

static bool coal_load(dc_gpu_t *gpu, bool hot, char *err, uint32_t cap) {
    if (!dc_gpu_paint_material(gpu, 65, 65, 1, DC_MATERIAL_COAL, err, cap))
        return false;
    const fr_reactive_profile_t *profile = fr_reactive_profile(DC_MATERIAL_COAL);
    if (!profile) return scene_error(err, cap, "Missing coal profile");
    uint32_t mass = (uint32_t)lroundf(profile->density * 65536.f);
    for (uint32_t y = 0; y < 3; y++)
        for (uint32_t x = 0; x < 3; x++) {
            if ((x == 0 || x == 2) && y != 1) continue;
            if (y != 1 && x != 1) continue;
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
        for (uint32_t x = 64; x < 80; x++) {
            uint32_t slot = 4u + x / DC_CHUNK_SIDE;
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
    return dc_gpu_admit_authored_rigid_cells(gpu, err, cap);
}

static bool thin_battlement_wood(dc_gpu_t *gpu, char *err, uint32_t cap) {
    /* The rails are narrow slats within their two-cell collision envelopes. */
    for (uint32_t member = 6u; member <= 12u; member++) {
        uint32_t id = FR_STRUCTURAL_BODY_ID_BASE + member;
        dc_gpu_world_body_t body;
        if (!dc_gpu_read_world_body(gpu, id, &body, err, cap)) return false;
        for (uint32_t y = 0; y < body.body.height; y++)
            for (uint32_t x = 0; x < body.body.width; x++) {
                fr_reactive_record_t record;
                bool found = false;
                if (!fr_gpu_reactive_read_body(gpu, id, x, y, &record,
                                                &found, err, cap)) return false;
                if (!found) continue;
                uint64_t parcel = ((uint64_t)record.state.parcel_id_high << 32) |
                                   record.state.parcel_id_low;
                uint32_t mass = record.state.dry_mass_fp /
                                CATAPULT_BATTLEMENT_WOOD_MASS_DIVISOR;
                if (!mass) mass = 1u;
                fr_reactive_state_t state;
                if (!fr_reactive_state_default(DC_MATERIAL_WOOD, mass, 0u,
                                                293.15f, parcel, &state,
                                                err, cap)) return false;
                state.ignition_threshold =
                    CATAPULT_BATTLEMENT_IGNITION_THRESHOLD;
                if (!fr_gpu_reactive_seed_body(gpu, id, x, y, &state,
                                                err, cap)) return false;
            }
    }
    return true;
}

bool dc_catapult_scene_build(dc_gpu_t *gpu, bool hot,
                             dc_catapult_scene_t *scene,
                             char *err_buf, uint32_t err_cap) {
    if (!gpu || !scene) return scene_error(err_buf, err_cap,
                                           "Missing catapult context or output");
    fr_structural_gpu_graph_t graph = support_graph();
    if (!fr_thermal_gpu_enable(gpu, err_buf, err_cap) ||
        !fr_thermal_gpu_overlay(gpu, 0) ||
        !fr_gpu_place_structure(gpu, &graph, err_buf, err_cap) ||
        !fr_thermal_gpu_enable(gpu, err_buf, err_cap) ||
        !thin_battlement_wood(gpu, err_buf, err_cap) ||
        !dc_gpu_set_rigid_solver(gpu, true)) return false;
    for (uint32_t tick = 0; tick < 5; tick++)
        if (!dc_gpu_rigid_step(gpu, err_buf, err_cap)) return false;
    if (!triangular_base(gpu, err_buf, err_cap)) return false;
    if (!wooden_arm(gpu, err_buf, err_cap) ||
        !dc_gpu_set_body_motion(gpu, CATAPULT_ARM_ID, CATAPULT_COCKED_ANGLE,
                                0.f, 0,
                                err_buf, err_cap) ||
        !coal_load(gpu, hot, err_buf, err_cap) ||
        !dc_gpu_admit_authored_rigid_cells(gpu, err_buf, err_cap)) return false;
    if (!dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0},
                                 err_buf, err_cap)) return false;
    fr_revolute_joint_t pivot = {.id = CATAPULT_PIVOT_ID,
        .body_a = FR_STRUCTURAL_BODY_ID_BASE + 5u, .body_b = CATAPULT_ARM_ID,
        .anchor_a = {4.f, 0.f}, .anchor_b = {0.f, 0.f},
        .flags = FR_REVOLUTE_SPRING | FR_REVOLUTE_LIMITS,
        .lower_angle = -1.2f, .upper_angle = 1.1f,
        .spring_stiffness = 20000.f, .spring_rest_angle = .6f};
    float held_angle = CATAPULT_COCKED_ANGLE - CATAPULT_FRAME_ANGLE;
    fr_revolute_joint_t latch = {.id = CATAPULT_LATCH_ID,
        .body_a = pivot.body_a, .body_b = CATAPULT_ARM_ID,
        .anchor_a = {8.f, 0.f}, .anchor_b = {0.f, 0.f},
        .flags = FR_REVOLUTE_LIMITS | FR_REVOLUTE_SPRING,
        .lower_angle = held_angle, .upper_angle = held_angle,
        .spring_stiffness = 20000.f,
        .spring_rest_angle = 2.f * held_angle - pivot.spring_rest_angle};
    fr_revolute_joint_t pouch = {.id = CATAPULT_POUCH_ID,
        .body_a = CATAPULT_ARM_ID, .body_b = CATAPULT_COAL_ID,
        .anchor_a = {36.f, 0.f}, .anchor_b = {0.f, 0.f}};
    if (!fr_gpu_revolute_add(gpu, &pivot, err_buf, err_cap) ||
        !fr_gpu_revolute_add(gpu, &latch, err_buf, err_cap) ||
        !fr_gpu_revolute_add(gpu, &pouch, err_buf, err_cap)) return false;
    fr_gas_model_t gas = {.gas_constant = {.25984, .29680, .18892,
                                           .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
        .kinetic_scale = 1. / 512.};
    if (!fr_gas_gpu_enable(gpu, &gas, .0012f, 293.15f, err_buf, err_cap) ||
        !fr_combustion_gpu_enable(gpu, err_buf, err_cap)) return false;
    *scene = (dc_catapult_scene_t){.frame_body = FR_STRUCTURAL_BODY_ID_BASE + 1u,
        .wall_body = FR_STRUCTURAL_BODY_ID_BASE + 2u,
        .wall_upper_body = FR_STRUCTURAL_BODY_ID_BASE + 3u,
        .base_body = CATAPULT_BASE_ID,
        .arm_body = pivot.body_b, .coal_body = pouch.body_b,
        .pivot_joint = pivot.id, .latch_joint = latch.id,
        .pouch_joint = pouch.id};
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
    if (!gpu || !scene || !scene->released || scene->projectile_released ||
        !scene->pouch_joint)
        return scene_error(err_buf, err_cap, "Invalid coal pouch release");
    if (!fr_gpu_revolute_remove(gpu, scene->pouch_joint, err_buf, err_cap))
        return false;
    scene->projectile_released = true;
    return true;
}
