#include <math.h>
#include <string.h>

#include "dungeoncraft/material_density.h"
#include "structural_store_internal.h"

static bool floats_valid(const float *values, unsigned count) {
    for (unsigned i = 0; i < count; i++)
        if (!isfinite(values[i]) || values[i] < 0)
            return false;
    return true;
}

static int node_index(const fr_structural_gpu_graph_t *g, uint32_t id) {
    for (unsigned i = 0; i < g->node_count; i++)
        if (g->nodes[i].id == id)
            return (int)i;
    return -1;
}

static float material_density(uint32_t material) {
    return material == DC_GPU_BODY_DENSE_WOOD ? DC_DENSITY_DENSE_WOOD
           : material == DC_GPU_BODY_WOOD     ? DC_DENSITY_WOOD
           : material == DC_GPU_BODY_MORTAR   ? DC_DENSITY_MORTAR
                                              : DC_DENSITY_STONE;
}

static float composition_density(const dc_gpu_body_record_t *body) {
    if (!body->piece_count)
        return material_density(body->shape.material);
    double volume = 0, mass = 0;
    for (unsigned p = 0; p < body->piece_count; p++) {
        const dc_gpu_body_shape_t *shape = &body->pieces[p];
        double twice_area = 0;
        for (unsigned i = 0; i < shape->count; i++) {
            unsigned j = (i + 1) % shape->count;
            twice_area +=
                (double)shape->vertices[i].x_fp * shape->vertices[j].y_fp -
                (double)shape->vertices[i].y_fp * shape->vertices[j].x_fp;
        }
        double area = fabs(twice_area);
        volume += area;
        mass += area * material_density(shape->material);
    }
    return (float)(mass / volume);
}

static bool pixel_valid(const dc_gpu_body_record_t *b, const uint32_t *pixels,
                        uint32_t words, float *density) {
    if (!pixels || b->compound_reserved > words ||
        !dc_gpu_pixel_geometry_valid(b, pixels + b->compound_reserved,
                                     words - b->compound_reserved, density))
        return false;
    double mass = *density * (double)(b->piece_count & ~DC_GPU_PIXEL_BODY_FLAG);
    return !b->motion_ready || fabs(b->mass - mass) < fmax(.002, mass * 1e-6);
}

static bool body_valid(const dc_gpu_body_record_t *b, const uint32_t *pixels,
                       uint32_t words) {
    if (!b->body.active && !b->reserved[0])
        return true;
    bool pixel = (b->piece_count & DC_GPU_PIXEL_BODY_FLAG) != 0;
    float density = 0;
    if (!b->body.id || b->body.active > 1 || !b->body.width ||
        !b->body.height || (!pixel && b->body.width > DC_GPU_BODY_MAX_SIDE) ||
        (!pixel && b->body.height > DC_GPU_BODY_MAX_SIDE) ||
        b->local_x_fp < 0 || b->local_y_fp < 0 || b->local_x_fp >= 64 * 65536 ||
        b->local_y_fp >= 64 * 65536 || !isfinite(b->angle) ||
        fabsf(b->angle) > 3.141593f || !isfinite(b->angular_velocity) ||
        fabsf(b->angular_velocity) > .5f || (b->motion_flags & ~3u) ||
        b->motion_ready > 1 || (!pixel && b->piece_count > DC_GPU_BODY_PIECES))
        return false;
    if (b->reserved[0] != 0 && b->reserved[0] != DC_GPU_STRUCTURAL_TAG &&
        b->reserved[0] != DC_GPU_STRUCTURAL_CONVERTED_TAG)
        return false;
    if (b->reserved[0] && !b->reserved[1])
        return false;
    if (!b->reserved[0] && b->reserved[1])
        return false;
    if (b->shape.count > DC_GPU_CONVEX_VERTICES ||
        b->shape.material > DC_GPU_BODY_DENSE_WOOD)
        return false;
    if (pixel) {
        if (!pixel_valid(b, pixels, words, &density))
            return false;
    } else if (b->piece_count) {
        dc_gpu_compound_shape_t shape = {.count = b->piece_count};
        memcpy(shape.pieces, b->pieces, sizeof(shape.pieces));
        if (!dc_gpu_valid_compound_shape(&b->body, &shape))
            return false;
    } else if (b->shape.count && !dc_gpu_valid_body_shape(&b->body, &b->shape))
        return false;
    if (!isfinite(b->mass) || b->mass < 0 || !isfinite(b->inertia) ||
        b->inertia < 0)
        return false;
    if (b->motion_ready) {
        if (!pixel)
            density = composition_density(b);
        if (b->mass <= 0 || b->inertia <= 0 || !isfinite(b->density) ||
            fabsf(b->density - density) > 1e-6f ||
            !floats_valid(&b->inverse_mass, 2) || !isfinite(b->center[0]) ||
            !isfinite(b->center[1]) || !isfinite(b->radius) || b->radius <= 0)
            return false;
    }
    return true;
}

static bool graph_valid(const dc_structural_store_payload_t *p) {
    const fr_structural_gpu_graph_t *g = (const void *)p->graph;
    if (g->node_count > 32 || g->member_count > 16 || g->joint_count > 64 ||
        g->anchor_count > 32)
        return false;
    for (unsigned i = 0; i < 12; i++)
        if (g->reserved[i])
            return false;
    for (unsigned i = 0; i < g->node_count; i++) {
        const fr_structural_gpu_node_t *n = &g->nodes[i];
        if (!n->id || !n->generation || n->x_fp < 0 || n->y_fp < 0 ||
            n->x_fp >= 64 * 65536 || n->y_fp >= 64 * 65536 ||
            n->reserved[0] > 1 || n->reserved[1] || n->reserved[2] ||
            n->reserved[3])
            return false;
        for (unsigned j = 0; j < i; j++)
            if (g->nodes[j].id == n->id)
                return false;
    }
    for (unsigned i = 0; i < g->member_count; i++) {
        const fr_structural_gpu_member_t *m = &g->members[i];
        if (!m->id || m->id >= FR_STRUCTURAL_BODY_ID_BASE || !m->generation ||
            node_index(g, m->a) < 0 || node_index(g, m->b) < 0 ||
            m->a == m->b || m->material >= FR_STRUCTURAL_MATERIAL_COUNT ||
            !m->depth_fp || m->depth_fp > 16u * 65536u || (m->flags & ~2u) ||
            m->reserved >= DC_GPU_BODY_CAPACITY)
            return false;
        for (unsigned j = 0; j < i; j++)
            if (g->members[j].id == m->id)
                return false;
        if (!(m->flags & 2u)) {
            const dc_gpu_body_record_t *b = &p->bodies[m->reserved];
            uint32_t material =
                m->material == FR_STRUCTURAL_WOOD     ? DC_GPU_BODY_WOOD
                : m->material == FR_STRUCTURAL_MORTAR ? DC_GPU_BODY_MORTAR
                                                      : DC_GPU_BODY_STONE;
            if (!b->body.active ||
                b->body.id != FR_STRUCTURAL_BODY_ID_BASE + m->id ||
                b->reserved[0] != DC_GPU_STRUCTURAL_TAG ||
                b->reserved[1] != i + 1 || b->shape.material != material ||
                !(b->motion_flags & DC_GPU_BODY_KINEMATIC))
                return false;
        }
        fr_structural_gpu_member_state_t state;
        memcpy(&state, p->graph + 24000u + i * 16u, sizeof(state));
        if (state.id != m->id || state.generation != m->generation ||
            state.state > FR_STRUCTURAL_CONVERTED || !isfinite(state.damage) ||
            state.damage < 0 || state.damage > 1 || !isfinite(state.axial) ||
            !isfinite(state.shear) || !isfinite(state.moment) ||
            !floats_valid(&state.utilization, 6))
            return false;
    }
    for (unsigned i = 0; i < g->joint_count; i++) {
        const fr_structural_gpu_joint_t *j = &g->joints[i];
        if (!j->id || !j->generation || node_index(g, j->a) < 0 ||
            node_index(g, j->b) < 0 || j->a == j->b ||
            j->kind > FR_JOINT_MOMENT ||
            !floats_valid(&j->normal_stiffness, 8) ||
            !isfinite(j->normal_angle) || j->reserved[0] > 1 || j->reserved[1])
            return false;
        for (unsigned k = 0; k < i; k++)
            if (g->joints[k].id == j->id)
                return false;
    }
    for (unsigned i = 0; i < g->anchor_count; i++) {
        const fr_structural_gpu_anchor_t *a = &g->anchors[i];
        if (!a->id || !a->generation || node_index(g, a->node) < 0 ||
            a->kind > FR_JOINT_MOMENT || a->x >= 64 || a->y >= 64 ||
            !a->width || !a->height || a->width > 8 || a->height > 8 ||
            a->terrain_material != DC_MATERIAL_STONE ||
            !floats_valid(&a->normal_stiffness, 8) ||
            !isfinite(a->normal_angle) || a->reserved[0] > 1 || a->reserved[1])
            return false;
        for (unsigned k = 0; k < i; k++)
            if (g->anchors[k].id == a->id)
                return false;
    }
    return true;
}

bool dc_gpu_structure_payload_valid(const dc_gpu_t *gpu,
                                    const dc_structural_store_payload_t *p,
                                    const uint32_t *pixels, uint32_t words) {
    if (p->control[0] > 1 || p->control[2] > 192 || p->control[3] > 0x7fffffffu)
        return false;
    for (unsigned i = 4; i < 8; i++)
        if (p->control[i])
            return false;
    const fr_structural_gpu_graph_t *g = (const void *)p->graph;
    if (g->member_count > 16)
        return false;
    const uint32_t *current = dc_gpu_structural_data((dc_gpu_t *)gpu) +
                              dc_gpu_structural_graph_offset();
    if (memcmp(p->graph + 29000u, current + 29000u, 36 * sizeof(uint32_t)))
        return false;
    for (unsigned i = 0; i < DC_GPU_BODY_CAPACITY; i++) {
        const dc_gpu_body_record_t *b = &p->bodies[i];
        if (!body_valid(b, pixels, words))
            return false;
        if (b->reserved[0] == DC_GPU_STRUCTURAL_TAG) {
            unsigned member = b->reserved[1] - 1u;
            if (member >= g->member_count || g->members[member].reserved != i ||
                (g->members[member].flags & 2u))
                return false;
        }
        if (b->body.active || b->reserved[0])
            for (unsigned j = 0; j < i; j++)
                if ((p->bodies[j].body.active || p->bodies[j].reserved[0]) &&
                    p->bodies[j].body.id == b->body.id)
                    return false;
        const uint32_t *output = p->graph + 36000u + i * 4u;
        if (output[0] && (!output[1] || output[2] > p->control[3] ||
                          (output[3] != 1 && output[3] != 2)))
            return false;
        if (output[3] == 2 &&
            (b->body.active ||
             b->reserved[0] != DC_GPU_STRUCTURAL_CONVERTED_TAG ||
             output[0] != b->body.id || output[1] != b->reserved[1]))
            return false;
        const uint32_t *pending = p->graph + 38000u + i * 4u;
        if (pending[0] > 1000000 || pending[1] > 1000000 || pending[2] > 2)
            return false;
    }
    uint32_t maximum_ticks = 0;
    for (unsigned m = 0; m < FR_STRUCTURAL_MATERIAL_COUNT; m++) {
        float grace;
        memcpy(&grace, p->graph + 29000u + m * 12u + 7u, sizeof(grace));
        uint32_t ticks = (uint32_t)ceilf(grace * 60.0f);
        if (ticks > maximum_ticks)
            maximum_ticks = ticks;
    }
    for (unsigned i = 0; i < 32; i++) {
        const uint32_t *clock = p->graph + 28000u + i * 4u;
        if (clock[0] > maximum_ticks || clock[1] || clock[2] || clock[3])
            return false;
    }
    for (unsigned i = 0; i < 96; i++) {
        float damage;
        memcpy(&damage, p->graph + 28300u + i, sizeof(damage));
        if (!isfinite(damage) || damage < 0 || damage > 1)
            return false;
    }
    for (unsigned i = 0; i < 192; i++) {
        const uint32_t *bond = p->graph + 30000u + i * 20u;
        if (bond[0] > 1)
            return false;
        if (!bond[0])
            continue;
        if (bond[1] >= DC_GPU_BODY_CAPACITY ||
            bond[2] >= DC_GPU_BODY_CAPACITY || bond[5] > FR_JOINT_MOMENT ||
            p->bodies[bond[1]].body.id != bond[3] ||
            p->bodies[bond[2]].body.id != bond[4])
            return false;
        float fields[14];
        memcpy(fields, bond + 6, sizeof(fields));
        for (unsigned f = 0; f < 14; f++)
            if (!isfinite(fields[f]))
                return false;
    }
    return graph_valid(p);
}
