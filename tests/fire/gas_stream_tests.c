#define main existing_stream_main
#include "../world/stream_tests.c"
#undef main
#include <math.h>
static void test_gas_atomic_stream_preserves_private_stock_geometry_and_valves(void) {
    char directory[]="build/gas_stream_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_checkpoint_chunk_t *gas=calloc(1,sizeof(*gas)),*expected=malloc(sizeof(*expected));ASSERT_TRUE(s&&c&&gas&&expected);
    c->coord=(dc_chunk_coord_t){-9,4};gas->cells[27][1]=.0012f;gas->cells[27][5]=.9f;
    gas->cells[27][11]=1e-9f;gas->cells[27][12]=.75f;gas->cells[27][13]=.5f;gas->cells[27][15]=.125f;gas->gates[27]=3u;
    *expected=*gas;
    ASSERT_TRUE(dc_stream_request_save_gas(s,c,NULL,NULL,NULL,gas,1));memset(gas,0,sizeof(*gas));
    dc_stream_result_t r={0};ASSERT_TRUE(wait_result(s,&r));ASSERT_EQ(r.kind,DC_STREAM_SAVED);dc_stream_result_release(&r);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,2)&&wait_result(s,&r));ASSERT_EQ(r.kind,DC_STREAM_LOADED);
    ASSERT_TRUE(r.gas&&!r.water_excess&&r.reactive&&memcmp(expected,r.gas,sizeof(*expected))==0);
    dc_stream_result_release(&r);ASSERT_TRUE(!r.gas);
    expected->cells[4095][0]=-1;ASSERT_TRUE(!dc_stream_request_save_gas(s,c,NULL,NULL,NULL,expected,3));expected->cells[4095][0]=0;
    expected->cells[4095][5]=NAN;ASSERT_TRUE(!dc_stream_request_save_gas(s,c,NULL,NULL,NULL,expected,4));expected->cells[4095][5]=0;
    expected->clock.advanced_seconds=NAN;ASSERT_TRUE(!dc_stream_request_save_gas(s,c,NULL,NULL,NULL,expected,4));expected->clock.advanced_seconds=0.;
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,5)&&wait_result(s,&r));ASSERT_EQ(r.kind,DC_STREAM_LOADED);
    ASSERT_TRUE(r.gas&&memcmp(expected,r.gas,sizeof(*expected))==0);dc_stream_result_release(&r);
    char path[256];snprintf(path,sizeof(path),"%s/chunk_%016llx_%016llx.bin",directory,(unsigned long long)c->coord.x,(unsigned long long)c->coord.y);
    FILE *f=fopen(path,"r+b");ASSERT_TRUE(f&&fseek(f,-9,SEEK_END)==0);int value=fgetc(f);ASSERT_TRUE(value!=EOF);
    ASSERT_EQ(fseek(f,-9,SEEK_END),0);ASSERT_TRUE(fputc(value^1,f)!=EOF);ASSERT_EQ(fclose(f),0);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,6)&&wait_result(s,&r));ASSERT_EQ(r.kind,DC_STREAM_FAILED);dc_stream_result_release(&r);
    ASSERT_TRUE(dc_stream_request_save(s,c,7)&&wait_result(s,&r));ASSERT_EQ(r.kind,DC_STREAM_SAVED);dc_stream_result_release(&r);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,8)&&wait_result(s,&r));ASSERT_EQ(r.kind,DC_STREAM_LOADED);ASSERT_TRUE(!r.gas);dc_stream_result_release(&r);
    dc_stream_destroy(s);free(c);free(gas);free(expected);PASS();
}
static void test_gas_stream_retains_pending_products_at_zero_volume(void) {
    char directory[] = "build/gas_stream_pending_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *stream = dc_stream_create(directory, 123, 4);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_checkpoint_chunk_t *gas = calloc(1, sizeof(*gas));
    ASSERT_TRUE(stream && chunk && gas);
    const uint32_t source = 1300u;
    chunk->coord = (dc_chunk_coord_t){-3, 5};
    chunk->cells[source] = (dc_cell_t){.material = DC_MATERIAL_WATER, .fluid_mass = DC_FLUID_FULL};
    gas->cells[source][2] = .0014f;
    gas->cells[source][4] = .0002f;
    gas->cells[source][5] = 5.f;
    gas->cells[source][9] = .0002f;
    gas->cells[source][11] = 1e-9f;
    gas->carrier_tail[source][0] = 0x1p-36f;
    gas->carrier_tail[source][1] = -0x1p-38f;
    gas->gates[source] = 1u;
    ASSERT_TRUE(dc_stream_request_save_gas(stream, chunk, NULL, NULL, NULL, gas, 1));
    dc_stream_result_t result = {0};
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(stream, chunk->coord, 2) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_TRUE(result.gas && memcmp(gas, result.gas, sizeof(*gas)) == 0);
    ASSERT_EQ(result.chunk->cells[source].fluid_mass, DC_FLUID_FULL);
    dc_stream_result_release(&result);
    gas->carrier_tail[source][0] = INFINITY;
    ASSERT_TRUE(!dc_stream_request_save_gas(stream, chunk, NULL, NULL, NULL, gas, 3));
    gas->carrier_tail[source][0] = 0x1p-36f;
    gas->cells[source][5] = gas->cells[source][11] = 0.f;
    ASSERT_TRUE(!dc_stream_request_save_gas(stream, chunk, NULL, NULL, NULL, gas, 3));
    gas->cells[source][5] = 5.f;
    gas->cells[source][11] = 1e-9f;
    fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
    ASSERT_TRUE(bulk);
    bulk->oil_fp[source + 1u] = 1000u;
    dc_stream_chunk_time_t stamp = {.seconds = 75.5, .known = true};
    ASSERT_TRUE(dc_stream_request_save_snapshot(stream, chunk, NULL, NULL, NULL,
                                               gas, bulk, stamp, 4u));
    ASSERT_TRUE(wait_result(stream, &result) && result.kind == DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(stream, chunk->coord, 5) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_TRUE(result.gas && memcmp(gas, result.gas, sizeof(*gas)) == 0);
    ASSERT_TRUE(result.bulk && result.bulk->oil_fp[source + 1u] == 1000u);
    ASSERT_TRUE(result.time.known && result.time.seconds == stamp.seconds);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    free(bulk);
    free(gas);
    free(chunk);
    PASS();
}

int main(void) {
    RUN(test_gas_atomic_stream_preserves_private_stock_geometry_and_valves);
    RUN(test_gas_stream_retains_pending_products_at_zero_volume);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
