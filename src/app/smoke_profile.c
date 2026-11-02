#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>

#include "smoke_profile.h"

bool dc_smoke_profile_defaults(void) {
    static const char *const options[]={
        "DC_GAS_PRACTICAL","DC_GAS_DEVICE_WORK","DC_GAS_DEVICE_STATE","DC_GAS_FIXED_AXIS",
        "DC_GAS_FUSED_HYBRID","DC_GAS_DIRECT_FIRST_AXIS","DC_GAS_GROUP_RATE","DC_GAS_HALF_PRESSURE",
        "DC_GAS_MULTIGRID","DC_GAS_FUSED_METADATA","DC_GAS_TEMPORAL_SOR","DC_GAS_COARSE_WG",
        "DC_GAS_COARSE","DC_GAS_ROTATE_PUBLISH","DC_GAS_HYBRID_LINES","DC_GAS_FUSED_AXIS",
        "DC_GAS_FUSED_APPLY","DC_GAS_WIDE_SCAN","DC_GAS_LOCAL_GEOMETRY","DC_GAS_SPARSE_OWNERS",
        "DC_GAS_FUSED_GEOMETRY","DC_GAS_LOCAL_GEOMETRY_ACTIVITY","DC_GAS_FUSED_TICK",
        "DC_GAS_PRESSURE_NOISE","DC_GAS_RHS_TILE_CACHE","DC_GAS_EXPLICIT_RATE_CACHE",
        "DC_GAS_LAZY_CLEAR","DC_GAS_VORTICITY","DC_GAS_SUBGRID"
    };
    const char *legacy=getenv("DC_GAS_LOCAL_STAGES");
    if(!getenv("DC_GAS_EXPLICIT_RATE_CACHE")&&legacy&&setenv("DC_GAS_EXPLICIT_RATE_CACHE",legacy,0)!=0) return false;
    for(unsigned n=0;n<sizeof(options)/sizeof(options[0]);n++)
        if(!getenv(options[n])&&setenv(options[n],"1",0)!=0) return false;
    return getenv("DC_GAS_SOR_TILE")||setenv("DC_GAS_SOR_TILE","32",0)==0;
}
