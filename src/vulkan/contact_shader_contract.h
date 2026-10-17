#ifndef FERRUM_VULKAN_CONTACT_SHADER_CONTRACT_H
#define FERRUM_VULKAN_CONTACT_SHADER_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/** Check typed-body specialization and transient dispatch storage agreement. */
static bool dc_gpu_contact_shader_compatible(const char *path,uint32_t word,char *err,uint32_t cap) {
    (void)path;(void)word;
    if(err&&cap) snprintf(err,cap,"Contact shader contract check is unimplemented");
    return false;
}

#endif
