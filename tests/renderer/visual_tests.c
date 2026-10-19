#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include <SDL.h>

#include "concave_scene.h"
#include "ferrum/renderer/texture.h"

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
static bool snapshot(dc_gpu_t *g, const char *name, uint32_t *pixels,
                     char *err) {
  if (!dc_gpu_readback(g, pixels, RENDERER_WIDTH * RENDERER_HEIGHT, err, 256))
    return false;
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
      pixels, RENDERER_WIDTH, RENDERER_HEIGHT, 32, RENDERER_WIDTH * 4,
      SDL_PIXELFORMAT_ABGR8888);
  bool okay = surface && SDL_SaveBMP(surface, name) == 0;
  if (surface)
    SDL_FreeSurface(surface);
  return okay;
}
static void
test_two_concave_textured_bodies_with_colored_lights_sun_and_stable_pause(
    void) {
  char err[256] = {0};
  dc_gpu_t *g;
  ASSERT_TRUE(dc_gpu_create_window(&g, 480, 270, 1920, 1080,
                                   "build/shaders/pattern.comp.spv", err,
                                   sizeof(err)));
  ASSERT_TRUE(renderer_concave_scene(g, err, sizeof(err)));
  ASSERT_TRUE(renderer_four_lights(g, 4));
  ASSERT_TRUE(dc_gpu_set_display_zoom(g, 4));
  ASSERT_TRUE(
      dc_gpu_set_window_title(g, "Textured concave bodies | sun + colored "
                                 "lights | exact physical pixels"));
  uint32_t *pixels = malloc(480 * 270 * 4), *paused = malloc(480 * 270 * 4);
  ASSERT_TRUE(pixels && paused);
  mkdir("build/screenshots", 0777);
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_ALBEDO));
  ASSERT_TRUE(dc_gpu_present_chunks(g, err, sizeof(err)));
  ASSERT_TRUE(
      snapshot(g, "build/screenshots/renderer_albedo.bmp", pixels, err));
  memcpy(paused, pixels, 480 * 270 * 4);
  ASSERT_TRUE(fr_renderer_set_mode(g, FR_RENDERER_LIT));
  ASSERT_TRUE(dc_gpu_present_chunks(g, err, sizeof(err)));
  ASSERT_TRUE(
      snapshot(g, "build/screenshots/renderer_lit_before.bmp", pixels, err));
  ASSERT_TRUE(memcmp(paused, pixels, 480 * 270 * 4) != 0);
  uint32_t count;
  ASSERT_TRUE(dc_gpu_read_pixel_shape(g, 0x80000001u, NULL, 0, &count, err,
                                      sizeof(err)));
  ASSERT_EQ(count, 1176u);
  ASSERT_TRUE(dc_gpu_read_pixel_shape(g, 0x80000002u, NULL, 0, &count, err,
                                      sizeof(err)));
  ASSERT_EQ(count, 1872u);
  ASSERT_TRUE(
      dc_gpu_set_body_motion(g, 0x80000001u, 0, .005f, 0, err, sizeof(err)));
  ASSERT_TRUE(
      dc_gpu_set_body_motion(g, 0x80000002u, 0, -.003f, 0, err, sizeof(err)));
  SDL_Delay(250);
  uint64_t began = SDL_GetPerformanceCounter();
  for (unsigned tick = 0; tick < 90; tick++) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
      ASSERT_TRUE(event.type != SDL_QUIT);
    ASSERT_TRUE(dc_gpu_present_chunks_steps(g, 1, err, sizeof(err)));
  }
  double elapsed = (SDL_GetPerformanceCounter() - began) /
                   (double)SDL_GetPerformanceFrequency();
  dc_gpu_body_motion_t a, b;
  ASSERT_TRUE(dc_gpu_read_body_motion(g, 0x80000001u, &a, err, sizeof(err)));
  ASSERT_TRUE(dc_gpu_read_body_motion(g, 0x80000002u, &b, err, sizeof(err)));
  ASSERT_TRUE(fabsf(a.angle) > .01f || fabsf(b.angle) > .01f);
  ASSERT_TRUE(fabsf(a.mass - 705.6f) < .002f &&
              fabsf(b.mass - 2894.4f) < .003f);
  ASSERT_TRUE(
      snapshot(g, "build/screenshots/renderer_lit_motion.bmp", pixels, err));
  memcpy(paused, pixels, 480 * 270 * 4);
  for (unsigned frame = 0; frame < 4; frame++) {
    ASSERT_TRUE(dc_gpu_present_chunks(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(g, pixels, 480 * 270, err, sizeof(err)));
    ASSERT_EQ(memcmp(paused, pixels, 480 * 270 * 4), 0);
  }
  ASSERT_TRUE(
      fr_renderer_set_material_style(g, DC_MATERIAL_STONE, FR_TEXTURE_BRICK));
  ASSERT_TRUE(fr_renderer_set_material_style(g, DC_MATERIAL_DENSE_WOOD,
                                             FR_TEXTURE_WOOD_RINGS));
  ASSERT_TRUE(dc_gpu_present_chunks(g, err, sizeof(err)));
  ASSERT_TRUE(
      snapshot(g, "build/screenshots/renderer_brick_rings.bmp", pixels, err));
  printf("Displayed480x270, actual physics+renderer %.2f ticks/s (%u "
         "frames/%.3fs), exact mass and four identical paused frames\n",
         90 / elapsed, 90u, elapsed);
  free(pixels);
  free(paused);
  dc_gpu_destroy(g);
  PASS();
}
int main(void) {
  RUN(test_two_concave_textured_bodies_with_colored_lights_sun_and_stable_pause);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
