#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_cells_retain_sparse_state_and_brush_replacement_retires_it(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    c->coord=(dc_chunk_coord_t){-7,12};
    for(uint32_t i=0;i<512;i++) c->cells[i].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_record_t record;bool found=true;
    memset(&record,0xa5,sizeof(record));fr_reactive_record_t untouched=record;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,4,&record,&found,err,sizeof(err)));
    ASSERT_TRUE(!found&&memcmp(&record,&untouched,sizeof(record))==0);
    fr_reactive_state_t state;
    for(uint32_t i=0;i<512;i++) {
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,i%300,500+i,100+i,&state,err,sizeof(err)));
        state.ignition_threshold=.7f;state.ignition_integral=.3f;
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,i,&state,err,sizeof(err)));
    }
    for(uint32_t i=0;i<512;i+=17) {
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,i,&record,&found,err,sizeof(err)));
        ASSERT_TRUE(found);ASSERT_EQ(record.state.parcel_id_low,100+i);
        ASSERT_EQ(record.state.bound_water_fp,i%300);ASSERT_EQ(record.state.ignition_integral,.3f);
        fr_reactive_owner_t owner=fr_reactive_cell_owner(c->coord,i);
        ASSERT_TRUE(memcmp(&record.owner,&owner,sizeof(owner))==0);
    }
    /* Reusing an ID for another cell cannot duplicate mass or history. */
    ASSERT_TRUE(!fr_gpu_reactive_seed_cell(g,0,0,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,1,0,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,1,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,2,&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(dc_gpu_paint_material(g,1,0,0,DC_MATERIAL_WOOD,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,1,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    free(c);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_cells_retain_sparse_state_and_brush_replacement_retires_it);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
