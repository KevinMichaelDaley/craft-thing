#ifndef DC_NATIVE_DEMO_CONTROLS_H
#define DC_NATIVE_DEMO_CONTROLS_H
#include <stdbool.h>
#include <SDL2/SDL.h>

static void native_demo_event(const SDL_Event *event,bool *running,bool *paused) {
    if(event->type==SDL_QUIT) *running=false;
    if(event->type==SDL_KEYDOWN&&!event->key.repeat) {
        if(event->key.keysym.sym==SDLK_ESCAPE) *running=false;
        if(event->key.keysym.sym==SDLK_SPACE) *paused=!*paused;
    }
}
#endif
