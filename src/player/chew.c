#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/player/chew.h"

static bool chew_target_valid(const fr_chew_target_t *t) {
    return t&&t->eligible&&t->parcel_id&&t->generation&&t->dry_mass_fp&&
        (t->material==DC_MATERIAL_WOOD||t->material==DC_MATERIAL_DENSE_WOOD);
}
static bool chew_same_target(const fr_chew_target_t *a,const fr_chew_target_t *b) {
    return a->parcel_id==b->parcel_id&&a->generation==b->generation&&
        a->material==b->material&&a->dry_mass_fp==b->dry_mass_fp;
}
bool fr_chew_work_advance(fr_chew_work_t *work,const fr_chew_target_t *target,
    uint32_t effort,uint64_t elapsed,bool held,bool *ready) {
    if(!work||!ready||!effort) return false;
    fr_chew_work_t next=*work;
    if(!chew_target_valid(target)) {
        memset(&next,0,sizeof(next));*work=next;*ready=false;return true;
    }
    if(!chew_same_target(&next.target,target)) next=(fr_chew_work_t){.target=*target};
    uint64_t required=(uint64_t)target->dry_mass_fp*1000;
    if(next.work>required) return false;
    if(held) {
        uint64_t missing=required-next.work;
        uint64_t milliseconds=missing/effort+(missing%effort!=0);
        if(elapsed>=milliseconds) next.work=required;
        else next.work+=elapsed*effort;
    }
    *ready=held&&next.work==required;*work=next;return true;
}
bool fr_chew_work_committed(fr_chew_work_t *work,const fr_chew_target_t *target) {
    if(!work||!chew_target_valid(target)||!chew_same_target(&work->target,target)||
       work->work!=(uint64_t)target->dry_mass_fp*1000) return false;
    memset(work,0,sizeof(*work));return true;
}
