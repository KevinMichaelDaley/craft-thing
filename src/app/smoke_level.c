#include <stdio.h>
#include <stdlib.h>
#include "ferrum/fire/gas_gpu.h"
#include "level_internal.h"
#include "smoke_box.h"
#include "view_config.h"
static bool error(char *err,uint32_t cap,const char *message) {if(err&&cap) snprintf(err,cap,"%s",message);return false;}
dc_level_view_t *dc_level_view_create_smoke_box(const char *directory,uint64_t seed,char *err,uint32_t cap) {
    (void)directory;(void)seed;error(err,cap,"Cold smoke fixture factory not implemented");return NULL;
}
static bool valve(dc_level_view_t *view,uint32_t index,bool closed,char *err,uint32_t cap) {
    static const int32_t rectangle[3][5]={{124,47,8,1,1},{324,47,8,1,1},{399,-48,1,16,0}};
    if(!view||index>=3) return error(err,cap,"Invalid smoke fixture valve");
    int64_t x=(int64_t)rectangle[index][0]-view->mapped_origin.x*64+DC_CHUNK_SIDE;
    int64_t y=(int64_t)rectangle[index][1]-view->mapped_origin.y*64+DC_CHUNK_SIDE;
    if(x<0||y<0||x+rectangle[index][2]>SIM_WIDTH||y+rectangle[index][3]>SIM_HEIGHT)
        return error(err,cap,"Smoke fixture valve is outside the resident window");
    if(!fr_gas_gpu_gate(view->gpu,(uint32_t)x,(uint32_t)y,rectangle[index][2],rectangle[index][3],rectangle[index][4],closed,err,cap)) return false;
    if(closed) view->smoke_closed|=1u<<index;else view->smoke_closed&=~(1u<<index);return true;
}
bool dc_level_view_setup_smoke_box(dc_level_view_t *view,char *err,uint32_t cap) {
    if(!view||VIEW_WIDTH!=480||VIEW_HEIGHT!=270) return error(err,cap,"Smoke box requires quarter-native placement resolution");
    if(view->smoke_box) return true;
    if(view->restored) return error(err,cap,"Smoke fixture checkpoint admission is not implemented yet");
    if(view->origin.x!=0||view->origin.y!=INITIAL_CHUNK_Y) return error(err,cap,"Fresh smoke box requires the initial world origin");
    if(!dc_level_view_wait_visible(view,120000,err,cap)) return false;
    uint32_t count=view->table.capacity;dc_chunk_coord_t *coords=calloc(count,sizeof(*coords));
    dc_smoke_box_chunk_t *chunks=calloc(count,sizeof(*chunks));
    if(!coords||!chunks) {free(coords);free(chunks);return error(err,cap,"Cannot preflight smoke box");}
    for(uint32_t i=0;i<count;i++) coords[i]=view->table.slots[i].coord;
    bool okay=dc_smoke_box_build(coords,count,chunks,err,cap);free(coords);
    for(uint32_t i=0;okay&&i<count;i++) okay=dc_gpu_upload_chunk(view->gpu,i,chunks[i].chunk,err,cap)&&
        fr_renderer_upload_chunk_colors(view->gpu,i,chunks[i].colors,err,cap)&&dc_chunk_table_mark_dirty(&view->table,i);
    fr_gas_model_t model=dc_smoke_box_model();
    if(okay) okay=dc_level_view_set_spring_enabled(view,false)&&dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO)&&
        fr_gas_gpu_enable(view->gpu,&model,.0012f,293.15f,err,cap)&&
        fr_gas_gpu_set_pressure_policy(view->gpu,FR_GAS_PRESSURE_IMPLICIT,err,cap)&&fr_gas_gpu_set_gravity(view->gpu,900,err,cap);
    for(uint32_t i=0;okay&&i<count;i++) okay=fr_gas_gpu_seed_chunk(view->gpu,i,chunks[i].gas,chunks[i].void_fractions,4096,err,cap);
    dc_smoke_box_release(chunks,count);free(chunks);
    for(uint32_t i=0;okay&&i<3;i++) okay=valve(view,i,true,err,cap);
    if(okay) view->smoke_box=true;
    return okay;
}
bool dc_level_view_smoke_valve(dc_level_view_t *view,uint32_t index,char *err,uint32_t cap) {
    if(!view||!view->smoke_box||index>=3) return error(err,cap,"Smoke box is not active");
    return valve(view,index,(view->smoke_closed&(1u<<index))==0u,err,cap);
}
bool dc_level_view_smoke_overlay(dc_level_view_t *view,uint32_t mode) {
    return view&&view->smoke_box&&fr_gas_gpu_overlay(view->gpu,mode);
}
