#include <string.h>

#include "../vulkan/gpu_internal.h"
#include "offscreen_internal.h"

typedef struct {
    fr_structural_gpu_graph_t graph;
    uint32_t nodes[FR_STRUCTURAL_NODE_CAPACITY][DC_STRUCTURAL_NODE_STATE_WORDS];
    uint32_t members[FR_STRUCTURAL_MEMBER_CAPACITY][16];
    uint32_t components[FR_STRUCTURAL_NODE_CAPACITY][16];
    uint32_t clocks[FR_STRUCTURAL_NODE_CAPACITY][4];
    uint32_t anchors[FR_STRUCTURAL_ANCHOR_CAPACITY], joints[FR_STRUCTURAL_JOINT_CAPACITY];
} graph_state_t;

static void copy_state(dc_gpu_t *gpu, graph_state_t *state, bool publish) {
    uint32_t *base = dc_gpu_structural_data(gpu) + dc_gpu_structural_graph_offset(gpu);
    const struct { void *records; uint32_t offset; size_t bytes; } ranges[] = {
        {&state->graph, 0, sizeof(state->graph)},
        {state->nodes, 2 * DC_STRUCTURAL_GRAPH_WORDS, sizeof(state->nodes)},
        {state->members, 24000, sizeof(state->members)}, {state->components, 24400, sizeof(state->components)},
        {state->clocks, 28000, sizeof(state->clocks)}, {state->anchors, 28300, sizeof(state->anchors)},
        {state->joints, 28300 + FR_STRUCTURAL_ANCHOR_CAPACITY, sizeof(state->joints)}
    };
    for (unsigned i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i) {
        if (publish) memcpy(base + ranges[i].offset, ranges[i].records, ranges[i].bytes);
        else memcpy(ranges[i].records, base + ranges[i].offset, ranges[i].bytes);
    }
    if (publish) {
        for (unsigned i = 0; i < state->graph.member_count; ++i) {
            fr_structural_gpu_member_t *m = &state->graph.members[i];
            if (!(m->flags & 2u)) dc_gpu_body_record(gpu, m->reserved)->reserved[1] = i + 1u;
        }
        dc_gpu_structural_data(gpu)[0] = state->graph.member_count != 0;
    }
}

/* All four native graph record types start with their stable 32-bit ID. */
static uint32_t index_of(const void *records, size_t stride, uint32_t count, uint32_t id) {
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t key;
        memcpy(&key, (const char *)records + i * stride, sizeof(key));
        if (key == id) return i;
    }
    return count;
}

static bool append(void *records, size_t stride, uint32_t *count, uint32_t capacity, const void *record) {
    uint32_t id;
    memcpy(&id, record, sizeof(id));
    if (*count >= capacity || index_of(records, stride, *count, id) != *count) return false;
    memcpy((char *)records + *count * stride, record, stride);
    ++*count;
    return true;
}

enum { STAYS = 1u, MOVES = 2u };

bool dc_offscreen_partition_graph(dc_gpu_t *from, dc_gpu_t *to, const dc_offscreen_bond_group_t *group,
    const uint32_t *dest_slots) {
    graph_state_t source, remaining = {0}, destination;
    copy_state(from, &source, false);
    copy_state(to, &destination, false);
    const fr_structural_gpu_graph_t *g = &source.graph;
    uint32_t owners[FR_STRUCTURAL_NODE_CAPACITY] = {0}, moved = 0;
    for (unsigned i = 0; i < g->member_count; ++i) {
        fr_structural_gpu_member_t m = g->members[i];
        unsigned j = 0;
        while (j < group->member_count && group->members[j].id != FR_STRUCTURAL_BODY_ID_BASE + m.id) ++j;
        bool moving = j < group->member_count;
        if (moving) {
            if (!(m.flags & 2u) || m.reserved != group->members[j].slot ||
                m.generation != group->members[j].member_generation) return false;
            m.reserved = dest_slots[j];
            ++moved;
        }
        graph_state_t *out = moving ? &destination : &remaining;
        unsigned at = out->graph.member_count;
        if (!append(out->graph.members, sizeof(m), &out->graph.member_count, FR_STRUCTURAL_MEMBER_CAPACITY, &m)) return false;
        memcpy(out->members[at], source.members[i], sizeof(out->members[at]));
        unsigned a = index_of(g->nodes, sizeof(*g->nodes), g->node_count, m.a);
        unsigned b = index_of(g->nodes, sizeof(*g->nodes), g->node_count, m.b);
        if (a == g->node_count || b == g->node_count) return false;
        owners[a] |= moving ? MOVES : STAYS;
        owners[b] |= moving ? MOVES : STAYS;
    }
    if (moved != group->member_count) return false;
    for (unsigned pass = 0; pass < g->node_count; ++pass) {
        bool changed = false;
        for (unsigned i = 0; i < g->joint_count; ++i) {
            unsigned a = index_of(g->nodes, sizeof(*g->nodes), g->node_count, g->joints[i].a);
            unsigned b = index_of(g->nodes, sizeof(*g->nodes), g->node_count, g->joints[i].b);
            if (a == g->node_count || b == g->node_count) return false;
            changed |= owners[a] != owners[b];
            owners[a] = owners[b] = owners[a] | owners[b];
        }
        if (!changed) break;
    }
    for (unsigned i = 0; i < g->node_count; ++i) {
        if (owners[i] == (STAYS | MOVES)) return false;
        graph_state_t *out = owners[i] == MOVES ? &destination : &remaining;
        unsigned at = out->graph.node_count;
        if (!append(out->graph.nodes, sizeof(*g->nodes), &out->graph.node_count,
                    FR_STRUCTURAL_NODE_CAPACITY, &g->nodes[i])) return false;
        memcpy(out->nodes[at], source.nodes[i], sizeof(out->nodes[at]));
        memcpy(out->components[at], source.components[i], sizeof(out->components[at]));
        memcpy(out->clocks[at], source.clocks[i], sizeof(out->clocks[at]));
    }
    for (unsigned i = 0; i < g->joint_count; ++i) {
        unsigned a = index_of(g->nodes, sizeof(*g->nodes), g->node_count, g->joints[i].a);
        graph_state_t *out = owners[a] == MOVES ? &destination : &remaining;
        unsigned at = out->graph.joint_count;
        if (!append(out->graph.joints, sizeof(*g->joints), &out->graph.joint_count,
                    FR_STRUCTURAL_JOINT_CAPACITY, &g->joints[i])) return false;
        out->joints[at] = source.joints[i];
    }
    for (unsigned i = 0; i < g->anchor_count; ++i) {
        unsigned a = index_of(g->nodes, sizeof(*g->nodes), g->node_count, g->anchors[i].node);
        if (a == g->node_count) return false;
        graph_state_t *out = owners[a] == MOVES ? &destination : &remaining;
        unsigned at = out->graph.anchor_count;
        if (!append(out->graph.anchors, sizeof(*g->anchors), &out->graph.anchor_count,
                    FR_STRUCTURAL_ANCHOR_CAPACITY, &g->anchors[i])) return false;
        out->anchors[at] = source.anchors[i];
    }
    copy_state(from, &remaining, true);
    copy_state(to, &destination, true);
    return true;
}
