#include <stdio.h>
#include <stdlib.h>

#include "gpu_internal.h"

bool dc_gpu_load_shader_module(dc_gpu_t *gpu, const char *path,
                               VkShaderModule *module, char *err, uint32_t cap) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (err && cap) snprintf(err, cap, "Cannot open SPIR-V shader: %s", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        if (err && cap) snprintf(err, cap, "Cannot size SPIR-V shader: %s", path);
        return false;
    }
    long length = ftell(file);
    if (length < 4 || (length & 3) || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        if (err && cap) snprintf(err, cap, "Invalid SPIR-V shader: %s", path);
        return false;
    }
    uint32_t *words = malloc((size_t)length);
    if (!words) {
        fclose(file);
        if (err && cap) snprintf(err, cap, "Out of memory reading shader: %s", path);
        return false;
    }
    bool read_ok = fread(words, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!read_ok) {
        free(words);
        if (err && cap) snprintf(err, cap, "Cannot read SPIR-V shader: %s", path);
        return false;
    }
    VkShaderModuleCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)length, .pCode = words };
    VkResult result = vkCreateShaderModule(gpu->device, &info, NULL, module);
    free(words);
    if (result != VK_SUCCESS) {
        if (err && cap) snprintf(err, cap, "Cannot create SPIR-V module: %s", path);
        return false;
    }
    return true;
}
