#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/player/chew.h"
#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gpu.h"


enum { CHEW_REACH_CELLS = 12u, CHEW_RATE_PER_SECOND_FP = 300u };

void fr_player_inventory_init(fr_player_inventory_t *inventory) {
    if (!inventory) return;
    memset(inventory, 0, sizeof(*inventory));
}

uint64_t fr_player_inventory_mass(const fr_player_inventory_t *inventory,
                                  dc_item_kind_t kind) {
    if (!inventory || kind == 0u || kind >= 8u) return 0u;
    return inventory->mass_fp[kind];
}



static dc_item_kind_t contract_wood_kind(uint16_t material) {
    return material == DC_MATERIAL_WOOD ? DC_ITEM_WOOD :
           material == DC_MATERIAL_DENSE_WOOD ? DC_ITEM_DENSE_WOOD : 0u;
}

bool fr_chew_work_advance(fr_chew_work_t *work,const fr_chew_target_t *target,
    uint32_t effort,uint64_t delta_ms,bool chewing,bool *bite_ready) {
    if(!work||!bite_ready||effort==0u) return false;
    if(!target) {
        work->work=0u;work->armed=false;
        *bite_ready=false;return true;
    }
    dc_item_kind_t kind=contract_wood_kind(target->material);
    bool eligible=target->reachable&&target->eligible&&kind!=0u;
    if(!eligible) {
        work->work=0u;work->parcel_id=target->parcel_id;
        work->generation=target->generation;
        work->dry_mass_fp=target->dry_mass_fp;
        work->material=target->material;work->armed=false;
        *bite_ready=false;return true;
    }
    if(work->parcel_id!=target->parcel_id||
       work->generation!=target->generation||
       work->dry_mass_fp!=target->dry_mass_fp||
       work->material!=target->material) {
        work->work=0u;work->parcel_id=target->parcel_id;
        work->generation=target->generation;
        work->dry_mass_fp=target->dry_mass_fp;
        work->material=target->material;work->armed=false;
    }
    uint64_t threshold=(uint64_t)target->dry_mass_fp*1000u;
    if(work->armed) {*bite_ready=true;return true;}
    if(chewing&&delta_ms) {
        uint64_t add=threshold;
        if(delta_ms!=0u&&effort<=UINT64_MAX/delta_ms) {
            add=effort*delta_ms;
            if(add>threshold) add=threshold;
        }
        work->work+=add;
        if(work->work>threshold) work->work=threshold;
    }
    if(work->work>=threshold) {
        work->work=threshold;work->armed=true;*bite_ready=true;
    } else *bite_ready=false;
    return true;
}

bool fr_chew_work_committed(fr_chew_work_t *work,const fr_chew_target_t *target) {
    if(!work||!target||!work->armed||work->parcel_id!=target->parcel_id||
       work->generation!=target->generation||
       work->dry_mass_fp!=target->dry_mass_fp)
        return false;
    work->armed=false;work->work=0u;
    return true;
}
