#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "dungeoncraft/gpu.h"

static int g_pass,g_fail;
static char selected_driver[64];
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

SDL_Window *SDLCALL __wrap_SDL_CreateWindow(const char *title,int x,int y,int w,int h,Uint32 flags) {
    (void)title;(void)x;(void)y;(void)w;(void)h;(void)flags;
    const char *driver=SDL_GetCurrentVideoDriver();
    snprintf(selected_driver,sizeof(selected_driver),"%s",driver?driver:"");
    return NULL;
}
static bool probe_driver(void) {
    char err[256]={0};dc_gpu_t *g=NULL;selected_driver[0]=0;
    bool created=dc_gpu_create_window(&g,8,8,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err));
    dc_gpu_destroy(g);
    printf("selected video driver: %s; Vulkan initialization prevented\n",selected_driver);
    return !created&&selected_driver[0]!=0;
}
static void test_native_wayland_is_preferred_when_no_driver_is_requested(void) {
    const char *display=getenv("WAYLAND_DISPLAY");
    if(!display||!*display) {puts("SKIP no native Wayland display");return;}
    ASSERT_EQ(unsetenv("SDL_VIDEODRIVER"),0);SDL_ResetHint(SDL_HINT_VIDEODRIVER);
    ASSERT_TRUE(probe_driver());ASSERT_EQ(strcmp(selected_driver,"wayland"),0);PASS();
}
static void test_explicit_video_driver_environment_is_respected(void) {
    ASSERT_EQ(setenv("SDL_VIDEODRIVER","dummy",1),0);
    ASSERT_TRUE(probe_driver());ASSERT_EQ(strcmp(selected_driver,"dummy"),0);
    ASSERT_EQ(unsetenv("SDL_VIDEODRIVER"),0);SDL_ResetHint(SDL_HINT_VIDEODRIVER);PASS();
}
static void test_explicit_sdl_video_driver_hint_is_respected(void) {
    ASSERT_EQ(unsetenv("SDL_VIDEODRIVER"),0);ASSERT_TRUE(SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy"));
    ASSERT_TRUE(probe_driver());ASSERT_EQ(strcmp(selected_driver,"dummy"),0);
    SDL_ResetHint(SDL_HINT_VIDEODRIVER);PASS();
}
int main(void) {
    RUN(test_native_wayland_is_preferred_when_no_driver_is_requested);
    RUN(test_explicit_video_driver_environment_is_respected);
    RUN(test_explicit_sdl_video_driver_hint_is_respected);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
