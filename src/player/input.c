#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ferrum/player/input.h"

static fr_input_player_t *input_player(fr_input_t *i,uint64_t id) {
    if (i && id) for (unsigned n=0;n<FR_INPUT_PLAYERS;n++)
        if (i->players[n].identity==id) return &i->players[n];
    return NULL;
}
static fr_input_device_t *input_device(fr_input_t *i,int64_t id) {
    if (i) for (unsigned n=0;n<FR_INPUT_DEVICES;n++)
        if (i->devices[n].used && i->devices[n].id==id) return &i->devices[n];
    return NULL;
}
static void input_recompute(fr_input_t *i,fr_input_player_t *p) {
    float values[FR_INPUT_ACTIONS]={0};
    for (unsigned n=0;n<FR_INPUT_BINDINGS;n++) {
        fr_input_binding_t *b=&i->bindings[n];
        fr_input_device_t *d=b->used?input_device(i,b->device):NULL;
        if (d && d->owner==p->identity) values[b->action]+=b->value*b->scale;
    }
    for (unsigned a=0;a<FR_INPUT_ACTIONS;a++) {
        float v=fmaxf(-1,fminf(1,values[a]));
        bool was=p->state.values[a]!=0,held=v!=0;
        if (held && !was) p->state.pressed|=1u<<a;
        if (was && !held) p->state.released|=1u<<a;
        p->state.values[a]=v;
    }
}
static void input_clear(fr_input_t *i,fr_input_player_t *p) {
    for (unsigned n=0;n<FR_INPUT_BINDINGS;n++) {
        fr_input_binding_t *b=&i->bindings[n];
        fr_input_device_t *d=b->used?input_device(i,b->device):NULL;
        if (d && d->owner==p->identity) b->value=0;
    }
    p->state.pressed=0;
    input_recompute(i,p);
    p->state.has_target=false;
}
void fr_input_init(fr_input_t *i) {
    if (i) {memset(i,0,sizeof(*i));i->focused=true;}
}
bool fr_input_add_player(fr_input_t *i,uint64_t id) {
    if (!i || !id || input_player(i,id)) return false;
    for (unsigned n=0;n<FR_INPUT_PLAYERS;n++) if (!i->players[n].identity) {
        i->players[n]=(fr_input_player_t){.identity=id};return true;
    }
    return false;
}
bool fr_input_connect(fr_input_t *i,int64_t id,uint64_t owner) {
    if (!input_player(i,owner)) return false;
    fr_input_device_t *d=input_device(i,id);
    if (d) return d->owner==owner;
    for (unsigned n=0;n<FR_INPUT_DEVICES;n++) if (!i->devices[n].used) {
        i->devices[n]=(fr_input_device_t){.id=id,.owner=owner,.used=true};return true;
    }
    return false;
}
bool fr_input_disconnect(fr_input_t *i,int64_t id) {
    fr_input_device_t *d=input_device(i,id);
    if (!d) return false;
    fr_input_player_t *p=input_player(i,d->owner);
    for (unsigned n=0;n<FR_INPUT_BINDINGS;n++)
        if (i->bindings[n].used && i->bindings[n].device==id)
            i->bindings[n]=(fr_input_binding_t){0};
    *d=(fr_input_device_t){0};
    p->state.pressed=0;
    input_recompute(i,p);
    p->state.has_target=false;
    return true;
}
bool fr_input_bind(fr_input_t *i,int64_t id,uint32_t control,uint32_t action,float scale) {
    fr_input_device_t *d=input_device(i,id);
    if (!d || action>=FR_INPUT_ACTIONS || !isfinite(scale) || scale<-1 || scale>1) return false;
    fr_input_binding_t *slot=NULL;
    for (unsigned n=0;n<FR_INPUT_BINDINGS;n++) {
        fr_input_binding_t *b=&i->bindings[n];
        if (b->used && b->device==id && b->control==control && b->action==action) {
            b->scale=scale;input_recompute(i,input_player(i,d->owner));return true;
        }
        if (!b->used && !slot) slot=b;
    }
    if (!slot) return false;
    *slot=(fr_input_binding_t){.device=id,.control=control,.action=action,.scale=scale,.used=true};
    return true;
}
bool fr_input_event(fr_input_t *i,int64_t id,uint32_t control,float value,bool repeat) {
    fr_input_device_t *d=input_device(i,id);
    if (!d || !isfinite(value) || value<-1 || value>1) return false;
    fr_input_player_t *p=input_player(i,d->owner);
    if (repeat || !i->focused || p->context!=FR_INPUT_GAMEPLAY) return true;
    for (unsigned n=0;n<FR_INPUT_BINDINGS;n++) {
        fr_input_binding_t *b=&i->bindings[n];
        if (b->used && b->device==id && b->control==control) b->value=value;
    }
    input_recompute(i,p);return true;
}
void fr_input_focus(fr_input_t *i,bool focused) {
    if (!i) return;
    if (!focused) for (unsigned n=0;n<FR_INPUT_PLAYERS;n++)
        if (i->players[n].identity) input_clear(i,&i->players[n]);
    i->focused=focused;
}
bool fr_input_context(fr_input_t *i,uint64_t id,fr_input_context_t context) {
    fr_input_player_t *p=input_player(i,id);
    if (!p || (unsigned)context>FR_INPUT_UI) return false;
    if (p->context!=context) {input_clear(i,p);p->context=context;}
    return true;
}
bool fr_input_target(fr_input_t *i,uint64_t id,bool valid,fr_player_point_t target) {
    fr_input_player_t *p=input_player(i,id);
    if (!p || (valid && (!isfinite(target.x)||!isfinite(target.y)||
        target.x<0||target.y<0||target.x>=DC_CHUNK_SIDE||target.y>=DC_CHUNK_SIDE))) return false;
    p->state.has_target=valid && i->focused && p->context==FR_INPUT_GAMEPLAY;
    p->state.target=valid?target:(fr_player_point_t){0};return true;
}
bool fr_input_sample(fr_input_t *i,uint64_t id,fr_input_state_t *out) {
    fr_input_player_t *p=input_player(i,id);
    if (!p || !out) return false;
    *out=p->state;p->state.pressed=0;p->state.released=0;return true;
}
