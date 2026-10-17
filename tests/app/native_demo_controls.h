#ifndef DC_NATIVE_DEMO_CONTROLS_H
#define DC_NATIVE_DEMO_CONTROLS_H
#include <stdbool.h>
#include <SDL2/SDL.h>

static void native_demo_event(const SDL_Event *event,bool *running,bool *paused) {
    (void)event;(void)running;(void)paused;
}
#endif
