#ifndef DC_TEST_WINDOW_H
#define DC_TEST_WINDOW_H
#include <SDL.h>
#include "../src/vulkan/gpu_internal.h"

static inline bool dc_test_create_window(dc_gpu_t **gpu,uint32_t width,uint32_t height,
    uint32_t window_width,uint32_t window_height,const char *shader,
    char *err,uint32_t cap) {
    if(!dc_gpu_create_window(gpu,width,height,window_width,window_height,shader,err,cap)) return false;
    SDL_HideWindow((*gpu)->window);
    SDL_PumpEvents();
    return true;
}
#endif
