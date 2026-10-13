#define main pixel_component_fixture_main
#include "pixel_component_tests.c"
#undef main

#include <string.h>

static void ring_world(unsigned composition) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunks = calloc(2, sizeof(*chunks));
    ASSERT_TRUE(chunks);
    chunks[1].coord.x = 1;
    for (unsigned slot = 0; slot < 2; slot++)
        for (unsigned i = 0; i < 4096; i++)
            chunks[slot].cells[i].fluid_mass = DC_FLUID_FULL;
    double mass = 0;
    for (unsigned y = 10; y < 46; y++)
        for (unsigned x = 40; x < 92; x++)
            if (x < 42 || x >= 90 || y < 12 || y >= 44) {
                unsigned material = composition == 0 ? DC_MATERIAL_WOOD
                                    : composition == 1 ? DC_MATERIAL_DENSE_WOOD
                                    : x < 64 ? DC_MATERIAL_WOOD : DC_MATERIAL_STONE;
                chunks[x / 64].cells[y * 64 + x % 64].material = material;
                mass += material == DC_MATERIAL_WOOD ? .6
                        : material == DC_MATERIAL_DENSE_WOOD ? 1.2 : 2.7;
            }
    chunks[0].cells[9 * 64 + 39].material = DC_MATERIAL_STONE;
    chunks[0].cells[24 * 64 + 62].material = DC_MATERIAL_SAND;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunks, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 0x80000001u, &body, err, sizeof(err)));
    ASSERT_EQ(body.width, 1u);
    ASSERT_TRUE(!dc_gpu_read_body_id(gpu, 0x80000002u, &body, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, chunks + 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    for (unsigned tick = 0; tick < 5; tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 0x80000002u, &body, err, sizeof(err)));
    ASSERT_EQ(body.width, 52u);
    ASSERT_EQ(body.height, 36u);
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, body.id, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabs(motion.mass - mass) < .002);
    ASSERT_TRUE(fabs(motion.density - mass / 336) < 1e-6);
    ASSERT_TRUE(composition == 0 ? body.vy_fp < 0 : body.vy_fp > 0);
    dc_gpu_body_pixel_t pixels[336], restored[336];
    uint32_t count;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, body.id, pixels, 336, &count, err, sizeof(err)));
    ASSERT_EQ(count, 336u);
    for (unsigned i = 0; i < count; i++) {
        ASSERT_TRUE(pixels[i].x < 2 || pixels[i].x >= 50 || pixels[i].y < 2 || pixels[i].y >= 34);
        unsigned expected = composition == 0 ? DC_GPU_BODY_WOOD
                            : composition == 1 ? DC_GPU_BODY_DENSE_WOOD
                            : pixels[i].x < 24 ? DC_GPU_BODY_WOOD : DC_GPU_BODY_STONE;
        ASSERT_EQ(pixels[i].material, expected);
    }
    ASSERT_TRUE(dc_gpu_set_body_motion(gpu, body.id, .35f, .01f, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, body.id, &body, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, body.id, &motion, err, sizeof(err)));
    double dx = 26 - motion.center_x, dy = 18 - motion.center_y;
    unsigned hx = (unsigned)floor(body.x_fp / 65536. + motion.center_x + cos(motion.angle) * dx - sin(motion.angle) * dy);
    unsigned hy = (unsigned)floor(body.y_fp / 65536. + motion.center_y + sin(motion.angle) * dx + cos(motion.angle) * dy);
    uint32_t current, swept;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, hx, hy, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    uint64_t water = 0;
    unsigned solids = 0;
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, slot, chunks + slot, err, sizeof(err)));
        for (unsigned i = 0; i < 4096; i++) {
            solids += chunks[slot].cells[i].material != DC_MATERIAL_AIR;
            water += chunks[slot].cells[i].fluid_mass;
        }
    }
    ASSERT_EQ(solids, 1u);
    ASSERT_EQ(chunks[0].cells[24 * 64 + 62].material, DC_MATERIAL_SAND);
    ASSERT_TRUE(water == (uint64_t)8192 * DC_FLUID_FULL);
    ASSERT_TRUE(fr_gpu_save_structure(gpu, "build/pixel_ring_world.dcs", err, sizeof(err)));
    dc_gpu_t *copy = NULL;
    ASSERT_TRUE(dc_gpu_create(&copy, 192, 128, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    for (unsigned slot = 0; slot < 2; slot++)
        ASSERT_TRUE(dc_gpu_upload_chunk(copy, slot, chunks + slot, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(copy, (dc_chunk_coord_t){1, 0}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(copy, 0, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(copy, "build/pixel_ring_world.dcs", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_pixel_shape(copy, body.id, restored, 336, &count, err, sizeof(err)));
    ASSERT_EQ(count, 336u);
    ASSERT_EQ(memcmp(restored, pixels, sizeof(pixels)), 0);
    dc_gpu_body_motion_t loaded;
    ASSERT_TRUE(dc_gpu_read_body_motion(copy, body.id, &loaded, err, sizeof(err)));
    ASSERT_EQ(loaded.mass, motion.mass);
    ASSERT_EQ(loaded.inertia, motion.inertia);
    ASSERT_EQ(loaded.angle, motion.angle);
    printf("Ring %u mass %.6f density %.6f vy %d: cold/warm seam, hole, water and relocated reload\n",
           composition, motion.mass, motion.density, body.vy_fp);
    dc_gpu_destroy(copy);
    dc_gpu_destroy(gpu);
    free(chunks);
    PASS();
}
static void test_hollow_wood_component_floats_with_exact_holes_and_streaming(void) { ring_world(0); }
static void test_hollow_dense_wood_component_sinks_from_material_density(void) { ring_world(1); }
static void test_mixed_hollow_component_preserves_composition_across_warm_seams(void) { ring_world(2); }
static void test_pixel_contact_features_identify_the_full_cell_ordinal(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 10; y < 30; y++)
        for (unsigned x = 10; x < 30; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_record_t *body = gpu->body_mapped;
    body[0].body.y_fp = body[0].local_y_fp = 12 * 65536;
    body[0].body.vy_fp = 0;
    for (unsigned x = 0; x < 64; x++)
        ASSERT_TRUE(dc_gpu_paint_material(gpu, x, 32, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_contact_stats_t stats;
    dc_gpu_contact_t *contacts = calloc(8192, sizeof(*contacts));
    ASSERT_TRUE(contacts);
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 8192, err, sizeof(err)));
    ASSERT_EQ(stats.overflow, 0u);
    const uint32_t *geometry = (uint32_t *)gpu->body_mapped + body[0].compound_reserved;
    unsigned found = 0;
    for (unsigned i = 0; i < stats.count; i++) {
        if (contacts[i].body_a != 0x80000001u || contacts[i].kind != DC_GPU_CONTACT_TERRAIN)
            continue;
        found++;
        uint32_t ordinal = (contacts[i].feature_a & 0x7fffffffu) >> 3;
        ASSERT_TRUE(ordinal < geometry[1] * 256);
        uint32_t coord = geometry[8 + geometry[2] * geometry[3] + ordinal / 256 * 33];
        ASSERT_EQ((coord >> 16) * 16 + (ordinal % 256) / 16, 19u);
    }
    ASSERT_TRUE(found > 0);
    free(contacts);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_partial_grid_fracture_uses_all_tile_cells_conservatively(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunks = calloc(4, sizeof(*chunks));
    ASSERT_TRUE(chunks);
    for (unsigned slot = 0; slot < 4; slot++) {
        chunks[slot].coord.x = slot % 2;
        chunks[slot].coord.y = slot / 2;
    }
    for (unsigned y = 1; y < 66; y++)
        for (unsigned x = 1; x < 66; x++)
            chunks[y / 64 * 2 + x / 64].cells[y % 64 * 64 + x % 64].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 67, 67, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    for (unsigned slot = 0; slot < 4; slot++) {
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slot, chunks + slot, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(gpu, slot % 2, slot / 2, slot, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(gpu, 0x80000001u, .02f, .001f, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(gpu));
    uint32_t *pending = dc_gpu_structural_data(gpu) + dc_gpu_structural_graph_offset() + 38000u;
    pending[0] = pending[1] = 1000000;
    pending[2] = 1;
    pending[3] = 0x80000001u;
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    double mass = 0;
    uint32_t count = 0;
    for (unsigned child = 0; child < 2; child++) {
        dc_gpu_body_motion_t motion;
        uint32_t n, id = 0x80000001u + child;
        ASSERT_TRUE(dc_gpu_read_body_motion(gpu, id, &motion, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, id, NULL, 0, &n, err, sizeof(err)));
        mass += motion.mass;
        count += n;
    }
    ASSERT_EQ(count, 4225u);
    ASSERT_TRUE(fabs(mass - 11407.5) < .002);
    free(chunks);
    dc_gpu_destroy(gpu);
    PASS();
}
int main(void) {
    RUN(test_hollow_wood_component_floats_with_exact_holes_and_streaming);
    RUN(test_hollow_dense_wood_component_sinks_from_material_density);
    RUN(test_mixed_hollow_component_preserves_composition_across_warm_seams);
    RUN(test_pixel_contact_features_identify_the_full_cell_ordinal);
    RUN(test_partial_grid_fracture_uses_all_tile_cells_conservatively);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
