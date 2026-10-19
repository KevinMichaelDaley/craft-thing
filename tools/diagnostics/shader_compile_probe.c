#define _POSIX_C_SOURCE 200809L
#define main bounded_probe_main
#include "vulkan_probe.c"
#undef main
#include <time.h>

VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueSubmit(VkQueue queue,
                                                    uint32_t count,
                                                    const VkSubmitInfo *info,
                                                    VkFence fence) {
    (void)queue;
    (void)count;
    (void)info;
    (void)fence;
    fputs("ERROR: compiler probe forbids queue submissions\n", stderr);
    exit(2);
}
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueSubmit2(VkQueue queue,
                                                     uint32_t count,
                                                     const VkSubmitInfo2 *info,
                                                     VkFence fence) {
    (void)queue;
    (void)count;
    (void)info;
    (void)fence;
    fputs("ERROR: compiler probe forbids queue submissions\n", stderr);
    exit(2);
}

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: %s shader.spv generic|phase [constant1]\n", argv[0]);
        return 2;
    }
    probe_t p = {0};
    if (!initialize_device(&p))
        return 2;
    VkPhysicalDeviceSubgroupProperties subgroup = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceProperties2 properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &subgroup};
    vkGetPhysicalDeviceProperties2(p.physical, &properties);
    printf("subgroup size%u operations0x%x compute%d\n", subgroup.subgroupSize,
           subgroup.supportedOperations, (subgroup.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0);
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    uint32_t code[65536];
    size_t bytes = fread(code, 1, sizeof(code), file);
    int extra = fgetc(file);
    fclose(file);
    if (bytes == 0 || bytes % 4 || extra != EOF)
        return 2;
    VkShaderModuleCreateInfo module = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = bytes,
        .pCode = code};
    if (!check(vkCreateShaderModule(p.device, &module, NULL, &p.shader),
               "shader module"))
        return 2;
    VkDescriptorSetLayoutBinding bindings[36] = {0};
    for (unsigned i = 0; i < 36; i++)
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = i,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo descriptor = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 36,
        .pBindings = bindings};
    if (!check(vkCreateDescriptorSetLayout(p.device, &descriptor, NULL,
                                           &p.descriptor_layout),
               "descriptor layout"))
        return 2;
    VkPushConstantRange push = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                .size = 32};
    VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &p.descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push};
    if (!check(vkCreatePipelineLayout(p.device, &layout, NULL, &p.layout),
               "pipeline layout"))
        return 2;
    uint32_t phase = UINT32_MAX;
    if (strcmp(argv[2], "generic") != 0) {
        char *end;
        unsigned long number = strtoul(argv[2], &end, 10);
        if (*end || number > 12)
            return 2;
        phase = (uint32_t)number;
    }
    uint32_t values[2] = {phase, 0};
    if (argc == 4) {
        char *end;
        unsigned long value = strtoul(argv[3], &end, 10);
        if (*end || value > UINT32_MAX || phase == UINT32_MAX) return 2;
        values[1] = (uint32_t)value;
    }
    VkSpecializationMapEntry entries[2] = {{.constantID = 0, .size = sizeof(uint32_t)},
                                          {.constantID = 1, .offset = sizeof(uint32_t), .size = sizeof(uint32_t)}};
    VkSpecializationInfo specialization = {.mapEntryCount = argc == 4 ? 2u : 1u,
                                           .pMapEntries = entries,
                                           .dataSize = argc == 4 ? sizeof(values) : sizeof(phase),
                                           .pData = values};
    VkComputePipelineCreateInfo pipeline = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = p.shader,
                  .pName = "main",
                  .pSpecializationInfo =
                      phase == UINT32_MAX ? NULL : &specialization},
        .layout = p.layout};
    double start = seconds();
    VkResult result = vkCreateComputePipelines(p.device, VK_NULL_HANDLE, 1,
                                               &pipeline, NULL, &p.pipeline);
    printf("CPU compile phase%u %.3fms status%d; zero queue submissions\n",
           phase, (seconds() - start) * 1000., result);
    if (p.pipeline)
        vkDestroyPipeline(p.device, p.pipeline, NULL);
    vkDestroyPipelineLayout(p.device, p.layout, NULL);
    vkDestroyDescriptorSetLayout(p.device, p.descriptor_layout, NULL);
    vkDestroyShaderModule(p.device, p.shader, NULL);
    vkDestroyDevice(p.device, NULL);
    vkDestroyInstance(p.instance, NULL);
    return result == VK_SUCCESS ? 0 : 1;
}
