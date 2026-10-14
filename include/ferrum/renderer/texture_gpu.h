#ifndef FERRUM_RENDERER_TEXTURE_GPU_H
#define FERRUM_RENDERER_TEXTURE_GPU_H

#include "dungeoncraft/gpu.h"
#include "ferrum/renderer/texture.h"

typedef struct fr_texture_gpu fr_texture_gpu_t;

/** Allocate a fixed renderer-owned appearance channel; no physics buffer changes. */
bool fr_texture_gpu_create(dc_gpu_t *gpu, uint32_t capacity, fr_texture_gpu_t **out,
                            char *err, uint32_t cap);
/** Release renderer buffers/pipeline after all submitted work has completed. */
void fr_texture_gpu_destroy(fr_texture_gpu_t *textures);
/** Explicitly import immutable owners for generation; not a normal-frame copyback. */
bool fr_texture_gpu_upload(fr_texture_gpu_t *textures, const fr_appearance_owner_t *owners,
                            const uint32_t *styles, uint32_t count);
/** Generate one independent appearance record per GPU invocation. */
bool fr_texture_gpu_generate(fr_texture_gpu_t *textures, char *err, uint32_t cap);
/** Diagnostic readback of the separate linear color/surface channel. */
bool fr_texture_gpu_read(fr_texture_gpu_t *textures, uint32_t index,
                          fr_appearance_texel_t *texel);

#endif
