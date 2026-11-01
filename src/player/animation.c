#include <stddef.h>

#include "ferrum/player/player.h"
#include "player_internal.h"

bool fr_player_clip(fr_player_action_t action,fr_player_clip_t *out) {
    static const fr_player_clip_t clips[FR_PLAYER_ACTION_COUNT]={
        [FR_PLAYER_IDLE]={1,100,true}, [FR_PLAYER_WALK]={6,100,true},
        [FR_PLAYER_CHEW]={8,80,false}, [FR_PLAYER_CAST]={4,100,false}
    };
    if ((unsigned)action>=FR_PLAYER_ACTION_COUNT || !out) return false;
    *out=clips[action];
    return true;
}

bool fr_player_set_action(fr_player_t *p,fr_player_action_t action,bool restart) {
    fr_player_clip_t clip;
    if (!player_valid(p) || !fr_player_clip(action,&clip)) return false;
    if (p->action!=action || restart) p->elapsed_ms=0;
    p->action=action;
    return true;
}

bool fr_player_advance(fr_player_t *p,uint64_t elapsed) {
    fr_player_clip_t clip;
    if (!player_valid(p) || !fr_player_clip(p->action,&clip)) return false;
    uint64_t duration=(uint64_t)clip.frames*clip.frame_ms;
    if (clip.loop) p->elapsed_ms=(p->elapsed_ms%duration+elapsed%duration)%duration;
    else {
        uint64_t current=p->elapsed_ms<duration?p->elapsed_ms:duration;
        p->elapsed_ms=elapsed>=duration-current?duration:current+elapsed;
    }
    return true;
}

bool fr_player_frame(const fr_player_t *p,uint32_t *out) {
    fr_player_clip_t clip;
    if (!player_valid(p) || !out || !fr_player_clip(p->action,&clip)) return false;
    uint64_t frame=p->elapsed_ms/clip.frame_ms;
    *out=clip.loop?(uint32_t)(frame%clip.frames):
        (frame>=clip.frames?clip.frames-1:(uint32_t)frame);
    return true;
}

bool fr_player_action_complete(const fr_player_t *p) {
    fr_player_clip_t clip;
    return player_valid(p) && fr_player_clip(p->action,&clip) && !clip.loop &&
        p->elapsed_ms>=(uint64_t)clip.frames*clip.frame_ms;
}
