#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void
test_reloaded_coordinate_in_a_new_slot_checkpoints_latest_state_once(void) {
    char err[256];
    dc_gpu_t *gpu = grid(), *restored = grid();
    ASSERT_TRUE(gpu && restored);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    chunk->cells[20 * 64 + 20].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    chunk->cells[20 * 64 + 20].fluid_mass = 12345;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(gpu, "/tmp/shadow-chunk-store.dcs", err,
                                      sizeof(err)));
    bool loaded = fr_gpu_load_structure(restored, "/tmp/shadow-chunk-store.dcs",
                                        err, sizeof(err));
    bool overridden = false, latest = false;
    if (loaded) {
        memset(chunk, 0, sizeof(*chunk));
        loaded = fr_gpu_upload_structure_chunk(restored, 0, chunk, &overridden,
                                               err, sizeof(err)) &&
                 dc_gpu_download_chunk(restored, 0, chunk, err, sizeof(err));
        latest = chunk->cells[20 * 64 + 20].fluid_mass == 12345;
    }
    dc_gpu_destroy(restored);
    dc_gpu_destroy(gpu);
    free(chunk);
    remove("/tmp/shadow-chunk-store.dcs");
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(overridden);
    ASSERT_TRUE(latest);
    PASS();
}
int main(void) {
    RUN(test_reloaded_coordinate_in_a_new_slot_checkpoints_latest_state_once);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
