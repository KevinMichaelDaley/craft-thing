#include <stdio.h>

#include "../../src/vulkan/gpu_internal.h"
#include "concave_scene.h"

static int g_pass, g_fail;
static VkQueryPool g_queries;
static unsigned g_dispatch;
static bool g_recording, g_subsystem;
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
void __real_fr_renderer_record(dc_gpu_t *g);
void __wrap_fr_renderer_record(dc_gpu_t *g) {
  vkCmdResetQueryPool(g->command, g_queries, 0, 12);
  g_dispatch = 0;
  g_recording = true;
  vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 0);
  __real_fr_renderer_record(g);
  vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 1);
  g_recording = false;
}
void __real_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y,
                          uint32_t z);
void __wrap_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y,
                          uint32_t z) {
  unsigned query = 99;
  if (g_recording && !g_subsystem) {
    if (g_dispatch == 2)
      query = 2;
    else if (g_dispatch == 3)
      query = 4;
    else if (g_dispatch == 5)
      query = 10;
    g_dispatch++;
  }
  if (query != 99)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         g_queries, query);
  __real_vkCmdDispatch(command, x, y, z);
  if (query != 99)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         g_queries, query + 1);
}
void __real_fr_occlusion_record(fr_occlusion_gpu_t *, VkBuffer, VkDeviceSize);
void __wrap_fr_occlusion_record(fr_occlusion_gpu_t *c, VkBuffer b,
                                VkDeviceSize offset) {
  extern VkCommandBuffer g_command;
  g_subsystem = true;
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 6);
  __real_fr_occlusion_record(c, b, offset);
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 7);
  g_subsystem = false;
}
void __real_fr_lighting_record(fr_lighting_gpu_t *, VkBuffer, VkDeviceSize);
void __wrap_fr_lighting_record(fr_lighting_gpu_t *l, VkBuffer b,
                               VkDeviceSize offset) {
  extern VkCommandBuffer g_command;
  g_subsystem = true;
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 8);
  __real_fr_lighting_record(l, b, offset);
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 9);
  g_subsystem = false;
}
VkCommandBuffer g_command;
static void test_sun_and_four_lights_stay_within_added_gpu_budget(void) {
  dc_gpu_t *g;
  char err[256] = {0};
  ASSERT_TRUE(dc_gpu_create(&g, 480, 270, "build/shaders/pattern.comp.spv", err,
                            sizeof(err)));
  ASSERT_TRUE(renderer_concave_scene(g, err, sizeof(err)));
  ASSERT_TRUE(renderer_four_lights(g, 4));
  g_command = g->command;
  VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                              .queryType = VK_QUERY_TYPE_TIMESTAMP,
                              .queryCount = 12};
  ASSERT_EQ(vkCreateQueryPool(g->device, &qi, NULL, &g_queries), VK_SUCCESS);
  ASSERT_TRUE(
      dc_gpu_set_body_motion(g, 0x80000001u, 0, .005f, 0, err, sizeof(err)));
  ASSERT_TRUE(
      dc_gpu_set_body_motion(g, 0x80000002u, 0, -.003f, 0, err, sizeof(err)));
  double times[6] = {0};
  unsigned samples = 24;
  for (unsigned i = 0; i < samples; i++) {
    ASSERT_TRUE(dc_gpu_tick_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
    uint64_t q[12];
    ASSERT_EQ(vkGetQueryPoolResults(
                  g->device, g_queries, 0, 12, sizeof(q), q, 8,
                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
              VK_SUCCESS);
    uint64_t mask = g->timestamp_bits == 64
                        ? UINT64_MAX
                        : ((UINT64_C(1) << g->timestamp_bits) - 1);
    for (unsigned stage = 0; stage < 6; stage++)
      times[stage] += ((q[2 * stage + 1] - q[2 * stage]) & mask) *
                      g->timestamp_period / 1e6;
  }
  printf("Renderer mean GPU ms: total %.3f, texture %.3f, optical %.3f, "
         "hierarchy %.3f, light %.3f, compose %.3f\n",
         times[0] / samples, times[1] / samples, times[2] / samples,
         times[3] / samples, times[4] / samples, times[5] / samples);
  vkDestroyQueryPool(g->device, g_queries, NULL);
  g_queries = VK_NULL_HANDLE;
  dc_gpu_destroy(g);
  ASSERT_TRUE(times[0] / samples <= 2.0);
  PASS();
}
int main(void) {
  RUN(test_sun_and_four_lights_stay_within_added_gpu_budget);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
