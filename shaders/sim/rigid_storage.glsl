#ifndef DC_RIGID_STORAGE_GLSL
#define DC_RIGID_STORAGE_GLSL
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#include "../../include/dungeoncraft/body_storage_layout.h"

layout(buffer_reference,std430,buffer_reference_align=16) buffer BodyStorageRecords {Body records[];};
layout(buffer_reference,std430,buffer_reference_align=8) buffer BodyStorageDirectory {BodyStoragePage pages[];};
layout(buffer_reference,std430,buffer_reference_align=4) buffer BodyStorageWords {uint words[];};
layout(buffer_reference,std430,buffer_reference_align=8) buffer BodyStorageOwners {BodyStorageOwner owners[];};

bool body_storage_root_valid(BodyStorageRoot root) {
    return root.abi_version==DC_BODY_STORAGE_ABI&&root.slab_shift==DC_BODY_SLAB_SHIFT&&
        root.slab_count>0u&&root.directory_capacity>=root.slab_count&&any(notEqual(root.directory,uvec2(0)));
}
bool body_storage_slot_valid(BodyStorageRoot root,uint slot) {
    return body_storage_root_valid(root)&&slot!=DC_BODY_SLOT_NONE&&(slot>>root.slab_shift)<root.slab_count;
}
/* Callers validate their handle/list bounds before resolving an lvalue. */
#define DC_BODY_PAGE(root,slot) BodyStorageDirectory((root).directory).pages[(slot)>>(root).slab_shift]
#define DC_BODY_AT(root,slot) BodyStorageRecords(DC_BODY_PAGE(root,slot).records).records[(slot)&((1u<<(root).slab_shift)-1u)]
#define DC_BODY_DATA(root,index) BodyStorageWords((root).data).words[(index)]
#define DC_BODY_OWNER(root,slot) BodyStorageOwners(DC_BODY_PAGE(root,slot).ownership).owners[(slot)&((1u<<(root).slab_shift)-1u)]
bool body_storage_handle_valid(BodyStorageRoot root,uvec2 handle) {
    if(!body_storage_slot_valid(root,handle.x)||handle.y==0u) return false;
    BodyStorageOwner owner=DC_BODY_OWNER(root,handle.x);
    return owner.generation==handle.y&&(owner.state==DC_BODY_OWNER_RESERVED||owner.state==DC_BODY_OWNER_LIVE);
}
#endif
