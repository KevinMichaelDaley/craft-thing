#include <stddef.h>

#include "ferrum/player/player.h"
#include "player_internal.h"

enum { CHEW_FRAME_MS=100, CHEW_ENTRY_MS=300, CHEW_LOOP_MS=300,
       CHEW_RECOVERY_MS=100 };

bool fr_player_chew_tick(fr_player_t *p,bool valid,uint64_t elapsed) {
    if (!player_valid(p)) return false;
    bool active=p->intent.chew_held && p->intent.has_target && valid;
    if (p->chew_recovering) {
        uint64_t remaining=CHEW_RECOVERY_MS-p->elapsed_ms;
        if (elapsed<remaining) return fr_player_advance(p,elapsed);
        if (!fr_player_advance(p,remaining)) return false;
        elapsed-=remaining;
        if (!active) return true;
    }
    if (active) {
        if (!fr_player_set_action(p,FR_PLAYER_CHEW,false)) return false;
    } else if (p->action==FR_PLAYER_CHEW) {
        p->chew_recovering=true;
        p->elapsed_ms=0;
    } else return true;
    return fr_player_advance(p,elapsed);
}

bool fr_player_clip(fr_player_action_t action,fr_player_clip_t *out) {
    static const fr_player_clip_t clips[FR_PLAYER_ACTION_COUNT]={
        [FR_PLAYER_IDLE]={1,100,true}, [FR_PLAYER_WALK]={6,100,true},
        [FR_PLAYER_CHEW]={8,CHEW_FRAME_MS,false}, [FR_PLAYER_CAST]={4,100,false}
    };
    if ((unsigned)action>=FR_PLAYER_ACTION_COUNT || !out) return false;
    *out=clips[action];
    return true;
}

bool fr_player_set_action(fr_player_t *p,fr_player_action_t action,bool restart) {
    fr_player_clip_t clip;
    if (!player_valid(p) || !fr_player_clip(action,&clip)) return false;
    if (p->action!=action || restart) {
        p->elapsed_ms=0;
        p->chew_recovering=false;
    }
    p->action=action;
    return true;
}

bool fr_player_advance(fr_player_t *p,uint64_t elapsed) {
    fr_player_clip_t clip;
    if (!player_valid(p) || !fr_player_clip(p->action,&clip)) return false;
    if (p->action==FR_PLAYER_CHEW) {
        if (p->chew_recovering) {
            if (elapsed>=CHEW_RECOVERY_MS-p->elapsed_ms)
                return fr_player_set_action(p,FR_PLAYER_IDLE,false);
            p->elapsed_ms+=elapsed;
        } else {
            uint64_t remaining=p->elapsed_ms<CHEW_ENTRY_MS?CHEW_ENTRY_MS-p->elapsed_ms:0;
            if (elapsed<remaining) p->elapsed_ms+=elapsed;
            else p->elapsed_ms=CHEW_ENTRY_MS+
                ((p->elapsed_ms+remaining-CHEW_ENTRY_MS)%CHEW_LOOP_MS+
                 (elapsed-remaining)%CHEW_LOOP_MS)%CHEW_LOOP_MS;
        }
        return true;
    }
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
    if (p->action==FR_PLAYER_CHEW) {
        *out=p->chew_recovering?7u:1u+(uint32_t)(p->elapsed_ms/CHEW_FRAME_MS);
        return true;
    }
    uint64_t frame=p->elapsed_ms/clip.frame_ms;
    *out=clip.loop?(uint32_t)(frame%clip.frames):
        (frame>=clip.frames?clip.frames-1:(uint32_t)frame);
    return true;
}

bool fr_player_action_complete(const fr_player_t *p) {
    fr_player_clip_t clip;
    return player_valid(p) && p->action!=FR_PLAYER_CHEW &&
        fr_player_clip(p->action,&clip) && !clip.loop &&
        p->elapsed_ms>=(uint64_t)clip.frames*clip.frame_ms;
}
