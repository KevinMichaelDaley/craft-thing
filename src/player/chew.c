#include "ferrum/player/chew.h"

bool fr_chew_work_advance(fr_chew_work_t *work,const fr_chew_target_t *target,
    uint32_t effort,uint64_t elapsed,bool held,bool *ready) {
    (void)work;(void)target;(void)effort;(void)elapsed;(void)held;(void)ready;
    return false;
}
bool fr_chew_work_committed(fr_chew_work_t *work,const fr_chew_target_t *target) {
    (void)work;(void)target;return false;
}
