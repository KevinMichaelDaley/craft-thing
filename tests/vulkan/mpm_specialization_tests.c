#define main particle_fixture_main
#include "particle_tests.c"
#undef main

uint32_t diagnostic_mpm_selected_modes(void);

static int marker_order(const void *a, const void *b) {
    const dc_marker_t *x = a, *y = b;
    if (x->id != y->id) return (x->id > y->id) - (x->id < y->id);
    if (x->kind != y->kind) return (x->kind > y->kind) - (x->kind < y->kind);
    if (x->x_fp != y->x_fp) return (x->x_fp > y->x_fp) - (x->x_fp < y->x_fp);
    return (x->y_fp > y->y_fp) - (x->y_fp < y->y_fp);
}

static void coupled_state(const char *variant, dc_chunk_t *chunk) {
    char err[256] = {0};
    ASSERT_EQ(setenv("DC_MPM_VARIANTS", variant, 1), 0);
    for (unsigned y = 8; y < 16; y++)
        for (unsigned x = 20; x < 44; x++)
            chunk->cells[y * 64 + x].material = (x & 1) ? DC_MATERIAL_SAND : DC_MATERIAL_DIRT;
    for (unsigned y = 8; y < 48; y++)
        for (unsigned x = 1; x < 63; x++)
            chunk->cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    for (unsigned x = 0; x < 64; x++)
        chunk->cells[60 * 64 + x].material = DC_MATERIAL_STONE;
    dc_chunk_seed_particles(chunk);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    for (unsigned i = 0; i < 3; i++)
        ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    qsort(chunk->markers, chunk->marker_count, sizeof(*chunk->markers), marker_order);
    dc_gpu_destroy(gpu);
}

static void test_specialized_dispatch_preserves_coupled_particle_state(void) {
    dc_chunk_t *reference = calloc(1, sizeof(*reference));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(reference && chunk);
    coupled_state("production", reference);
    if (g_fail) return;
    coupled_state("specialized", chunk);
    if (g_fail) return;
    uint64_t mass = 0, bound_water = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        mass += chunk->particles[i].mass_fp;
        bound_water += chunk->particles[i].flags & DC_MPM_MOISTURE_MASK;
    }
    uint64_t hash = UINT64_C(1469598103934665603);
    const unsigned char *bytes = (const unsigned char *)chunk;
    for (size_t i = 0; i < sizeof(*chunk); i++)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    printf("coupled MPM fingerprint %016llx selected modes0x%x\n",
           (unsigned long long)hash, diagnostic_mpm_selected_modes());
    bool equal = memcmp(reference, chunk, sizeof(*chunk)) == 0;
    free(reference);
    free(chunk);
    ASSERT_EQ(mass, UINT64_C(192) * DC_FLUID_FULL);
    ASSERT_TRUE(bound_water > 0);
    ASSERT_TRUE(equal);
    ASSERT_EQ(diagnostic_mpm_selected_modes(), 255u);
    PASS();
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    RUN(test_specialized_dispatch_preserves_coupled_particle_state);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
