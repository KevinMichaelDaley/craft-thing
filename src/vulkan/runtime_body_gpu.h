#ifndef DC_RUNTIME_BODY_GPU_H
#define DC_RUNTIME_BODY_GPU_H
#include "gpu_internal.h"
#include "dungeoncraft/body_storage_layout.h"
#include <stdio.h>
#include <string.h>

/** Create the versioned root while retaining the legacy snapshot metadata basis. */
bool dc_gpu_runtime_bodies_init(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Release addressed resources after all submissions have completed. */
void dc_gpu_runtime_bodies_destroy(dc_gpu_t *gpu);
/** Fence-safe resource growth; no body geometry is copied or solved on the host. */
bool dc_gpu_runtime_bodies_reserve(dc_gpu_t *gpu,uint32_t slots,char *err,uint32_t cap);
/** Consume only the completed constant-size allocation mailbox. */
bool dc_gpu_runtime_bodies_service(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Build GPU ID lookup, live count, generation ownership and page free masks. */
void dc_gpu_runtime_record_body_index(dc_gpu_t *gpu);
/** Dynamic work buffer for GPU-produced extraction dispatch triplets. */
VkBuffer dc_gpu_runtime_body_work_buffer(const dc_gpu_t *gpu);
/** Inclusive GPU pair scan; input and result use the root's first scan plane. */
void dc_gpu_runtime_record_prefix(dc_gpu_t *gpu,uint32_t count,uint32_t indirect_word);
/** Explicit completed-state access for diagnostics and checkpoint transactions. */
uint32_t *dc_gpu_body_solver_state(const dc_gpu_t *gpu,uint32_t slot);
/** Explicit completed hydrodynamic state for diagnostics. */
uint32_t *dc_gpu_body_hydro_state(const dc_gpu_t *gpu,uint32_t slot);
dc_body_storage_owner_t *dc_gpu_body_owner(const dc_gpu_t *gpu,uint32_t slot);
/** Mirror the legacy diagnostic codec only; ordinary physics uses addressed state. */
void dc_gpu_runtime_record_legacy_state(dc_gpu_t *gpu);
/** GPU-only source-size preflight before an explicit carving transaction. */
bool dc_gpu_runtime_prepare_fragments(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Consume a completed carving resource-retry flag without reading geometry. */
bool dc_gpu_runtime_carve_retry(dc_gpu_t *gpu);
/** Descriptor for the root when runtime consumers are enabled. */
VkDescriptorBufferInfo dc_gpu_runtime_bodies_binding(const dc_gpu_t *gpu);
/** Select an addressed shader variant for a production body consumer. */
static inline const char *dc_gpu_runtime_body_shader(const dc_gpu_t *g,const char *path,char output[1024]) {
    if(!g||!g->runtime_body_consumers) return path;
    const char *name=strrchr(path,'/');name=name?name+1:path;
    const char *consumers[]={"pattern.comp.spv","rigid.comp.spv","rigid_solver.comp.spv","contacts.comp.spv",
        "broadphase.comp.spv","component_extract.comp.spv","component_local.comp.spv","structural.comp.spv",
        "pixel_hydro.comp.spv","mpm.comp.spv","mpm_active.comp.spv","mpm_compact.comp.spv","tick_probe.comp.spv","scene.comp.spv"};
    for(unsigned i=0;i<sizeof(consumers)/sizeof(*consumers);i++)
        if(!strcmp(name,consumers[i])) {
            int n=snprintf(output,1024,"build/shaders/runtime/%s",name);
            return n>0&&n<1024?output:path;
        }
    return path;
}
#endif
