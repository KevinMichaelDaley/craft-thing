#ifndef DC_FIRE_RAY_RENDERER_H
#define DC_FIRE_RAY_RENDERER_H

#include "gpu_internal.h"

/** Attach the authoritative trace channel, or restore the empty binding. */
void fr_renderer_bind_fire_ray(dc_gpu_t *gpu,const VkDescriptorBufferInfo *buffer);

#endif
