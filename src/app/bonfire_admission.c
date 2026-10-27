#include "bonfire.h"
bool dc_bonfire_admit(dc_gpu_t *gpu,char *err,uint32_t cap) {
    dc_bonfire_plan_t plan;
    return dc_bonfire_plan(&plan)&&fr_gpu_place_structure(gpu,&plan.pit,err,cap)&&
        dc_gpu_admit_authored_rigid_cells(gpu,err,cap);
}
