#include "ferrum/renderer/occlusion_gpu.h"
#include <stdio.h>
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
static void test_thin_wall_and_unknown_nodes_survive_hierarchy(void) {
  dc_gpu_t *g;
  fr_occlusion_gpu_t *c;
  char err[256];
  ASSERT_TRUE(dc_gpu_create(&g, 64, 64, "build/shaders/pattern.comp.spv", err,
                            sizeof(err)));
  ASSERT_TRUE(fr_occlusion_gpu_create(g, 7, 5, &c, err, sizeof(err)));
  float opacity[35] = {0};
  uint32_t known[35];
  for (unsigned i = 0; i < 35; i++)
    known[i] = 1;
  for (unsigned y = 0; y < 5; y++)
    opacity[y * 7 + 3] = 1;
  ASSERT_TRUE(fr_occlusion_gpu_upload(c, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(c, err, sizeof(err)));
  fr_occlusion_node_t node;
  ASSERT_TRUE(fr_occlusion_gpu_read(c, 0, 3, 2, &node));
  ASSERT_EQ(node.minimum, 1.f);
  ASSERT_TRUE(fr_occlusion_gpu_read(c, 3, 0, 0, &node));
  ASSERT_EQ(node.minimum, 0.f);
  ASSERT_EQ(node.maximum, 1.f);
  ASSERT_EQ(node.known, 1u);
  known[0] = 0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(c, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(c, err, sizeof(err)));
  ASSERT_TRUE(fr_occlusion_gpu_read(c, 0, 0, 0, &node));
  ASSERT_EQ(node.minimum, 1.f);
  ASSERT_EQ(node.known, 0u);
  ASSERT_TRUE(fr_occlusion_gpu_read(c, 3, 0, 0, &node));
  ASSERT_EQ(node.maximum, 1.f);
  ASSERT_EQ(node.known, 0u);
  fr_occlusion_gpu_destroy(c);
  dc_gpu_destroy(g);
  PASS();
}
int main(void) {
  RUN(test_thin_wall_and_unknown_nodes_survive_hierarchy);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
