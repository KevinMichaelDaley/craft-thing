#ifndef DUNGEONCRAFT_BODY_STORAGE_LAYOUT_H
#define DUNGEONCRAFT_BODY_STORAGE_LAYOUT_H

#define DC_BODY_STORAGE_ABI 1u
#define DC_BODY_SLAB_SHIFT 6u
#define DC_BODY_SLAB_RECORDS (1u<<DC_BODY_SLAB_SHIFT)
#define DC_BODY_SLOT_NONE 0xffffffffu
#define DC_BODY_MOTION_REACTIVE_MASS 4u
#define DC_BODY_MOTION_THERMAL_GRID 8u
#define DC_BODY_OWNER_FREE 0u
#define DC_BODY_OWNER_RESERVED 1u
#define DC_BODY_OWNER_LIVE 2u
#define DC_BODY_OWNER_RETIRED 3u
#define DC_BODY_ALLOCATION_NO_CAPACITY 1u
#define DC_BODY_ALLOCATION_INVALID_HANDLE 2u
#define DC_BODY_ANCHOR_BITS_WORD 47u
#define DC_BODY_ANCHOR_MAGIC_WORD 48u
#define DC_BODY_ANCHOR_SKIPS_WORD 49u
#define DC_BODY_ANCHOR_MAGIC 0x414e4331u

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
typedef struct {
    uint32_t generation,runtime_id,state,live_index;
    dc_body_address_t source_transaction;
    uint32_t source_object,flags;
} dc_body_storage_owner_t;
typedef struct {
    uint32_t count,ready,required_slots,error,committed,commit;
    dc_body_address_t transaction;
} dc_body_allocation_batch_t;
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
struct BodyStorageOwner {
    uint generation,runtime_id,state,live_index;
    uvec2 source_transaction;
    uint source_object,flags;
};
struct BodyAllocationBatch {
    uint count,ready,required_slots,error,committed,commit;
    uvec2 transaction;
};
#endif
#endif
