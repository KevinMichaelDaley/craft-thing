#ifndef BONFIRE_PROGRESS_H
#define BONFIRE_PROGRESS_H
#include <stdbool.h>
typedef struct {
    double started,late,previous_chemical,late_chemical;
    bool continuous,sustained;
} bonfire_progress_t;
static inline bonfire_progress_t bonfire_progress_init(void) {
    return (bonfire_progress_t){.started=-1.,.late=-1.,.continuous=true};
}
static inline bool bonfire_progress_observe(bonfire_progress_t *p,double seconds,
    double chemical,unsigned active,bool all_ignited) {
    if(p->started>=0.&&p->previous_chemical>0.&&chemical>=p->previous_chemical-.000001)
        p->continuous=false;
    p->previous_chemical=chemical;
    if(p->started<0.&&all_ignited) p->started=seconds;
    if(p->started>=0.&&p->late<0.&&seconds>=p->started+15.) {
        p->late=seconds;p->late_chemical=chemical;
    }
    if(p->late>=0.&&seconds>=p->late+5.)
        p->sustained=p->continuous&&active>0u&&p->late_chemical-chemical>=10.;
    return all_ignited&&p->sustained;
}
#endif
