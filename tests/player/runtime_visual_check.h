#ifndef PLAYER_RUNTIME_VISUAL_CHECK_H
#define PLAYER_RUNTIME_VISUAL_CHECK_H

#include <math.h>

#include "ferrum/player/beaver.h"
#include "../../src/app/view_config.h"

static uint32_t visual_channel(float linear) {
    float srgb=linear<=.0031308f?12.92f*linear:1.055f*powf(linear,1.f/2.4f)-.055f;
    return (uint32_t)lroundf(fminf(1,fmaxf(0,srgb))*255.f);
}
/** Compare the actual rendered view with the authoritative current pose. Check
 * surrounding air as well: old/debug body footprints cannot pass as artwork. */
static bool runtime_visual_check(dc_level_view_t *v,char *err,uint32_t cap) {
    fr_player_t p;fr_beaver_body_t binding;dc_gpu_world_body_t body;dc_gpu_body_motion_t motion;
    dc_level_view_status_t status;uint32_t frame;
    if(!dc_players_status(v,0,&p,&binding)||!fr_player_frame(&p,&frame)||
       !dc_level_view_status(v,&status)||!dc_level_view_body_state(v,binding.binding.body_id,&body,&motion,err,cap)) return false;
    fr_player_action_t display=p.action==FR_PLAYER_WALK?FR_PLAYER_IDLE:p.action;
    if(display==FR_PLAYER_IDLE) frame=0;
    size_t bytes;uint32_t entries;fr_beaver_asset_size(display,&bytes,&entries);
    uint8_t *storage=malloc(bytes);uint32_t *mapping=malloc((size_t)entries*4);
    uint32_t *pixels=malloc((size_t)VIEW_WIDTH*VIEW_HEIGHT*4);fr_beaver_asset_t asset;
    bool okay=storage&&mapping&&pixels&&fr_beaver_asset_build(display,storage,bytes,mapping,entries,&asset,err,cap)&&
        dc_level_view_pixels(v,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,cap);
    float bx=(float)((body.chunk.x-status.origin.x)*64)-(float)status.offset_x+body.body.x_fp/65536.f;
    float by=(float)((body.chunk.y-status.origin.y)*64)-(float)status.offset_y+body.body.y_fp/65536.f;
    uint32_t checked=0,mismatch=0;
    if(okay) for(int y=(int)floorf(by)-4;y<(int)ceilf(by)+44;y++) for(int x=(int)floorf(bx)-44;x<(int)ceilf(bx)+84;x++) {
        if(x<0||y<0||x>=VIEW_WIDTH||y>=VIEW_HEIGHT) continue;
        int64_t world_y=(status.origin.y*64)+(int64_t)status.offset_y+y;
        if(world_y>=96) continue;
        int sx=(int)floorf(x+.5f-bx)+(int)binding.rest_origin_x+(int)(asset.prop.width-40)/2;
        int sy=(int)floorf(y+.5f-by)+(int)binding.rest_origin_y;
        fr_prop_appearance_t a={0};
        if(sx>=0&&sy>=0&&sx<(int)asset.prop.width&&sy<40) {
            if(p.facing<0) sx=(int)asset.prop.width-1-sx;
            fr_prop_appearance(&asset.prop,frame,(uint32_t)sy*asset.prop.width+(uint32_t)sx,&a);
        }
        for(unsigned c=0;c<3;c++) {
            float color=roundf(a.color[c]*255.f)/255.f,alpha=roundf(a.alpha*255.f)/255.f;
            uint32_t expected=visual_channel(alpha*color+(1-alpha)*.00886f);
            uint32_t actual=(pixels[y*VIEW_WIDTH+x]>>(c*8))&255u;
            int delta=(int)actual-(int)expected;
            if(delta>2||delta< -2) mismatch++;
        }
        checked++;
    }
    if(okay&&mismatch) {snprintf(err,cap,"Unexpected rendered colors/footprints: %u channel mismatches over %u cells",mismatch,checked);okay=false;}
    free(storage);free(mapping);free(pixels);return okay&&checked>0;
}
#endif
