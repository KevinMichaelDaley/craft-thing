#include <float.h>
#include <math.h>
#include <stdio.h>

#include "gas_gpu_state.h"

_Static_assert(FR_GAS_AUDIT_SPECIES_WORDS == FR_GAS_SPECIES, "Gas audit species layout");

static bool gas_audit_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool gas_audit_buffer(dc_gpu_t *g, size_t groups, char *err, uint32_t cap) {
    struct fr_gas_gpu *gas = g->gas;
    if (gas->audit_summary.buffer) return true;

    VkDeviceSize bytes = (VkDeviceSize)groups * FR_GAS_AUDIT_PARTIAL_WORDS * sizeof(float);
    if (!dc_gpu_address_buffer_create(g, bytes, true, &gas->audit_summary, err, cap)) return false;
    VkDescriptorBufferInfo buffer = {gas->audit_summary.buffer, 0, bytes};
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = gas->audit.descriptor,
        .dstBinding = 6u,
        .descriptorCount = 1u,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer,
    };
    vkUpdateDescriptorSets(g->device, 1u, &write, 0u, NULL);
    return true;
}

bool fr_gas_gpu_audit(dc_gpu_t *g, fr_gas_gpu_audit_t *out, char *err, uint32_t cap) {
    if (!g || !g->gas || !out) return gas_audit_error(err, cap, "Invalid cold gas audit");
    struct fr_gas_gpu *gas = g->gas;
    size_t groups = (size_t)gas->groups_x * gas->groups_y;
    if (vkQueueWaitIdle(g->queue) != VK_SUCCESS) {
        return gas_audit_error(err, cap, "Cannot wait for cold gas audit");
    }
    if (!gas_audit_buffer(g, groups, err, cap) || !fr_gas_run_audit(g, err, cap)) return false;

    double totals[FR_GAS_AUDIT_WORDS] = {0};
    totals[FR_GAS_AUDIT_MIN_TEMPERATURE] = FLT_MAX;
    const float *partial = gas->audit_summary.mapped;
    for (size_t group = 0; group < groups; ++group, partial += FR_GAS_AUDIT_PARTIAL_WORDS) {
        for (uint32_t word = 0; word < FR_GAS_AUDIT_WORDS; ++word) {
            double value = partial[word];
            if (word < FR_GAS_AUDIT_SPECIES_WORDS) {
                totals[word] += value + partial[FR_GAS_AUDIT_WORDS + word];
            } else if (word == FR_GAS_AUDIT_MIN_TEMPERATURE) {
                totals[word] = fmin(totals[word], value);
            } else if (FR_GAS_AUDIT_IS_MAX(word)) {
                totals[word] = fmax(totals[word], value);
            } else {
                totals[word] += value;
            }
        }
    }
    for (uint32_t word = 0; word < FR_GAS_AUDIT_WORDS; ++word) out->metrics[word] = (float)totals[word];
    for (uint32_t species = 0; species < FR_GAS_SPECIES; ++species) {
        out->species[species] = totals[FR_GAS_AUDIT_SPECIES + species];
    }
    return true;
}
