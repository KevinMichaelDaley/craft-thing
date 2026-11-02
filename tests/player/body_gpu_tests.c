#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/player/beaver.h"
#include "ferrum/player/body_gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_canonical_body_admits_hidden_owners_once(void) {
    dc_gpu_t *g=NULL;char err[256]={0};
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));free(chunk);
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_beaver_body_t body;
    ASSERT_TRUE(fr_beaver_body_admit(g,41,71,0,(dc_chunk_coord_t){0,0},8*65536,9*65536,&body,err,sizeof(err)));
    ASSERT_EQ(body.binding.identity,41);ASSERT_TRUE(body.binding.generation);
    uint32_t expected=0,source=0;
    for(uint32_t s=1;s<=FR_BEAVER_PIXELS;s++) {fr_beaver_source_t meta;if(fr_beaver_source(s,&meta)) {expected++;source=s;}}
    uint32_t count=0;ASSERT_TRUE(dc_gpu_read_pixel_shape(g,body.binding.body_id,NULL,0,&count,err,sizeof(err)));ASSERT_EQ(count,expected);
    fr_gpu_reactive_stats_t stats;ASSERT_TRUE(fr_gpu_reactive_read_stats(g,&stats,err,sizeof(err)));ASSERT_EQ(stats.live,expected);
    fr_reactive_record_t parcel;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,body.binding.body_id,(source-1)%40-body.rest_origin_x,
        (source-1)/40-body.rest_origin_y,&parcel,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_EQ(parcel.owner.key[0],body.binding.body_id);ASSERT_EQ(parcel.owner.key[1],body.binding.generation);
    dc_gpu_world_body_t pose;ASSERT_TRUE(dc_gpu_read_world_body(g,body.binding.body_id,&pose,err,sizeof(err)));
    ASSERT_EQ(pose.body.x_fp,8*65536);ASSERT_EQ(pose.body.y_fp,9*65536);
    dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_canonical_body_admits_hidden_owners_once);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
