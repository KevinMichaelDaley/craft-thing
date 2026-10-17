#ifndef DUNGEONCRAFT_BODY_STORAGE_LAYOUT_H
#define DUNGEONCRAFT_BODY_STORAGE_LAYOUT_H

#define DC_BODY_STORAGE_ABI 1u
#define DC_BODY_SLAB_SHIFT 6u
#define DC_BODY_SLAB_RECORDS (1u<<DC_BODY_SLAB_SHIFT)
#define DC_BODY_SLOT_NONE 0xffffffffu

/* Slab size is allocation/work granularity, not a bound on directory pages. */
#ifdef __STDC_VERSION__
#include <stdint.h>
typedef struct {uint32_t low,high;} dc_body_address_t;
typedef struct {
    dc_body_address_t directory;
    uint32_t live_count,slab_shift,abi_version,generation,slab_count,directory_capacity;
    dc_body_address_t data,geometry,work,requests;
} dc_body_storage_root_t;
typedef struct {
    dc_body_address_t records,solver,ownership,auxiliary;
    uint32_t free_mask[2],generation,flags;
} dc_body_storage_page_t;
#else
struct BodyStorageRoot {
    uvec2 directory;
    uint live_count,slab_shift,abi_version,generation,slab_count,directory_capacity;
    uvec2 data,geometry,work,requests;
};
struct BodyStoragePage {
    uvec2 records,solver,ownership,auxiliary;
    uvec2 free_mask;
    uint generation,flags;
};
#endif
#endif
