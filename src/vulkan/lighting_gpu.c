#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/renderer/light_transport.h"
#include "ferrum/renderer/lighting_gpu.h"

_Static_assert(sizeof(fr_renderer_point_light_t) == 32,
               "Point light GPU layout must be two vec4 values");
_Static_assert(sizeof(fr_renderer_light_field_t) == 64,
               "Public lighting diagnostic layout must be four vec4 values");
#include "renderer_internal.h"

enum { LIGHTING_MAX_PASSES = 64 };
typedef struct {
  uint32_t phase, level, iteration, groups, directions;
} light_pass_t;

struct fr_lighting_gpu {
  dc_gpu_t *gpu;
  uint32_t width, height;
  VkBuffer parameters, sources, field;
  VkDeviceMemory parameter_memory, source_memory, field_memory;
  void *parameter_mapped, *source_mapped, *field_mapped;
  fr_renderer_compute_t compute;
  VkBuffer transport[5];
  VkDeviceMemory transport_memory[5];
  void *work_mapped;
  light_pass_t passes[LIGHTING_MAX_PASSES];
  uint32_t pass_count;
  uint32_t diagonal_groups;
  VkPipeline specialized[2][FR_TRANSPORT_SOURCE_ITERATIONS][7];
  bool compact_available, common_direction;
};
static uint32_t tiles(uint32_t length) {
  return (length + FR_TRANSPORT_TILE - 1u) / FR_TRANSPORT_TILE;
}
static uint32_t larger(uint32_t a, uint32_t b) { return a > b ? a : b; }
static uint32_t smaller(uint32_t a, uint32_t b) { return a < b ? a : b; }
static uint32_t block_nodes(uint32_t length, uint32_t lines) {
  uint32_t count = tiles(length), sum = count;
  while (count > 1u) {
    count = tiles(count);
    sum += count;
  }
  return sum * lines;
}
static void add_pass(fr_lighting_gpu_t *l, uint32_t phase, uint32_t level,
                     uint32_t iteration, uint32_t groups, uint32_t directions) {
  l->passes[l->pass_count++] =
      (light_pass_t){phase, level, iteration, groups, directions};
}
static void plan_transport(fr_lighting_gpu_t *l) {
  add_pass(l, 5, 0, 0, tiles(l->width * l->height), 1);
  add_pass(l, 6, 0, 0, 0, 0);
  for (uint32_t iteration = 0; iteration < FR_TRANSPORT_SOURCE_ITERATIONS;
       iteration++) {
    bool diagonal = iteration == 1u;
    uint32_t horizontal[8], vertical[8], levels = 0;
    uint32_t rows = diagonal ? l->width + l->height - 1u : l->height;
    uint32_t columns = diagonal ? rows : l->width;
    horizontal[0] = tiles(diagonal ? smaller(l->width, l->height) : l->width);
    vertical[0] = tiles(diagonal ? smaller(l->width, l->height) : l->height);
    while (horizontal[levels] > 1u || vertical[levels] > 1u) {
      horizontal[levels + 1u] = tiles(horizontal[levels]);
      vertical[levels + 1u] = tiles(vertical[levels]);
      levels++;
    }
    add_pass(l, 0, 0, iteration,
             diagonal ? l->diagonal_groups :
             larger(rows * horizontal[0], columns * vertical[0]), 4);
    for (uint32_t level = 0; level < levels; level++)
      add_pass(l, 1, level, iteration,
               larger(rows * tiles(horizontal[level]),
                      columns * tiles(vertical[level])),
               4);
    for (int level = (int)levels - 2; level >= 0; level--)
      add_pass(
          l, 2, (uint32_t)level, iteration,
          tiles(larger(rows * horizontal[level], columns * vertical[level])),
          4);
    add_pass(l, 3, 0, iteration, tiles(l->width * l->height), 1);
  }
}
uint32_t fr_lighting_gpu_scan_groups(const fr_lighting_gpu_t *l,
                                      uint32_t iteration) {
  if (!l)
    return 0;
  for (uint32_t i = 0; i < l->pass_count; i++)
    if (l->passes[i].phase == 0u && l->passes[i].iteration == iteration)
      return l->passes[i].groups;
  return 0;
}
uint32_t fr_lighting_gpu_beam_stride(const fr_lighting_gpu_t *l) {
  return l ? (l->common_direction ? 16u : FR_TRANSPORT_BEAM_BYTES) : 0u;
}
uint32_t fr_lighting_gpu_scan_channels(const fr_lighting_gpu_t *l) {
  return l ? (l->common_direction ? 4u : 7u) : 0u;
}
uint32_t fr_lighting_gpu_source_cells(const fr_lighting_gpu_t *l) {
  if (!l) return 0u;
  uint64_t cells=(uint64_t)l->passes[1].groups*FR_TRANSPORT_TILE*l->passes[1].directions;
  return cells>UINT32_MAX?UINT32_MAX:(uint32_t)cells;
}
uint32_t fr_lighting_gpu_source_builds(const fr_lighting_gpu_t *l) {
  return l ? ((const uint32_t *)l->work_mapped)[2] : 0u;
}
void fr_lighting_gpu_destroy(fr_lighting_gpu_t *l) {
  if (!l)
    return;
  VkDevice d = l->gpu->device;
  for (unsigned compact = 0; compact < 2; compact++)
  for (unsigned iteration = 0; iteration < FR_TRANSPORT_SOURCE_ITERATIONS;
       iteration++)
    for (unsigned phase = 0; phase < 7; phase++)
      if (l->specialized[compact][iteration][phase])
        vkDestroyPipeline(d, l->specialized[compact][iteration][phase], NULL);
  fr_renderer_compute_destroy(l->gpu, &l->compute);
  if (l->work_mapped)
    vkUnmapMemory(d, l->transport_memory[3]);
  for (unsigned i = 0; i < 5; i++) {
    if (l->transport[i])
      vkDestroyBuffer(d, l->transport[i], NULL);
    if (l->transport_memory[i])
      vkFreeMemory(d, l->transport_memory[i], NULL);
  }
  if (l->parameter_mapped)
    vkUnmapMemory(d, l->parameter_memory);
  if (l->source_mapped)
    vkUnmapMemory(d, l->source_memory);
  if (l->field_mapped)
    vkUnmapMemory(d, l->field_memory);
  if (l->parameters)
    vkDestroyBuffer(d, l->parameters, NULL);
  if (l->sources)
    vkDestroyBuffer(d, l->sources, NULL);
  if (l->field)
    vkDestroyBuffer(d, l->field, NULL);
  if (l->parameter_memory)
    vkFreeMemory(d, l->parameter_memory, NULL);
  if (l->source_memory)
    vkFreeMemory(d, l->source_memory, NULL);
  if (l->field_memory)
    vkFreeMemory(d, l->field_memory, NULL);
  free(l);
}
bool fr_lighting_gpu_create(dc_gpu_t *g, fr_occlusion_gpu_t *c,
                            fr_lighting_gpu_t **out, char *err, uint32_t cap) {
  if (out)
    *out = NULL;
  if (!g || !c || !out)
    return false;
  fr_lighting_gpu_t *l = calloc(1, sizeof(*l));
  if (!l)
    return false;
  l->gpu = g;
  l->compact_available = getenv("DC_LIGHTING_GENERAL_TRANSPORT") == NULL;
  uint32_t layout[136] = {0};
  VkDescriptorBufferInfo buffers[9];
  if (!fr_occlusion_gpu_layout(c, &buffers[0], layout) ||
      !fr_occlusion_input_binding(c, &buffers[0]))
    goto fail;
  l->width = layout[0];
  l->height = layout[1];
  if ((uint64_t)l->width * l->height > UINT32_MAX / 4u ||
      l->width > UINT32_MAX - 127u || l->height > UINT32_MAX - 127u)
    goto fail;
  layout[6] = block_nodes(l->width, l->height);
  layout[7] = block_nodes(l->height, l->width);
  uint64_t diagonal_groups = 0;
  for (uint32_t along = 0; along < smaller(l->width,l->height);
       along += FR_TRANSPORT_TILE)
    diagonal_groups += (uint64_t)l->width + l->height - 1u - 2u * along;
  if (diagonal_groups > (UINT32_MAX - 136u) / 2u)
    goto fail;
  l->diagonal_groups = (uint32_t)diagonal_groups;
  VkDeviceSize parameter_bytes =
      ((VkDeviceSize)136u + 2u * l->diagonal_groups) * sizeof(uint32_t);
  uint32_t total_nodes = larger(2u * layout[6] + 2u * layout[7],
                                4u * block_nodes(smaller(l->width, l->height),
                                                 l->width + l->height - 1u));
  plan_transport(l);
  VkDeviceSize transport_sizes[5] = {
      (VkDeviceSize)l->width * l->height * 4u * FR_TRANSPORT_BEAM_BYTES,
      (VkDeviceSize)l->width * l->height * FR_TRANSPORT_BEAM_BYTES,
      (VkDeviceSize)total_nodes * FR_TRANSPORT_BEAM_BYTES,
      (4u + 4u * l->pass_count) * sizeof(uint32_t),
      (VkDeviceSize)l->width * l->height * FR_TRANSPORT_BEAM_BYTES};
  for (unsigned i = 0; i < 5; i++) {
    bool okay =
        i == 3u
            ? dc_gpu_make_mapped_buffer(g, transport_sizes[i], &l->transport[i],
                                        &l->transport_memory[i],
                                        &l->work_mapped, err, cap)
            : dc_gpu_make_device_buffer(g, transport_sizes[i], &l->transport[i],
                                        &l->transport_memory[i], err, cap);
    if (!okay)
      goto fail;
    buffers[4u + i] =
        (VkDescriptorBufferInfo){l->transport[i], 0, transport_sizes[i]};
  }
  uint32_t *commands = l->work_mapped;
  memset(commands, 0, (size_t)transport_sizes[3]);
  for (uint32_t i = 0; i < l->pass_count; i++) {
    uint32_t groups = l->passes[i].groups,
             x = groups < 65535u ? groups : 65535u;
    commands[4u + i * 4u] = commands[7u + i * 4u] = x;
    commands[5u + i * 4u] = (groups + 65534u) / 65535u;
    commands[6u + i * 4u] = l->passes[i].directions;
  }
  if (!dc_gpu_make_mapped_buffer(g, parameter_bytes, &l->parameters,
                                 &l->parameter_memory, &l->parameter_mapped,
                                 err, cap) ||
      !dc_gpu_make_mapped_buffer(g, FR_TRANSPORT_SOURCE_RECORDS * 32u, &l->sources, &l->source_memory,
                                 &l->source_mapped, err, cap) ||
      !dc_gpu_make_mapped_buffer(g, (VkDeviceSize)l->width * l->height * FR_RENDERER_FIELD_BYTES,
                                 &l->field, &l->field_memory, &l->field_mapped,
                                 err, cap))
    goto fail;
  memcpy(l->parameter_mapped, layout, sizeof(layout));
  /* Immutable geometry schedule: omit diagonals ending before each tile.
     No scene pixels or simulation state are read by this cold setup. */
  uint32_t *scan_map = (uint32_t *)l->parameter_mapped + 136u;
  for (uint32_t along = 0; along < smaller(l->width,l->height);
       along += FR_TRANSPORT_TILE) {
    for (uint32_t x = 0; x < l->width - along; x++) {
      *scan_map++ = x;
      *scan_map++ = along / FR_TRANSPORT_TILE;
    }
    for (uint32_t y = 1; y < l->height - along; y++) {
      *scan_map++ = l->width + y - 1u;
      *scan_map++ = along / FR_TRANSPORT_TILE;
    }
  }
  buffers[1] = (VkDescriptorBufferInfo){l->parameters, 0, parameter_bytes};
  buffers[2] = (VkDescriptorBufferInfo){l->sources, 0, FR_TRANSPORT_SOURCE_RECORDS * 32u};
  buffers[3] = (VkDescriptorBufferInfo){
      l->field, 0, (VkDeviceSize)l->width * l->height * FR_RENDERER_FIELD_BYTES};
  VkPhysicalDeviceSubgroupProperties subgroup = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
  VkPhysicalDeviceProperties2 properties = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &subgroup};
  vkGetPhysicalDeviceProperties2(g->physical, &properties);
  VkSubgroupFeatureFlags required = VK_SUBGROUP_FEATURE_BASIC_BIT |
                                    VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT;
  bool fast_scan = (subgroup.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) &&
                   (subgroup.supportedOperations & required) == required &&
                   subgroup.subgroupSize >= 16u && subgroup.subgroupSize <= 128u &&
                   !getenv("DC_LIGHTING_SHARED_SCAN");
  const char *shader = fast_scan ? "build/shaders/lighting_subgroup.comp.spv"
                                 : "build/shaders/lighting.comp.spv";
  if (!fr_renderer_compute_init(g, &l->compute, buffers, 9, 16,
                                shader, err, cap))
    goto fail;
  VkShaderModule module;
  if (!dc_gpu_load_shader_module(g, shader, &module, err, cap))
    goto fail;
  for (unsigned compact = 0; compact <= (unsigned)l->compact_available; compact++)
  for (unsigned i = 0; i < l->pass_count; i++) {
    light_pass_t pass = l->passes[i];
    if (l->specialized[compact][pass.iteration][pass.phase])
      continue;
    uint32_t values[3] = {pass.phase, pass.iteration, compact};
    VkSpecializationMapEntry entries[3] = {{0, 0, 4}, {1, 4, 4}, {2, 8, 4}};
    VkSpecializationInfo specialization = {3, entries, sizeof(values), values};
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module,
                  .pName = "main", .pSpecializationInfo = &specialization},
        .layout = l->compute.pipeline_layout};
    if (vkCreateComputePipelines(g->device, VK_NULL_HANDLE, 1, &info, NULL,
        &l->specialized[compact][pass.iteration][pass.phase]) != VK_SUCCESS) {
      vkDestroyShaderModule(g->device, module, NULL);
      goto fail;
    }
  }
  vkDestroyShaderModule(g->device, module, NULL);
  *out = l;
  return true;
fail:
  fr_lighting_gpu_destroy(l);
  return false;
}
bool fr_lighting_gpu_set_sources(fr_lighting_gpu_t *l,
                                 const fr_renderer_point_light_t *points,
                                 uint32_t count, fr_renderer_sun_t sun,
                                 uint32_t budget) {
  if (!l || count > FR_RENDERER_POINT_LIGHT_CAPACITY || (count && !points) ||
      !budget || budget > 65536 || sun.known_sky > 15)
    return false;
  for (uint32_t i = 0; i < count; i++) {
    const float values[8] = {points[i].x,        points[i].y,
                             points[i].height,   points[i].radius,
                             points[i].color[0], points[i].color[1],
                             points[i].color[2], points[i].intensity};
    for (uint32_t k = 0; k < 8; k++)
      if (!isfinite(values[k]))
        return false;
    if (points[i].radius <= 0 || points[i].height < 0 ||
        points[i].intensity < 0)
      return false;
    for (uint32_t k = 0; k < 3; k++)
      if (points[i].color[k] < 0)
        return false;
  }
  float length = 0;
  for (uint32_t k = 0; k < 3; k++) {
    if (!isfinite(sun.direction[k]) || !isfinite(sun.color[k]) ||
        sun.color[k] < 0)
      return false;
    length += sun.direction[k] * sun.direction[k];
  }
  if (!isfinite(sun.intensity) || sun.intensity < 0 ||
      (sun.intensity > 0 && length <= 0) || !isfinite(length))
    return false;
  float *sources = l->source_mapped;
  for (uint32_t i = 0; i < count; i++)
    memcpy(sources + 8 * i, &points[i], 32);
  float inv = length > 0 ? 1 / sqrtf(length) : 0;
  for (uint32_t k = 0; k < 3; k++) {
    sources[8 * count + k] = sun.direction[k] * inv;
    sources[8 * count + 4 + k] = sun.color[k];
  }
  float common_z = count ? points[0].height / hypotf(1.f, points[0].height) :
                          sources[8 * count + 2];
  l->common_direction = l->compact_available;
  if (count && sun.intensity > 0 && common_z != sources[8 * count + 2])
    l->common_direction = false;
  for (uint32_t i = 1; i < count; i++)
    if (points[i].height != points[0].height) l->common_direction = false;
  sources[8 * count + 3] = common_z;
  sources[8 * count + 7] = sun.intensity;
  uint32_t *p = l->parameter_mapped;
  p[3] = count;
  p[4] = budget;
  p[5] = sun.known_sky;
  p[FR_TRANSPORT_SOURCE_REVISION]++;
  float planar = sqrtf(fmaxf(0.f, 1.f - common_z * common_z));
  memcpy(p + 8, &planar, sizeof(planar));
  /* Source edits aggregate only the small configuration list. Scene pixels
     are cleared and injected in parallel on the GPU, never walked here. */
  memset(sources+FR_TRANSPORT_AGGREGATE_BASE*8u,0,FR_RENDERER_POINT_LIGHT_CAPACITY*32u);
  uint32_t largest=0;
  for (uint32_t i=0;i<count;i++) {
    float radius=fminf((float)budget,points[i].radius);
    uint32_t x0=(uint32_t)fminf((float)l->width,fmaxf(0,floorf(points[i].x-radius)));
    uint32_t y0=(uint32_t)fminf((float)l->height,fmaxf(0,floorf(points[i].y-radius)));
    uint32_t x1=(uint32_t)fminf((float)l->width,fmaxf(0,floorf(points[i].x+radius)+1.f));
    uint32_t y1=(uint32_t)fminf((float)l->height,fmaxf(0,floorf(points[i].y+radius)+1.f));
    uint32_t at=FR_TRANSPORT_SOURCE_RECTS+4u*i;
    p[at]=x0;p[at+1u]=y0;
    p[at+2u]=x1-x0;p[at+3u]=y1-y0;
    uint32_t groups=tiles((x1-x0)*(y1-y0));
    if (groups>largest) largest=groups;
    uint32_t owner=i;
    for(uint32_t j=0;j<i;j++)
      if(floorf(points[j].x)==floorf(points[i].x)&&floorf(points[j].y)==floorf(points[i].y)) {owner=j;break;}
    float height=points[i].height;
    float scale=points[i].intensity*.25f/(1.f+height*height);
    float vertical=height/hypotf(1.f,height);
    for(uint32_t c=0;c<3;c++) {
      float energy=points[i].color[c]*scale;
      sources[(FR_TRANSPORT_AGGREGATE_BASE+owner)*8u+c]+=energy;
      sources[(FR_TRANSPORT_AGGREGATE_BASE+owner)*8u+4u+c]+=energy*vertical;
    }
  }
  l->passes[1].groups=largest;
  l->passes[1].directions=count;
  uint32_t *commands=l->work_mapped;
  uint32_t x=smaller(largest,65535u);
  commands[8]=commands[11]=x;
  commands[9]=(largest+65534u)/65535u;
  commands[10]=count;
  return true;
}
bool fr_lighting_gpu_generate(fr_lighting_gpu_t *l, char *err, uint32_t cap) {
  (void)err;
  (void)cap;
  if (!l || !fr_renderer_begin(l->gpu))
    return false;
  dc_gpu_t *g = l->gpu;
  fr_renderer_barrier(
      g->command,
      VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  fr_lighting_record(l, VK_NULL_HANDLE, 0);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
  return fr_renderer_submit(g);
}
void fr_lighting_record(fr_lighting_gpu_t *l, VkBuffer indirect,
                        VkDeviceSize offset) {
  dc_gpu_t *g = l->gpu;
  fr_renderer_barrier(
      g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
  if (indirect) {
    VkBufferCopy copy = {offset, 0, sizeof(uint32_t)};
    vkCmdCopyBuffer(g->command, indirect, l->transport[3], 1, &copy);
  } else
    vkCmdFillBuffer(g->command, l->transport[3], 0, sizeof(uint32_t), 1);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    l->compute.pipeline);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          l->compute.pipeline_layout, 0, 1,
                          &l->compute.descriptor, 0, NULL);
  uint32_t push[4] = {4, 0, 0, l->pass_count};
  vkCmdPushConstants(g->command, l->compute.pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
  vkCmdDispatch(g->command, tiles(l->pass_count), 1, 1);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                          VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  for (uint32_t i = 0; i < l->pass_count; i++) {
    light_pass_t pass = l->passes[i];
    vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      l->specialized[l->common_direction][pass.iteration][pass.phase]);
    push[0] = pass.phase;
    push[1] = pass.level;
    push[2] = pass.iteration;
    vkCmdPushConstants(g->command, l->compute.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatchIndirect(g->command, l->transport[3],
                          (VkDeviceSize)(4u + i * 4u) * sizeof(uint32_t));
    fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  }
}
bool fr_lighting_gpu_read(fr_lighting_gpu_t *l, uint32_t x, uint32_t y,
                          fr_renderer_light_field_t *field) {
  if (!l || !field || x >= l->width || y >= l->height)
    return false;
  const uint32_t *packed = (const uint32_t *)l->field_mapped +
                           (y * l->width + x) * 6u;
  memset(field, 0, sizeof(*field));
  for (uint32_t color = 0; color < 3u; color++) {
    memcpy(&field->l0[color], &packed[color], sizeof(float));
    float *moments[3] = {field->lx, field->ly, field->lz};
    for (uint32_t axis = 0; axis < 3u; axis++) {
      int32_t component = (int32_t)((packed[3u + color] >> (axis * 10u)) & 1023u);
      if (component >= 512)
        component -= 1024;
      moments[axis][color] = (float)component / 511.f * field->l0[color];
    }
  }
  return true;
}
bool fr_lighting_field_binding(fr_lighting_gpu_t *l,
                               VkDescriptorBufferInfo *buffer) {
  if (!l || !buffer)
    return false;
  *buffer = (VkDescriptorBufferInfo){l->field, 0,
                                     (VkDeviceSize)l->width * l->height * FR_RENDERER_FIELD_BYTES};
  return true;
}
