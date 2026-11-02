#include "ferrum/player/input_sdl.h"

bool fr_input_sdl_defaults(fr_input_t *i,int64_t d,bool controller) {
    static const struct {uint32_t control,action;float scale;} keys[]={
        {SDL_SCANCODE_A,FR_INPUT_MOVE,-1},{SDL_SCANCODE_D,FR_INPUT_MOVE,1},
        {SDL_SCANCODE_SPACE,FR_INPUT_JUMP,1},{SDL_SCANCODE_C,FR_INPUT_CHEW,1},
        {SDL_SCANCODE_Q,FR_INPUT_FIRE,1},{SDL_SCANCODE_E,FR_INPUT_ICE,1}
    },pad[]={
        {FR_INPUT_SDL_AXIS|SDL_CONTROLLER_AXIS_LEFTX,FR_INPUT_MOVE,1},
        {FR_INPUT_SDL_AXIS|SDL_CONTROLLER_AXIS_RIGHTX,FR_INPUT_AIM_X,1},
        {FR_INPUT_SDL_AXIS|SDL_CONTROLLER_AXIS_RIGHTY,FR_INPUT_AIM_Y,1},
        {SDL_CONTROLLER_BUTTON_A,FR_INPUT_JUMP,1},
        {SDL_CONTROLLER_BUTTON_B,FR_INPUT_CHEW,1},
        {SDL_CONTROLLER_BUTTON_X,FR_INPUT_FIRE,1},
        {SDL_CONTROLLER_BUTTON_Y,FR_INPUT_ICE,1}
    };
    unsigned count=controller?sizeof(pad)/sizeof(pad[0]):sizeof(keys)/sizeof(keys[0]);
    fr_input_t next=*i;
    for (unsigned n=0;n<count;n++) {
        uint32_t control=controller?pad[n].control:keys[n].control;
        uint32_t action=controller?pad[n].action:keys[n].action;
        float scale=controller?pad[n].scale:keys[n].scale;
        if (!fr_input_bind(&next,d,control,action,scale)) return false;
    }
    *i=next;return true;
}
bool fr_input_sdl_event(fr_input_t *i,const SDL_Event *event) {
    if (!i || !event) return false;
    int64_t device;uint32_t control;float value;bool repeat=false;
    switch (event->type) {
    case SDL_WINDOWEVENT:
        if (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST) fr_input_focus(i,false);
        else if (event->window.event==SDL_WINDOWEVENT_FOCUS_GAINED) fr_input_focus(i,true);
        return false;
    case SDL_CONTROLLERDEVICEREMOVED:
        fr_input_disconnect(i,event->cdevice.which);return false;
    case SDL_KEYDOWN:case SDL_KEYUP:
        device=FR_INPUT_KEYBOARD_DEVICE;control=event->key.keysym.scancode;
        value=event->type==SDL_KEYDOWN?1:0;repeat=event->key.repeat!=0;break;
    case SDL_CONTROLLERBUTTONDOWN:case SDL_CONTROLLERBUTTONUP:
        device=event->cbutton.which;control=event->cbutton.button;
        value=event->type==SDL_CONTROLLERBUTTONDOWN?1:0;break;
    case SDL_CONTROLLERAXISMOTION: {
        device=event->caxis.which;control=FR_INPUT_SDL_AXIS|event->caxis.axis;
        int raw=event->caxis.value,magnitude=raw<0?-raw:raw;
        const int deadzone=8000;
        value=magnitude<=deadzone?0.f:(float)(magnitude-deadzone)/
            (float)((raw<0?32768:32767)-deadzone)*(raw<0?-1.f:1.f);
        break;
    }
    default:return false;
    }
    uint64_t owner=0;
    for (unsigned n=0;n<FR_INPUT_DEVICES;n++)
        if (i->devices[n].used && i->devices[n].id==device) owner=i->devices[n].owner;
    if (!owner) return false;
    bool gameplay=false,bound=false;
    for (unsigned n=0;n<FR_INPUT_PLAYERS;n++)
        if (i->players[n].identity==owner) gameplay=i->players[n].context==FR_INPUT_GAMEPLAY;
    for (unsigned n=0;n<FR_INPUT_BINDINGS;n++)
        if (i->bindings[n].used && i->bindings[n].device==device && i->bindings[n].control==control) bound=true;
    if (!bound) return false;
    fr_input_event(i,device,control,value,repeat);
    return gameplay;
}
