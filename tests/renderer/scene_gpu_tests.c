#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/renderer/occlusion_gpu.h"

static int g_pass, g_fail;
static unsigned g_hierarchy_recordings;
void __real_fr_occlusion_record(fr_occlusion_gpu_t *,VkBuffer,VkDeviceSize);
void __wrap_fr_occlusion_record(fr_occlusion_gpu_t *o,VkBuffer b,VkDeviceSize offset) {
  g_hierarchy_recordings++;
  __real_fr_occlusion_record(o,b,offset);
}
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
  chunk->cells[2 * 64 + 2].material = DC_MATERIAL_LEAVES;
  for (unsigned y = 0; y < 64; y++) {
    chunk->cells[y * 64 + 31].material = DC_MATERIAL_WATER;
    chunk->cells[y * 64 + 31].fluid_mass = 65536;
  }
  chunk->cells[20 * 64 + 32].material = DC_MATERIAL_STONE;
  chunk->cells[20 * 64 + 32].fluid_mass = 65536;
  chunk->cells[30 * 64 + 12].material = DC_MATERIAL_DIRT;
  chunk->cells[30 * 64 + 12].flags = DC_CELL_LOADED_TERRAIN;
  chunk->cells[30 * 64 + 13].material = DC_MATERIAL_DIRT;
  for (unsigned x = 12; x <= 13; x++) {
    dc_chunk_particle_init(&chunk->particles[30 * 64 + x], chunk->coord,
                           30 * 64 + x, DC_MATERIAL_DIRT);
    chunk->particles[30 * 64 + x].mass_fp = 49152;
  }
  chunk->particle_count = 2;
  ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)));
  ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
  dc_gpu_body_shape_t leaves = {
      .count = 4,
      .material = DC_GPU_BODY_LEAVES,
      .vertices = {{0, 0}, {65536, 0}, {65536, 65536}, {0, 65536}}};
  dc_gpu_world_body_t leaf_body = {.body = {.id = 77,
                                            .active = 1,
                                            .width = 1,
                                            .height = 1,
                                            .x_fp = 60 * 65536,
                                            .y_fp = 60 * 65536}};
  ASSERT_TRUE(
      dc_gpu_spawn_convex_body(g, leaf_body, &leaves, err, sizeof(err)));
  ASSERT_TRUE(
      dc_gpu_set_body_origin(g, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
  ASSERT_TRUE(dc_gpu_render_chunks(g, err, sizeof(err)));
  uint32_t reference[4096], lit[4096];
  memcpy(reference, g->mapped, sizeof(reference));
  dc_cell_t cells_before[4096];
  ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
  memcpy(cells_before, chunk->cells, sizeof(cells_before));
  dc_gpu_memory_stats_t before, after;
  ASSERT_TRUE(dc_gpu_memory_stats(g, &before));
  ASSERT_TRUE(fr_renderer_enable(g, err, sizeof(err)));
  ASSERT_TRUE(dc_gpu_memory_stats(g, &after));
  uint64_t bytes = after.mapped_local_bytes + after.mapped_system_bytes +
                   after.device_only_bytes - before.mapped_local_bytes -
                   before.mapped_system_bytes - before.device_only_bytes;
  uint64_t budget = (uint64_t)DC_GPU_CHUNK_SLOTS * 4096u * 40u +
                    4096u * 216u + 65536u;
  printf("renderer working memory %llu bytes, budget %llu\n",
         (unsigned long long)bytes, (unsigned long long)budget);
  ASSERT_TRUE(bytes <= budget);
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_ALBEDO));
  ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
  fr_occlusion_node_t leaf;
  ASSERT_TRUE(fr_renderer_read_opacity(g, 2, 2, &leaf));
  ASSERT_TRUE(leaf.maximum > 0 && leaf.maximum < 1);
  ASSERT_TRUE(fr_renderer_read_opacity(g, 60, 60, &leaf));
  ASSERT_TRUE(leaf.maximum > 0 && leaf.maximum < 1);
  /* Clear water is not an opaque leaf canopy: moving bulk water must not
     stamp column-shaped shadows into the incoming radiance field. */
  ASSERT_TRUE(fr_renderer_read_opacity(g, 31, 20, &leaf));
  ASSERT_EQ(leaf.maximum, 0.f);
  ASSERT_TRUE(fr_renderer_read_opacity(g, 32, 20, &leaf));
  ASSERT_EQ(leaf.maximum, 1.f);
  /* Terrain remains an intact optical solid while its MPM support settles.
     Loose grains still use their actual coverage. */
  ASSERT_TRUE(fr_renderer_read_opacity(g, 12, 30, &leaf));
  ASSERT_EQ(leaf.maximum, 1.f);
  ASSERT_TRUE(fr_renderer_read_opacity(g, 13, 30, &leaf));
  ASSERT_EQ(leaf.maximum, .75f);
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
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_LIT));
  ASSERT_TRUE(fr_renderer_set_sources(g, NULL, 0, (fr_renderer_sun_t){0}));
  ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
  ASSERT_EQ(((uint32_t *)g->mapped)[20 * 64 + 31], 0xff000000u);
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_REFERENCE));
  ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
  ASSERT_TRUE(memcmp(reference, g->mapped, sizeof(reference)) == 0);
  dc_gpu_destroy(g);
  free(chunk);
  ASSERT_EQ(g_hierarchy_recordings,0u);
  PASS();
}
int main(void) {
  RUN(test_renderer_uses_textures_preserves_holes_and_paused_physics);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
