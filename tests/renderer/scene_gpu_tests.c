#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/renderer/gpu.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
  do {                                                                         \
    printf("RUN %s\n", #fn);                                                   \
    fn();                                                                      \
  } while (0)
#define ASSERT_TRUE(e)                                                         \
  do {                                                                         \
    if (!(e)) {                                                                \
      printf("FAIL %d: %s\n", __LINE__, #e);                                   \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
static void
test_renderer_uses_textures_preserves_holes_and_paused_physics(void) {
  dc_gpu_t *g;
  char err[256];
  ASSERT_TRUE(dc_gpu_create(&g, 64, 64, "build/shaders/pattern.comp.spv", err,
                            sizeof(err)));
  dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
  ASSERT_TRUE(chunk);
  for (unsigned y = 5; y < 25; y++)
    for (unsigned x = 8; x < 28; x++)
      if (x < 12 || x >= 24 || y >= 21)
        chunk->cells[y * 64 + x].material = DC_MATERIAL_WOOD;
  for (unsigned y = 32; y < 50; y++)
    for (unsigned x = 35; x < 54; x++)
      if (x < 39 || y % 6 < 3)
        chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
  ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)));
  ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
  ASSERT_TRUE(dc_gpu_render_chunks(g, err, sizeof(err)));
  uint32_t reference[4096], lit[4096];
  memcpy(reference, g->mapped, sizeof(reference));
  dc_cell_t cells_before[4096];
  ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
  memcpy(cells_before, chunk->cells, sizeof(cells_before));
  ASSERT_TRUE(fr_renderer_enable(g, err, sizeof(err)));
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_ALBEDO));
  ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
  memcpy(lit, g->mapped, sizeof(lit));
  ASSERT_TRUE(memcmp(lit, reference, sizeof(lit)) != 0);
  unsigned variations = 0;
  for (unsigned y = 5; y < 25; y++)
    for (unsigned x = 8; x < 12; x++)
      variations += lit[y * 64 + x] != lit[5 * 64 + 8];
  ASSERT_TRUE(variations > 20);
  ASSERT_EQ(lit[10 * 64 + 18], lit[0]);
  for (unsigned frame = 0; frame < 4; frame++) {
    ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
    ASSERT_TRUE(memcmp(lit, g->mapped, sizeof(lit)) == 0);
  }
  ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
  ASSERT_TRUE(memcmp(cells_before, chunk->cells, sizeof(cells_before)) == 0);
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_REFERENCE));
  ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
  ASSERT_TRUE(memcmp(reference, g->mapped, sizeof(reference)) == 0);
  dc_gpu_destroy(g);
  free(chunk);
  PASS();
}
int main(void) {
  RUN(test_renderer_uses_textures_preserves_holes_and_paused_physics);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
