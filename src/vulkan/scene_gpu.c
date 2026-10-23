#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/renderer/gpu.h"
#include "ferrum/renderer/light_transport.h"
#include "ferrum/renderer/texture.h"
#include "renderer_internal.h"
#include "runtime_body_gpu.h"

enum { SCENE_PHASES = 8, SCENE_BUFFERS = 5, SCENE_WORK_WORDS = 128 + 4096 };
struct fr_scene_gpu {
  fr_renderer_mode_t mode;
  uint32_t light_revision, point_count;
  dc_chunk_coord_t source_origin, last_origin;
  fr_renderer_point_light_t points[16];
  fr_renderer_sun_t sun;
  fr_occlusion_gpu_t *occlusion;
  fr_lighting_gpu_t *lighting;
  fr_renderer_compute_t compute;
  VkPipeline phases[SCENE_PHASES];
  VkBuffer buffers[SCENE_BUFFERS];
  VkDeviceMemory memory[SCENE_BUFFERS];
  void *mapped[SCENE_BUFFERS];
};

void fr_renderer_rebind_colors(dc_gpu_t *g) {
  if(!g||!g->renderer) return;
  VkDescriptorBufferInfo b[3]={{g->shape_color_buffer,0,dc_gpu_color_terrain_bytes()},
    {g->shape_color_buffer,0,16},{g->shape_color_buffer,0,16}};
  VkWriteDescriptorSet w[3]={0};
  for(uint32_t i=0;i<3;i++) w[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet=g->renderer->compute.descriptor,.dstBinding=33+i,.descriptorCount=1,
    .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=b+i};
  vkUpdateDescriptorSets(g->device,3,w,0,NULL);
    g->descriptor_revision++;
}

bool fr_renderer_active(const dc_gpu_t *g) {
  return g && g->renderer && g->renderer->mode != FR_RENDERER_REFERENCE &&
         g->overlay == DC_GPU_OVERLAY_NONE;
}
void fr_renderer_destroy(dc_gpu_t *g) {
  if (!g || !g->renderer)
    return;
  struct fr_scene_gpu *s = g->renderer;
  for (uint32_t p = 1; p < SCENE_PHASES; p++)
    if (s->phases[p])
      vkDestroyPipeline(g->device, s->phases[p], NULL);
  fr_renderer_compute_destroy(g, &s->compute);
  fr_lighting_gpu_destroy(s->lighting);
  fr_occlusion_gpu_destroy(s->occlusion);
  for (uint32_t b = 0; b < SCENE_BUFFERS; b++) {
    if (s->mapped[b])
      vkUnmapMemory(g->device, s->memory[b]);
    if (s->buffers[b])
      vkDestroyBuffer(g->device, s->buffers[b], NULL);
    if (s->memory[b])
      vkFreeMemory(g->device, s->memory[b], NULL);
  }
  free(s);
  g->renderer = NULL;
}
static bool scene_pipelines(dc_gpu_t *g, char *err, uint32_t cap) {
  struct fr_scene_gpu *s = g->renderer;
  VkShaderModule shader;
  if (!dc_gpu_load_shader_module(g, "build/shaders/scene.comp.spv", &shader,
                                 err, cap))
    return false;
  s->phases[0] = s->compute.pipeline;
  bool okay = true;
  VkSpecializationMapEntry entry = {0, 0, sizeof(uint32_t)};
  for (uint32_t phase = 1; phase < SCENE_PHASES; phase++) {
    VkSpecializationInfo specialization = {1, &entry, sizeof(phase), &phase};
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = shader,
                  .pName = "main",
                  .pSpecializationInfo = &specialization},
        .layout = s->compute.pipeline_layout};
    if (vkCreateComputePipelines(g->device, VK_NULL_HANDLE, 1, &info, NULL,
                                 &s->phases[phase]) != VK_SUCCESS) {
      okay = false;
      break;
    }
  }
  vkDestroyShaderModule(g->device, shader, NULL);
  return okay;
}
bool fr_renderer_enable(dc_gpu_t *g, char *err, uint32_t cap) {
  if (!g)
    return false;
  if (g->renderer)
    return true;
  struct fr_scene_gpu *s = calloc(1, sizeof(*s));
  if (!s)
    return false;
  g->renderer = s;
  s->mode = FR_RENDERER_LIT;
  s->light_revision = 1;
  s->source_origin = s->last_origin = g->body_origin;
  uint32_t grid = g->width * g->height,
           atlas = DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS;
  VkDeviceSize sizes[SCENE_BUFFERS] = {
      (VkDeviceSize)atlas * 32, (VkDeviceSize)atlas * 8,
      (VkDeviceSize)grid * FR_RENDERER_SURFACE_BYTES, SCENE_WORK_WORDS * 4, 64};
  for (uint32_t b = 0; b < SCENE_BUFFERS; b++)
    if (!dc_gpu_make_mapped_buffer(g, sizes[b], &s->buffers[b], &s->memory[b],
                                   &s->mapped[b], err, cap))
      goto fail;
  memset(s->mapped[0],0,(size_t)sizes[0]);memset(s->mapped[1],0,(size_t)sizes[1]);
  if (!fr_occlusion_gpu_create(g, g->width, g->height, &s->occlusion, err,
                               cap) ||
      !fr_lighting_gpu_create(g, s->occlusion, &s->lighting, err, cap))
    goto fail;
  if (!fr_lighting_gpu_set_sources(s->lighting, NULL, 0, (fr_renderer_sun_t){0},
                                   1024))
    goto fail;
  VkDescriptorBufferInfo buffers[36] = {
      {g->chunk_buffer, 0, (VkDeviceSize)atlas * sizeof(dc_cell_t)},
      {g->page_buffer, 0, (VkDeviceSize)g->page_width * g->page_height * 4},
      {g->occupancy_buffer, 0, (VkDeviceSize)grid * 20},
      dc_gpu_runtime_bodies_binding(g),
      {g->particle_buffer, 0,
       (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MPM_PARTICLES_PER_CHUNK *
           sizeof(dc_mpm_particle_t)},
      {s->buffers[0], 0, sizes[0]},
      {s->buffers[1], 0, sizes[1]},
      {s->buffers[2], 0, sizes[2]},
      {VK_NULL_HANDLE, 0, 0},
      {s->buffers[3], 0, sizes[3]},
      {g->marker_grid_buffer, 0, (VkDeviceSize)atlas * 4},
      {g->cells, 0, (VkDeviceSize)grid * 4},
      {g->fluid_previous_buffer, 0, (VkDeviceSize)grid * 4},
      {VK_NULL_HANDLE, 0, 0},
      {s->buffers[4], 0, sizes[4]},
      {g->water_color_buffer,0,(VkDeviceSize)atlas*16}};
  for(uint32_t i=16;i<33;i++) buffers[i]=(VkDescriptorBufferInfo){g->cells,0,(VkDeviceSize)grid*4};
  VkDeviceSize terrain_colors=dc_gpu_color_terrain_bytes(),bank_colors=dc_gpu_color_bank_bytes(g);
  buffers[33]=(VkDescriptorBufferInfo){g->shape_color_buffer,0,terrain_colors};
  buffers[34]=(VkDescriptorBufferInfo){g->shape_color_buffer,g->runtime_body_consumers?0:terrain_colors,g->runtime_body_consumers?16:bank_colors};
  buffers[35]=(VkDescriptorBufferInfo){g->shape_color_buffer,g->runtime_body_consumers?0:terrain_colors+bank_colors,g->runtime_body_consumers?16:bank_colors};
  if (!fr_occlusion_input_binding(s->occlusion, &buffers[8]) ||
      !fr_lighting_field_binding(s->lighting, &buffers[13]) ||
      !fr_renderer_compute_init(g, &s->compute, buffers, 36, 32,
                                "build/shaders/scene.comp.spv", err, cap) ||
      !scene_pipelines(g, err, cap))
    goto fail;
  return true;
fail:
  fr_renderer_destroy(g);
  return false;
}
bool fr_renderer_set_mode(dc_gpu_t *g, fr_renderer_mode_t mode) {
  if (!g || !g->renderer || mode > FR_RENDERER_REFERENCE)
    return false;
  g->renderer->mode = mode;
  return true;
}
bool fr_renderer_authored_bindings(dc_gpu_t *g, VkDescriptorBufferInfo buffers[3]) {
  if (!g || !g->renderer || !buffers) return false;
  struct fr_scene_gpu *s = g->renderer;
  VkDeviceSize atlas = (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS;
  buffers[0] = (VkDescriptorBufferInfo){s->buffers[0], 0, atlas * 32};
  buffers[1] = (VkDescriptorBufferInfo){s->buffers[1], 0, atlas * 8};
  buffers[2] = (VkDescriptorBufferInfo){s->buffers[4], 0, 64};
  return true;
}
bool fr_renderer_set_material_style(dc_gpu_t *g, uint32_t material,
                                    uint32_t style) {
  if (!g || !g->renderer || material > DC_MATERIAL_LEAF_DEBRIS ||
      style > FR_TEXTURE_BRICK)
    return false;
  ((uint32_t *)g->renderer->mapped[4])[4 + material] = style;
  return true;
}
bool fr_renderer_set_sources(dc_gpu_t *g,
                             const fr_renderer_point_light_t *points,
                             uint32_t count, fr_renderer_sun_t sun) {
  if (!g || !g->renderer ||
      !fr_lighting_gpu_set_sources(g->renderer->lighting, points, count, sun,
                                   1024))
    return false;
  struct fr_scene_gpu *s = g->renderer;
  if (count)
    memcpy(s->points, points, count * sizeof(*points));
  s->point_count = count;
  s->sun = sun;
  s->source_origin = s->last_origin = g->body_origin;
  s->light_revision++;
  return true;
}
static void scene_rebase_sources(dc_gpu_t *g) {
  struct fr_scene_gpu *s = g->renderer;
  if (s->last_origin.x == g->body_origin.x &&
      s->last_origin.y == g->body_origin.y)
    return;
  long double dx =
      ((long double)s->source_origin.x - (long double)g->body_origin.x) * 64;
  long double dy =
      ((long double)s->source_origin.y - (long double)g->body_origin.y) * 64;
  fr_renderer_point_light_t points[16];
  for (uint32_t i = 0; i < s->point_count; i++) {
    points[i] = s->points[i];
    points[i].x = (float)fminl(1e10L, fmaxl(-1e10L, dx + points[i].x));
    points[i].y = (float)fminl(1e10L, fmaxl(-1e10L, dy + points[i].y));
  }
  fr_lighting_gpu_set_sources(s->lighting, points, s->point_count, s->sun,
                              1024);
  s->last_origin = g->body_origin;
  s->light_revision++;
}
static void scene_barrier(dc_gpu_t *g) {
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                          VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                          VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
}
static void scene_dispatch(dc_gpu_t *g, uint32_t phase, uint32_t work,
                           const uint32_t push[8]) {
  struct fr_scene_gpu *s = g->renderer;
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    s->phases[phase]);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          s->compute.pipeline_layout, 0, 1,
                          &s->compute.descriptor, 0, NULL);
  vkCmdPushConstants(g->command, s->compute.pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, 32, push);
  vkCmdDispatch(g->command, (work + 63) / 64, 1, 1);
  scene_barrier(g);
}
void fr_renderer_record(dc_gpu_t *g) {
  dc_gpu_runtime_record_body_index(g);
  struct fr_scene_gpu *s = g->renderer;
  scene_rebase_sources(g);
  uint32_t *config = s->mapped[4];
  config[0] = g->view_width;
  config[1] = g->view_height;
  fr_renderer_barrier(
      g->command,
      VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  uint32_t push[8] = {g->width,
                      g->height,
                      dc_gpu_structural_word_offset(g),
                      s->light_revision,
                      dc_gpu_render_flags(g),
                      (uint32_t)s->mode,
                      g->view_x,
                      g->view_y};
  scene_dispatch(g, 0, g->runtime_body_consumers?64:4096, push);
  if(!g->runtime_body_consumers) scene_dispatch(g, 1, 64, push);
  scene_dispatch(g, 2, DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS, push);
  if(s->mode==FR_RENDERER_ALBEDO) {
    scene_dispatch(g,6,g->view_width*g->view_height,push);
    return;
  }
  scene_dispatch(g, 3, g->width * g->height, push);
  scene_dispatch(g, 4, 32, push);
  if (s->mode == FR_RENDERER_LIT) {
    fr_lighting_record(s->lighting, s->buffers[3], sizeof(uint32_t));
    scene_barrier(g);
  }
  scene_dispatch(g, 5, g->view_width * g->view_height, push);
}
bool fr_renderer_render(dc_gpu_t *g, char *err, uint32_t cap) {
  return g && g->renderer && dc_gpu_render_chunks(g, err, cap);
}
bool fr_renderer_read_opacity(dc_gpu_t *g,uint32_t x,uint32_t y,
                              fr_occlusion_node_t *node) {
  if(g&&g->renderer&&g->renderer->mode==FR_RENDERER_ALBEDO) {
    if(x>=g->width||y>=g->height||!node||!fr_renderer_begin(g)) return false;
    uint32_t push[8]={g->width,g->height,dc_gpu_structural_word_offset(g),
        y*g->width+x,dc_gpu_render_flags(g),FR_RENDERER_ALBEDO,g->view_x,g->view_y};
    scene_dispatch(g,7,1,push);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(!fr_renderer_submit(g)) return false;
  }
  return g&&g->renderer&&fr_occlusion_input_read(g->renderer->occlusion,x,y,node);
}
