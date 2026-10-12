#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void
test_explicit_mortar_has_its_own_density_and_material_identity(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    dc_gpu_body_shape_t shape;
    uint32_t id = FR_STRUCTURAL_BODY_ID_BASE + 1;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, id, &motion, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_shape(g, id, &shape, err, sizeof(err)));
    ASSERT_INT_EQ(shape.material, DC_GPU_BODY_MORTAR);
    ASSERT_TRUE(fabs(motion.mass - 38.4) < .001);
    fr_structural_material_t material;
    ASSERT_TRUE(fr_structural_material_map(false, shape.material, &material));
    ASSERT_INT_EQ(material, FR_STRUCTURAL_MORTAR);
    ASSERT_TRUE(!fr_structural_material_map(true, DC_MATERIAL_DIRT, &material));
    ASSERT_TRUE(!fr_structural_material_map(true, DC_MATERIAL_SAND, &material));
    dc_gpu_destroy(g);
    PASS();
}

static void test_overloaded_mortar_crumble_preserves_mass_without_launch(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    bool advanced = dc_gpu_rigid_step(g, err, sizeof(err));
    if (!advanced) printf("rigid step: %s\n", err);
    ASSERT_TRUE(advanced);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    uint64_t mass = 0;
    int64_t momentum_x = 0, momentum_y = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        const dc_mpm_particle_t *p = &chunk->particles[i];
        if (!p->mass_fp)
            continue;
        ASSERT_INT_EQ(p->material, DC_MATERIAL_MORTAR_DEBRIS);
        mass += p->mass_fp;
        momentum_x += (int64_t)p->mass_fp * p->vx_fp;
        momentum_y += (int64_t)p->mass_fp * p->vy_fp;
    }
    ASSERT_TRUE(fabs((double)mass / DC_FLUID_FULL - 38.4) < .002);
    ASSERT_INT_EQ(chunk->particle_count, 24);
    ASSERT_EQ(momentum_x, 0);
    ASSERT_EQ(momentum_y, 0);
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &body,
                                    err, sizeof(err)));
    ASSERT_TRUE(!body.active);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void
test_crumble_accounts_fracture_work_and_remaining_dissipation(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_CONVERTED);
    ASSERT_TRUE(state.fracture_work > 0);
    ASSERT_TRUE(state.fracture_work <= state.strain_energy);
    ASSERT_TRUE(state.dissipated_energy > 0);
    ASSERT_TRUE(fabs(state.fracture_work + state.dissipated_energy -
                     state.strain_energy) < .01);
    dc_gpu_destroy(g);
    PASS();
}

static void test_rotated_crumble_across_chunk_seam_preserves_centroid(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(58);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    const double angle = .3;
    s.nodes[1].chunk.x = 1;
    s.nodes[1].x_fp = (int32_t)llround((58 + 12 * cos(angle) - 64) * 65536);
    s.nodes[1].y_fp = (int32_t)llround((40 + 12 * sin(angle)) * 65536);
    s.anchors[0].x = 57;
    s.anchors[0].y = 39;
    ASSERT_TRUE(dc_gpu_paint_material(g, 57, 39, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 3; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    double mass = 0, x = 0, y = 0;
    unsigned count = 0;
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g, slot, chunk, err, sizeof(err)));
        unsigned actual = 0;
        for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
            const dc_mpm_particle_t *p = &chunk->particles[i];
            if (!p->mass_fp)
                continue;
            ASSERT_INT_EQ(p->material, DC_MATERIAL_MORTAR_DEBRIS);
            double m = (double)p->mass_fp / 65536;
            mass += m;
            x += m * ((double)p->x_fp / 65536 + slot * 64);
            y += m * (double)p->y_fp / 65536;
            actual++;
        }
        ASSERT_EQ(actual, chunk->particle_count);
        count += actual;
    }
    fr_structural_gpu_member_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    printf("rotated crumble: state %u damage %.6f energy %.6f work %.6f count "
           "%u mass %.6f\n",
           state.state, state.damage, state.strain_energy, state.fracture_work,
           count, mass);
    fr_structural_gpu_component_state_t component;
    fr_structural_gpu_node_state_t node;
    ASSERT_TRUE(fr_gpu_read_structure_component(g, 1, &component));
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &node));
    printf("rotated equilibrium: flags %u residual %.9g pivot %.9g geometry "
           "%.9g roots %u cold %u\n",
           component.flags, component.relative_residual,
           component.minimum_pivot, component.geometry_minimum,
           node.valid_anchors, node.unknown_anchors);
    ASSERT_TRUE(count > 24);
    ASSERT_TRUE(fabs(mass - 38.4) < .002);
    ASSERT_TRUE(fabs(x / mass - (58 + 6 * cos(angle))) < .002);
    ASSERT_TRUE(fabs(y / mass - (40 + 6 * sin(angle))) < .002);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_crumble_pool_saturation_keeps_owner_then_retries_once(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    for (unsigned y = 39; y < 41; y++)
        for (unsigned x = 20; x < 32; x++) {
            unsigned cell = y * 64 + x;
            chunk->cells[cell].material = DC_MATERIAL_MORTAR_DEBRIS;
            dc_chunk_particle_init(&chunk->particles[cell], chunk->coord, cell,
                                   DC_MATERIAL_MORTAR_DEBRIS);
            chunk->particles[cell].mass_fp = DC_FLUID_FULL;
            chunk->particles[cell + DC_CHUNK_CELLS] = chunk->particles[cell];
            chunk->particles[cell + DC_CHUNK_CELLS].id_lo += DC_CHUNK_CELLS;
        }
    chunk->particle_count = 48;
    ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t state;
    dc_gpu_body_motion_t motion;
    uint32_t id = FR_STRUCTURAL_BODY_ID_BASE + 1;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_PENDING_CONVERSION);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, id, &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(fabs(motion.mass - 38.4) < .002);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 48);
    for (unsigned y = 39; y < 41; y++)
        for (unsigned x = 20; x < 32; x++)
            memset(&chunk->particles[y * 64 + x + DC_CHUNK_CELLS], 0,
                   sizeof(dc_mpm_particle_t));
    chunk->particle_count = 24;
    ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    uint64_t mass = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++)
        mass += chunk->particles[i].mass_fp;
    ASSERT_EQ(chunk->particle_count, 48);
    ASSERT_TRUE(fabs((double)mass / DC_FLUID_FULL - 62.4) < .002);
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_CONVERTED);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_committed_mortar_debris_moves_in_actual_mpm_simulation(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    for (unsigned i = 0; i < 8; i++)
        ASSERT_TRUE(dc_gpu_tick_step(g, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    uint64_t mass = 0;
    double mean_y = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        const dc_mpm_particle_t *p = &chunk->particles[i];
        if (!p->mass_fp)
            continue;
        ASSERT_EQ(p->material, DC_MATERIAL_MORTAR_DEBRIS);
        mass += p->mass_fp;
        mean_y += (double)p->mass_fp * p->y_fp / DC_FLUID_FULL;
        count++;
    }
    ASSERT_TRUE(fabs((double)mass / DC_FLUID_FULL - 38.4) < .002);
    ASSERT_EQ(count, chunk->particle_count);
    printf("mortar MPM: mass %.6f count %u mean y %.6f\n",
           (double)mass / DC_FLUID_FULL, count, mean_y / mass);
    ASSERT_TRUE(mean_y / mass > 40.5);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_mortar_debris_paint_uses_material_density(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 20, 0, DC_MATERIAL_MORTAR_DEBRIS,
                                      err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1);
    ASSERT_TRUE(
        fabs((double)chunk->particles[20 * 64 + 20].mass_fp / DC_FLUID_FULL -
             1.6) < .0001);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_overstressed_stone_creates_owned_falling_rigid_fragments(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE,
                                      err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    uint32_t parent = FR_STRUCTURAL_BODY_ID_BASE + 1;
    uint32_t child = FR_STRUCTURAL_OUTPUT_ID_BASE + 1;
    dc_gpu_body_motion_t original, a, b;
    dc_gpu_body_t ba, bb;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, parent, &original, err, sizeof(err)));
    for (unsigned i = 0; i < 18; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_t ordinary = {.x_fp = 90 << 16, .y_fp = 90 << 16,
                             .width = 1, .height = 1, .id = 7, .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, ordinary, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, parent, &a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, child, &b, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g, parent, &ba, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g, child, &bb, err, sizeof(err)));
    ASSERT_TRUE(a.mass > 0 && b.mass > 0);
    ASSERT_TRUE(fabs(a.mass + b.mass - original.mass) < .001);
    double dx = (double)(bb.x_fp - ba.x_fp) / 65536 + b.center_x - a.center_x;
    double dy = (double)(bb.y_fp - ba.y_fp) / 65536 + b.center_y - a.center_y;
    double inertia = a.inertia + b.inertia + a.mass * b.mass /
                     (a.mass + b.mass) * (dx * dx + dy * dy);
    ASSERT_TRUE(fabs(inertia - original.inertia) < .02);
    ASSERT_TRUE(!(a.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(!(b.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_EQ(ba.vx_fp, 0);
    ASSERT_EQ(bb.vy_fp, 0);
    ASSERT_EQ(a.angular_velocity, 0);
    ASSERT_EQ(b.angular_velocity, 0);
    ASSERT_TRUE(!dc_gpu_spawn_body(g, bb, err, sizeof(err)));
    int y = bb.y_fp;
    for (unsigned i = 0; i < 4; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g, child, &bb, err, sizeof(err)));
    ASSERT_TRUE(bb.y_fp > y);
    dc_gpu_destroy(g);
    PASS();
}

static void test_stone_fragment_pool_is_atomic_and_retry_does_not_skip_ids(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE,
                                      err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 63; i++) {
        dc_gpu_body_t ordinary = {.x_fp = (70 + i % 8 * 3) << 16,
                                  .y_fp = (70 + i / 8 * 3) << 16,
                                  .width = 1, .height = 1, .id = i + 1, .active = 1};
        ASSERT_TRUE(dc_gpu_spawn_body(g, ordinary, err, sizeof(err)));
    }
    for (unsigned i = 0; i < 18; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t state;
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_PENDING_CONVERSION);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(fabs(motion.mass - 64.8) < .001);
    ASSERT_TRUE(dc_gpu_remove_body(g, 63, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.mass > 0);
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_CONVERTED);
    dc_gpu_destroy(g);
    PASS();
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--mortar-probe") == 0) {
        RUN(test_overloaded_mortar_crumble_preserves_mass_without_launch);
        return g_fail != 0;
    }
    RUN(test_explicit_mortar_has_its_own_density_and_material_identity);
    RUN(test_overloaded_mortar_crumble_preserves_mass_without_launch);
    RUN(test_crumble_accounts_fracture_work_and_remaining_dissipation);
    RUN(test_rotated_crumble_across_chunk_seam_preserves_centroid);
    RUN(test_crumble_pool_saturation_keeps_owner_then_retries_once);
    RUN(test_committed_mortar_debris_moves_in_actual_mpm_simulation);
    RUN(test_mortar_debris_paint_uses_material_density);
    RUN(test_overstressed_stone_creates_owned_falling_rigid_fragments);
    RUN(test_stone_fragment_pool_is_atomic_and_retry_does_not_skip_ids);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
