#ifndef FERRUM_VULKAN_CONTACT_SHADER_CONTRACT_H
#define FERRUM_VULKAN_CONTACT_SHADER_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dungeoncraft/contact_work_layout.h"

/** Check specialization defaults and transient dispatch storage agreement. */
static bool dc_gpu_contact_shader_interface(const char *path,uint32_t word,bool pixel_only,char *err,uint32_t cap) {
    FILE *file=fopen(path,"rb");
    const char *failure="Cannot read contact shader interface";
    bool binding=false,dynamic=false,slot=false,work=false;
    if(!file) goto fail;
    if(fseek(file,0,SEEK_END)!=0) goto fail;
    long bytes=ftell(file);
    if(bytes<20||bytes%4!=0||fseek(file,0,SEEK_SET)!=0) goto fail;
    uint32_t header[5];
    if(fread(header,sizeof(*header),5,file)!=5||header[0]!=0x07230203u) goto fail;
    unsigned long remaining=(unsigned long)bytes/4u-5u;
    uint32_t type_result=0,slot_result=0,work_result=0;
    while(remaining) {
        uint32_t first;
        if(fread(&first,sizeof(first),1,file)!=1) goto fail;
        uint32_t count=first>>16,opcode=first&65535u;
        if(count==0||count>remaining) goto fail;
        uint32_t target=0,decoration=0;
        for(uint32_t i=1;i<count;i++) {
            uint32_t value;
            if(fread(&value,sizeof(value),1,file)!=1) goto fail;
            if(opcode==71u) {
                if(i==1u) target=value;
                else if(i==2u) decoration=value;
                else if(i==3u) {
                    if(decoration==33u&&value==27u) binding=true;
                    if(decoration==1u&&value==1u) type_result=target;
                    if(decoration==1u&&value==31u) slot_result=target;
                    if(decoration==1u&&value==32u) work_result=target;
                }
            } else if(opcode==(pixel_only?48u:49u)&&count==3u&&i==2u&&type_result&&value==type_result) {
                dynamic=true;
            } else if(opcode==50u&&count==4u) {
                if(i==2u) target=value;
                if(i==3u&&slot_result&&target==slot_result&&value==word) slot=true;
                if(i==3u&&work_result&&target==work_result&&value==DC_CONTACT_WORK_ABI) work=true;
            }
        }
        remaining-=count;
    }
    fclose(file);file=NULL;
    if(binding&&dynamic&&slot&&work) return true;
    failure="Incompatible contact shader; rebuild it for the current dispatch interface";
fail:
    if(file) fclose(file);
    if(err&&cap) snprintf(err,cap,"%s",failure);
    return false;
}

/** Validate a generic module which can execute legacy and pixel contacts. */
static inline bool dc_gpu_contact_shader_compatible(const char *path,uint32_t word,char *err,uint32_t cap) {
    return dc_gpu_contact_shader_interface(path,word,false,err,cap);
}
/** Validate a module dedicated to four-corner pixel geometry. */
static inline bool dc_gpu_contact_shader_pixels(const char *path,uint32_t word,char *err,uint32_t cap) {
    return dc_gpu_contact_shader_interface(path,word,true,err,cap);
}

#endif
