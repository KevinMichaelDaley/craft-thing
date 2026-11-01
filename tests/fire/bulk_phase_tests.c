#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/bulk_phase.h"

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static uint64_t phase_stock(const fr_bulk_cell_t *a,const fr_bulk_cell_t *b,uint32_t phase) {
    return (uint64_t)a->volume_fp[phase]+a->retained_fp[phase]+
        b->volume_fp[phase]+b->retained_fp[phase];
}
static uint64_t occupancy(const fr_bulk_cell_t *cell) {
    uint64_t total=0;for(uint32_t i=0;i<FR_BULK_PHASES;i++) total+=cell->volume_fp[i];return total;
}
static void test_pair_admits_only_shared_capacity_and_conserves_each_phase(void) {
    fr_bulk_cell_t a={.volume_fp={0,30000,0},.capacity_fp=65536};
    fr_bulk_cell_t b={.volume_fp={50000,0,0},.capacity_fp=65536},x={0},y={0};uint32_t moved=0;
    uint64_t water=phase_stock(&a,&b,FR_BULK_WATER),oil=phase_stock(&a,&b,FR_BULK_OIL);
    ASSERT_TRUE(fr_bulk_phase_transfer(&a,&b,FR_BULK_OIL,30000,&x,&y,&moved));
    ASSERT_INT_EQ(moved,15536);ASSERT_INT_EQ(y.volume_fp[FR_BULK_WATER],50000);
    ASSERT_INT_EQ(y.volume_fp[FR_BULK_OIL],15536);ASSERT_INT_EQ(x.volume_fp[FR_BULK_OIL],14464);
    ASSERT_EQ(phase_stock(&x,&y,FR_BULK_WATER),water);ASSERT_EQ(phase_stock(&x,&y,FR_BULK_OIL),oil);
    ASSERT_TRUE(occupancy(&x)<=x.capacity_fp&&occupancy(&y)<=y.capacity_fp);PASS();
}
static void test_full_vessel_retains_slime_without_double_occupancy(void) {
    fr_bulk_cell_t a={.volume_fp={0,0,65536},.capacity_fp=65536};
    fr_bulk_cell_t b={.volume_fp={65536,0,0},.capacity_fp=65536},x={0},y={0};uint32_t moved=99;
    ASSERT_TRUE(fr_bulk_phase_transfer(&a,&b,FR_BULK_SLIME,65536,&x,&y,&moved));
    ASSERT_INT_EQ(moved,0);ASSERT_EQ(memcmp(&a,&x,sizeof(a)),0);ASSERT_EQ(memcmp(&b,&y,sizeof(b)),0);PASS();
}
static void test_retained_overflow_moves_to_a_free_neighbor_without_loss(void) {
    fr_bulk_cell_t a={.volume_fp={0,10000,0},.retained_fp={0,2000,0},.capacity_fp=10000};
    fr_bulk_cell_t b={.capacity_fp=65536},x={0},y={0};uint32_t moved=0;
    ASSERT_TRUE(fr_bulk_phase_transfer(&a,&b,FR_BULK_OIL,12000,&x,&y,&moved));
    ASSERT_INT_EQ(moved,12000);ASSERT_INT_EQ(y.volume_fp[FR_BULK_OIL],12000);
    ASSERT_INT_EQ(x.volume_fp[FR_BULK_OIL]+x.retained_fp[FR_BULK_OIL],0);
    ASSERT_EQ(phase_stock(&x,&y,FR_BULK_OIL),12000u);PASS();
}
static void test_invalid_phase_is_transactional(void) {
    fr_bulk_cell_t a={.volume_fp={100,0,0},.capacity_fp=65536};
    fr_bulk_cell_t b={.capacity_fp=65536},x={.capacity_fp=777},y={.capacity_fp=888};uint32_t moved=999;
    ASSERT_TRUE(!fr_bulk_phase_transfer(&a,&b,FR_BULK_PHASES,100,&x,&y,&moved));
    ASSERT_INT_EQ(x.capacity_fp,777);ASSERT_INT_EQ(y.capacity_fp,888);ASSERT_INT_EQ(moved,999);PASS();
}
static void test_profile_density_orders_water_oil_and_slime_without_float_flag(void) {
    const float density[FR_BULK_PHASES]={1.0f,1.1f,1.2f};uint32_t order[FR_BULK_PHASES]={99,99,99};
    ASSERT_TRUE(fr_bulk_phase_density_order(density,order));
    ASSERT_INT_EQ(order[0],FR_BULK_WATER);ASSERT_INT_EQ(order[1],FR_BULK_OIL);
    ASSERT_INT_EQ(order[2],FR_BULK_SLIME);PASS();
}
static void test_moving_solid_retains_displaced_top_phase_and_restores_it(void) {
    const float density[FR_BULK_PHASES]={1.0f,1.1f,1.2f};
    fr_bulk_cell_t source={.volume_fp={30000,20000,10000},.capacity_fp=65536},compressed={0},expanded={0};
    ASSERT_TRUE(fr_bulk_phase_set_capacity(&source,40000,density,&compressed));
    ASSERT_INT_EQ(compressed.capacity_fp,40000);ASSERT_EQ(occupancy(&compressed),40000u);
    ASSERT_INT_EQ(compressed.volume_fp[FR_BULK_WATER],10000);
    ASSERT_INT_EQ(compressed.retained_fp[FR_BULK_WATER],20000);
    ASSERT_INT_EQ(compressed.volume_fp[FR_BULK_OIL],20000);
    ASSERT_INT_EQ(compressed.volume_fp[FR_BULK_SLIME],10000);
    ASSERT_TRUE(fr_bulk_phase_set_capacity(&compressed,60000,density,&expanded));
    ASSERT_EQ(memcmp(expanded.volume_fp,source.volume_fp,sizeof(source.volume_fp)),0);
    ASSERT_INT_EQ(expanded.retained_fp[FR_BULK_WATER],0);PASS();
}
static void test_sealed_capacity_collapse_keeps_each_phase_finite(void) {
    const float density[FR_BULK_PHASES]={1.0f,1.1f,1.2f};
    fr_bulk_cell_t source={.volume_fp={1000,2000,3000},.capacity_fp=65536},out={0};
    ASSERT_TRUE(fr_bulk_phase_set_capacity(&source,0,density,&out));
    ASSERT_EQ(occupancy(&out),0u);
    for(uint32_t i=0;i<FR_BULK_PHASES;i++) ASSERT_EQ(out.retained_fp[i],source.volume_fp[i]);
    PASS();
}
static void test_bulk_packet_roundtrip_and_rejects_overfull_water_pair(void) {
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));fr_bulk_phase_chunk_t *source=calloc(1,sizeof(*source));
    fr_bulk_phase_chunk_t *out=calloc(1,sizeof(*out));size_t length=fr_bulk_phase_chunk_bytes();
    uint8_t *bytes=malloc(length);ASSERT_TRUE(chunk&&source&&out&&bytes&&length>sizeof(*source));
    chunk->cells[17].fluid_mass=30000;source->oil_fp[17]=20000;source->slime_fp[17]=15536;
    source->oil_retained_fp[17]=1234;source->slime_retained_fp[17]=4321;
    ASSERT_TRUE(fr_bulk_phase_chunk_encode(chunk,source,bytes,length));
    ASSERT_TRUE(fr_bulk_phase_chunk_decode(chunk,bytes,length,out));
    ASSERT_EQ(memcmp(out,source,sizeof(*out)),0);
    chunk->cells[17].fluid_mass=30001;
    ASSERT_TRUE(!fr_bulk_phase_chunk_decode(chunk,bytes,length,out));
    ASSERT_EQ(memcmp(out,source,sizeof(*out)),0);
    free(bytes);free(out);free(source);free(chunk);PASS();
}
static void test_bulk_packet_corruption_and_truncation_are_transactional(void) {
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));fr_bulk_phase_chunk_t *source=calloc(1,sizeof(*source));
    fr_bulk_phase_chunk_t *out=calloc(1,sizeof(*out));size_t length=fr_bulk_phase_chunk_bytes();
    uint8_t *bytes=malloc(length);ASSERT_TRUE(chunk&&source&&out&&bytes);
    source->slime_fp[9]=123;out->oil_fp[9]=456;
    ASSERT_TRUE(fr_bulk_phase_chunk_encode(chunk,source,bytes,length));
    ASSERT_TRUE(!fr_bulk_phase_chunk_decode(chunk,bytes,length-1,out));
    ASSERT_INT_EQ(out->oil_fp[9],456);
    bytes[length/2u]^=1u;
    ASSERT_TRUE(!fr_bulk_phase_chunk_decode(chunk,bytes,length,out));
    ASSERT_INT_EQ(out->oil_fp[9],456);
    free(bytes);free(out);free(source);free(chunk);PASS();
}
static void test_bulk_packet_rejects_retained_volume_larger_than_stock(void) {
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    uint8_t *bytes=malloc(fr_bulk_phase_chunk_bytes());ASSERT_TRUE(chunk&&phase&&bytes);
    phase->oil_fp[7]=100u;phase->oil_retained_fp[7]=101u;
    ASSERT_TRUE(!fr_bulk_phase_chunk_encode(chunk,phase,bytes,fr_bulk_phase_chunk_bytes()));
    phase->oil_retained_fp[7]=100u;phase->slime_fp[9]=2u;phase->slime_retained_fp[9]=3u;
    ASSERT_TRUE(!fr_bulk_phase_chunk_encode(chunk,phase,bytes,fr_bulk_phase_chunk_bytes()));
    free(bytes);free(phase);free(chunk);PASS();
}
int main(void) {
    RUN(test_pair_admits_only_shared_capacity_and_conserves_each_phase);
    RUN(test_full_vessel_retains_slime_without_double_occupancy);
    RUN(test_retained_overflow_moves_to_a_free_neighbor_without_loss);
    RUN(test_invalid_phase_is_transactional);
    RUN(test_profile_density_orders_water_oil_and_slime_without_float_flag);
    RUN(test_moving_solid_retains_displaced_top_phase_and_restores_it);
    RUN(test_sealed_capacity_collapse_keeps_each_phase_finite);
    RUN(test_bulk_packet_roundtrip_and_rejects_overfull_water_pair);
    RUN(test_bulk_packet_corruption_and_truncation_are_transactional);
    RUN(test_bulk_packet_rejects_retained_volume_larger_than_stock);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail>0?1:0;
}
