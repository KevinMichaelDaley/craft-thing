#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/sprite_layout.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/renderer/sprite_gpu.h"
#include "body_address_gpu.h"
#include "renderer_internal.h"

struct fr_sprite_gpu {dc_gpu_address_buffer_t channel;uint32_t pixels;};
_Static_assert(sizeof(fr_renderer_sprite_t)==10u*sizeof(uint32_t),"Sprite pose ABI");
_Static_assert(sizeof(fr_renderer_sprite_pixel_t)==2u*sizeof(uint32_t),"Sprite texel ABI");
static bool sprite_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}
void fr_renderer_sprites_destroy(dc_gpu_t *g) {
    if(!g||!g->sprites) return;
    dc_gpu_address_buffer_destroy(g,&g->sprites->channel);free(g->sprites);g->sprites=NULL;
}

VkDeviceAddress fr_renderer_sprites_address(const dc_gpu_t *g) {
    return g->sprites ? g->sprites->channel.address : 0;
}

static void sprite_build_contact_map(uint32_t *map, const uint32_t *frame, uint32_t texels) {
    memset(map, 0, DC_SPRITE_SOURCE_COUNT * sizeof(*map));
    for (uint32_t i = 0; i < texels; ++i) {
        uint32_t source = frame[2u * i + 1u];
        if (source && source <= DC_SPRITE_SOURCE_COUNT && (frame[2u * i] >> 24u) && !map[source - 1u])
            map[source - 1u] = i + 1u;
    }
}

bool fr_renderer_sprite_atlas(dc_gpu_t *g,const fr_renderer_sprite_pixel_t *p,
    uint32_t n,char *err,uint32_t cap) {
    if(!g||!p||!n||n>(UINT32_MAX-DC_SPRITE_ATLAS_WORD-DC_SPRITE_CONTACT_WORDS)/2u)
        return sprite_error(err,cap,"Invalid sprite atlas");
    uint32_t contact=DC_SPRITE_ATLAS_WORD+2u*n;
    VkDeviceSize bytes=((VkDeviceSize)contact+DC_SPRITE_CONTACT_WORDS)*4u;
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    if(bytes>properties.limits.maxStorageBufferRange)
        return sprite_error(err,cap,"Sprite atlas exceeds device buffer range");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_enable(g,err,cap)) return false;
    struct fr_sprite_gpu *next=calloc(1,sizeof(*next));
    if(!next) return sprite_error(err,cap,"Cannot allocate sprite atlas owner");
    if(!dc_gpu_address_buffer_create(g,bytes,true,&next->channel,err,cap)) {free(next);return false;}
    next->pixels=n;uint32_t *w=next->channel.mapped;
    memset(w,0,(size_t)bytes);w[0]=DC_SPRITE_MAGIC;
    memcpy(w+DC_SPRITE_ATLAS_WORD,p,(size_t)n*sizeof(*p));
    VkDescriptorBufferInfo binding={next->channel.buffer,0,bytes};
    fr_renderer_bind_sprites(g,&binding);
    fr_renderer_sprites_destroy(g);g->sprites=next;
    return true;
}
bool fr_renderer_sprite_prepare_poses(dc_gpu_t *g,const fr_renderer_sprite_t *p,
    uint32_t n,char *err,uint32_t cap) {
    if(!g||!g->sprites||n>FR_RENDERER_SPRITES||(n&&!p))
        return sprite_error(err,cap,"Invalid sprite pose batch");
    for(uint32_t i=0;i<n;i++) {
        uint64_t end=(uint64_t)p[i].offset+(uint64_t)p[i].width*p[i].height;
        if(!p[i].body_id||!p[i].generation||!p[i].width||!p[i].height||p[i].flip>1||
            end>g->sprites->pixels||p[i].width>4096||p[i].height>4096)
            return sprite_error(err,cap,"Invalid sprite body, generation or atlas extent");
        for(uint32_t j=0;j<i;j++) if(p[i].body_id==p[j].body_id)
            return sprite_error(err,cap,"Duplicate sprite body");
    }
    return vkQueueWaitIdle(g->queue)==VK_SUCCESS||sprite_error(err,cap,"Cannot reach sprite publication boundary");
}

void fr_renderer_sprite_commit_poses(dc_gpu_t *g,const fr_renderer_sprite_t *p,uint32_t n) {
    uint32_t *w=g->sprites->channel.mapped;
    const uint32_t contact=DC_SPRITE_ATLAS_WORD+2u*g->sprites->pixels;
    for(uint32_t i=0;i<n;i++) {
        uint32_t *pose=w+DC_SPRITE_HEADER_WORDS+i*DC_SPRITE_POSE_WORDS;
        uint32_t reverse_at=contact+i*DC_SPRITE_SOURCE_COUNT;
        /* Mirroring and origin offsets are applied in the shader. */
        if(pose[2]!=p[i].offset||pose[3]!=p[i].width||pose[4]!=p[i].height) {
            const uint32_t *frame=w+DC_SPRITE_ATLAS_WORD+2u*p[i].offset;
            sprite_build_contact_map(w+reverse_at,frame,p[i].width*p[i].height);
        }
        memcpy(pose,&p[i],sizeof(*p));
        pose[DC_SPRITE_CONTACT_WORD]=reverse_at;
    }
    w[1]=n;
}

bool fr_renderer_sprite_poses(dc_gpu_t *g,const fr_renderer_sprite_t *p,
    uint32_t n,char *err,uint32_t cap) {
    if(!fr_renderer_sprite_prepare_poses(g,p,n,err,cap)) return false;
    fr_renderer_sprite_commit_poses(g,p,n);
    return true;
}
