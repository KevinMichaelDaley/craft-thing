#include "ferrum/player/input_sdl.h"

bool fr_input_sdl_defaults(fr_input_t *i,int64_t d,bool controller) {
    (void)i;(void)d;(void)controller;return false;
}
bool fr_input_sdl_event(fr_input_t *i,const SDL_Event *event) {
    (void)i;(void)event;return false;
}
