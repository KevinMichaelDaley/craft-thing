#ifndef FERRUM_STRUCTURAL_GPU_H
#define FERRUM_STRUCTURAL_GPU_H
#include "dungeoncraft/gpu.h"
#include "ferrum/structural/mechanics.h"

#define FR_STRUCTURAL_NODE_CAPACITY 32u
#define FR_STRUCTURAL_MEMBER_CAPACITY 16u
#define FR_STRUCTURAL_JOINT_CAPACITY 64u
#define FR_STRUCTURAL_ANCHOR_CAPACITY 32u
#define FR_STRUCTURAL_BODY_ID_BASE 0x40000000u
#define FR_STRUCTURAL_OUTPUT_ID_BASE 0x80000000u
typedef enum {
    FR_STRUCTURAL_SUPPORTED,
    FR_STRUCTURAL_OVERLOADED,
    FR_STRUCTURAL_UNSUPPORTED,
    FR_STRUCTURAL_UNKNOWN,
    FR_STRUCTURAL_PENDING_CONVERSION,
    FR_STRUCTURAL_CONVERTED
} fr_structural_state_t;
enum { FR_STRUCTURAL_MARK_TERRAIN = 1u };
enum {
    FR_STRUCTURAL_DIAG_COLD = 1u,
    FR_STRUCTURAL_DIAG_LOAD_OVERFLOW = 2u,
    FR_STRUCTURAL_DIAG_LOAD_CLIPPED = 4u,
    FR_STRUCTURAL_DIAG_NUMERICAL = 8u,
    FR_STRUCTURAL_DIAG_MECHANISM = 16u
};
typedef struct {
    uint32_t id, generation;
    dc_chunk_coord_t chunk;
    int32_t x_fp, y_fp;
    uint32_t reserved[4];
} fr_structural_gpu_node_t;
typedef struct {
    uint32_t id, generation, a, b, material, depth_fp, flags, reserved;
} fr_structural_gpu_member_t;
typedef struct {
    uint32_t id, generation, a, b, kind;
    float normal_stiffness, shear_stiffness, rotational_stiffness;
    float bearing_capacity, pullout_capacity, shear_capacity, moment_capacity;
    float friction;
    float normal_angle;
    uint32_t reserved[2];
} fr_structural_gpu_joint_t;
typedef struct {
    uint32_t id, generation, node, kind;
    dc_chunk_coord_t chunk;
    uint32_t x, y, width, height, terrain_material;
    float normal_stiffness, shear_stiffness, rotational_stiffness;
    float bearing_capacity, pullout_capacity, shear_capacity, moment_capacity,
        friction;
    float normal_angle;
    uint32_t reserved[2];
} fr_structural_gpu_anchor_t;
typedef struct {
    uint32_t node_count, member_count, joint_count, anchor_count;
    uint32_t reserved[12];
    fr_structural_gpu_node_t nodes[FR_STRUCTURAL_NODE_CAPACITY];
    fr_structural_gpu_member_t members[FR_STRUCTURAL_MEMBER_CAPACITY];
    fr_structural_gpu_joint_t joints[FR_STRUCTURAL_JOINT_CAPACITY];
    fr_structural_gpu_anchor_t anchors[FR_STRUCTURAL_ANCHOR_CAPACITY];
} fr_structural_gpu_graph_t;
typedef struct {
    uint32_t id, component, state, valid_anchors, unknown_anchors, generation;
    float displacement[3], reaction[3];
} fr_structural_gpu_node_state_t;
typedef struct {
    uint32_t id, generation, component, state;
    float axial, shear, moment, utilization, utilization_location;
    float strain_energy, damage;
    float fracture_work, dissipated_energy;
    uint32_t reserved[3];
} fr_structural_gpu_member_state_t;
typedef struct {
    uint32_t id, state, node_count, flags;
    float external[3], reaction[3], relative_residual, minimum_pivot;
    uint32_t iterations, active_iterations, refinement_iterations;
    float geometry_minimum;
} fr_structural_gpu_component_state_t;
/** Atomically place/replace an explicitly authored graph. IDs/generations are
 * stable. Initial geometry accepts unit-thickness rectangular members up to 16
 * cells. Marking accepts whole horizontal stone rectangles; unsupported shapes
 * are rejected. */
bool fr_gpu_place_structure(dc_gpu_t *gpu,
                            const fr_structural_gpu_graph_t *graph,
                            char *err_buf, uint32_t err_cap);
/** Bound future conversion bond reservations (0..192), preserving existing
 * bonds. Explicit authoring configuration; no frame-time readback. */
bool fr_gpu_set_structure_bond_capacity(dc_gpu_t *gpu, uint32_t capacity);
/** Explicit diagnostic GPU readback; ordinary frames do not traverse/read the
 * graph. */
bool fr_gpu_read_structure_node(dc_gpu_t *gpu, uint32_t id,
                                fr_structural_gpu_node_state_t *state);
/** Explicit diagnostic of recovered member stress and material utilization. */
bool fr_gpu_read_structure_member(dc_gpu_t *gpu, uint32_t id,
                                  fr_structural_gpu_member_state_t *state);
/** Explicit component diagnostic. Pass the component's minimum node ID;
 * moments are about that node's rest position. Inspect state before using
 * equilibrium quantities, which are current only for SUPPORTED/OVERLOADED.
 * Converted records retain historical diagnostics rather than current equilibrium.
 */
bool fr_gpu_read_structure_component(
    dc_gpu_t *gpu, uint32_t id, fr_structural_gpu_component_state_t *state);
#endif
