#include <stdio.h>
#include <string.h>
#include "gpu_internal.h"

static int64_t cross(int64_t x,int64_t y,int64_t px,int64_t py) { return x*py-y*px; }
static bool separated(dc_gpu_body_shape_t a,dc_gpu_body_shape_t b) {
    for(uint32_t i=0;i<a.count;++i) {
        uint32_t j=(i+1)%a.count;
        int64_t dx=(int64_t)a.vertices[j].x_fp-a.vertices[i].x_fp;
        int64_t dy=(int64_t)a.vertices[j].y_fp-a.vertices[i].y_fp;
        int64_t amin=INT64_MAX,amax=INT64_MIN,bmin=INT64_MAX,bmax=INT64_MIN;
        for(uint32_t k=0;k<a.count;++k) {
            int64_t v=cross(dx,dy,a.vertices[k].x_fp,a.vertices[k].y_fp);
            if(v<amin) amin=v;
            if(v>amax) amax=v;
        }
        for(uint32_t k=0;k<b.count;++k) {
            int64_t v=cross(dx,dy,b.vertices[k].x_fp,b.vertices[k].y_fp);
            if(v<bmin) bmin=v;
            if(v>bmax) bmax=v;
        }
        if(amax<=bmin || bmax<=amin) return true;
    }
    return false;
}
static bool shared_edge(dc_gpu_body_shape_t a,dc_gpu_body_shape_t b) {
    for(uint32_t i=0;i<a.count;++i) for(uint32_t k=0;k<b.count;++k) {
        uint32_t j=(i+1)%a.count,l=(k+1)%b.count;
        int64_t dx=(int64_t)a.vertices[j].x_fp-a.vertices[i].x_fp;
        int64_t dy=(int64_t)a.vertices[j].y_fp-a.vertices[i].y_fp;
        if(cross(dx,dy,(int64_t)b.vertices[k].x_fp-a.vertices[i].x_fp,
            (int64_t)b.vertices[k].y_fp-a.vertices[i].y_fp)!=0 ||
            cross(dx,dy,(int64_t)b.vertices[l].x_fp-a.vertices[i].x_fp,
            (int64_t)b.vertices[l].y_fp-a.vertices[i].y_fp)!=0) continue;
        int32_t av=dx ? a.vertices[i].x_fp : a.vertices[i].y_fp;
        int32_t aw=dx ? a.vertices[j].x_fp : a.vertices[j].y_fp;
        int32_t bv=dx ? b.vertices[k].x_fp : b.vertices[k].y_fp;
        int32_t bw=dx ? b.vertices[l].x_fp : b.vertices[l].y_fp;
        int32_t alo=av<aw ? av : aw,ahi=av>aw ? av : aw;
        int32_t blo=bv<bw ? bv : bw,bhi=bv>bw ? bv : bw;
        if((alo>blo ? alo : blo)<(ahi<bhi ? ahi : bhi)) return true;
    }
    return false;
}
bool dc_gpu_valid_compound_shape(const dc_gpu_body_t *body,const dc_gpu_compound_shape_t *shape) {
    if(!shape || shape->count<2 || shape->count>DC_GPU_BODY_PIECES) return false;
    bool linked[DC_GPU_BODY_PIECES][DC_GPU_BODY_PIECES]={{false}};
    for(uint32_t i=0;i<shape->count;++i) {
        if(!dc_gpu_valid_body_shape(body,&shape->pieces[i]) ||
            shape->pieces[i].material!=shape->pieces[0].material) return false;
        for(uint32_t j=0;j<i;++j) {
            if(!separated(shape->pieces[i],shape->pieces[j]) &&
                !separated(shape->pieces[j],shape->pieces[i])) return false;
            linked[i][j]=linked[j][i]=shared_edge(shape->pieces[i],shape->pieces[j]);
        }
    }
    uint32_t reached=1;
    for(uint32_t pass=0;pass<shape->count;++pass)
        for(uint32_t i=0;i<shape->count;++i) if(reached&(1u<<i))
            for(uint32_t j=0;j<shape->count;++j) if(linked[i][j]) reached|=1u<<j;
    return reached==(1u<<shape->count)-1u;
}
bool dc_gpu_spawn_compound_body(dc_gpu_t *gpu,dc_gpu_world_body_t body,
    const dc_gpu_compound_shape_t *shape,char *err,uint32_t cap) {
    if(!dc_gpu_valid_compound_shape(&body.body,shape)) {
        if(err && cap) snprintf(err,cap,"Invalid compound body pieces");
        return false;
    }
    dc_gpu_compound_shape_t copy={.count=shape->count};
    for(uint32_t i=0;i<copy.count;++i) {
        copy.pieces[i].count=shape->pieces[i].count;
        copy.pieces[i].material=shape->pieces[i].material;
        memcpy(copy.pieces[i].vertices,shape->pieces[i].vertices,
            shape->pieces[i].count*sizeof(shape->pieces[i].vertices[0]));
    }
    if(!dc_gpu_spawn_convex_body(gpu,body,&copy.pieces[0],err,cap)) return false;
    for(uint32_t i=0;i<gpu->body_count;++i) if(gpu->body_ids[i]==body.body.id) {
        dc_gpu_body_record_t *record=(dc_gpu_body_record_t *)gpu->body_mapped+i;
        record->piece_count=copy.count;
        memcpy(record->pieces,copy.pieces,sizeof(copy.pieces));
        return true;
    }
    return false;
}
bool dc_gpu_read_compound_shape(dc_gpu_t *gpu,uint32_t id,dc_gpu_compound_shape_t *shape,
    char *err,uint32_t cap) {
    if(gpu && shape && id) for(uint32_t i=0;i<gpu->body_count;++i) if(gpu->body_ids[i]==id) {
        const dc_gpu_body_record_t *record=(dc_gpu_body_record_t *)gpu->body_mapped+i;
        *shape=(dc_gpu_compound_shape_t){.count=record->piece_count};
        memcpy(shape->pieces,record->pieces,sizeof(shape->pieces));
        return true;
    }
    if(err && cap) snprintf(err,cap,"Invalid compound shape readback");
    return false;
}
