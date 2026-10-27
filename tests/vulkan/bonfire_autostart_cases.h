#include "../../src/app/bonfire.h"
static void test_finite_ember_automatically_establishes_the_dry_bonfire(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_coord_t coord={2,1};
    dc_thermal_bench_chunk_t *scene=calloc(1,sizeof(*scene));ASSERT_TRUE(scene);
    ASSERT_TRUE(dc_bonfire_build(&coord,1,scene,err,sizeof(err))&&context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,scene->chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_import_chunk(g,0,scene->chunk,&scene->reactive,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_mark_bonfire_world(g));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_gravity(g,900,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    bool burned[4096]={0};uint32_t wood_count=0,ever_burned=0;bool sustained=false,source_spent=false;
    for(uint32_t i=0;i<4096;i++) if(scene->chunk->cells[i].material==DC_MATERIAL_WOOD&&i%64<56) wood_count++;
    for(uint32_t step=0;step<12000;step++) {
        ASSERT_TRUE(fr_thermal_gpu_step(g,.1f,err,sizeof(err))&&fr_gas_gpu_step(g,.1f,err,sizeof(err)));
        if(step%100!=99) continue;
        uint32_t flaming=0;float hottest=0;fr_reactive_record_t record;bool found=false;
        for(uint32_t i=0;i<4096;i++) if(scene->chunk->cells[i].material==DC_MATERIAL_WOOD&&i%64<56) {
            ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,i,&record,&found,err,sizeof(err)));
            if(!found||record.state.fuel_mass_fp<39322) {if(!burned[i]) {burned[i]=true;ever_burned++;}}
            if(found) {
                float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&record.state,&temperature,err,sizeof(err)));
                if(temperature>hottest) hottest=temperature;
                if(record.state.flags&FR_REACTIVE_FLAMING) flaming++;
            }
        }
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,27*64+32,&record,&found,err,sizeof(err)));
        source_spent=!found||record.state.char_mass_fp==0;
        if(source_spent&&flaming) sustained=true;
        printf("Automatic bonfire %.1fs: burned%u/%u flaming%u max%.2fK ember%s\n",(step+1)*.1,
            ever_burned,wood_count,flaming,hottest,source_spent?" spent":" active");fflush(stdout);
        ASSERT_TRUE(present(g,err,sizeof(err)));
        if(sustained&&ever_burned==wood_count) break;
    }
    ASSERT_TRUE(ever_burned==wood_count&&source_spent&&sustained);
    free(scene->chunk);free(scene->colors);fr_reactive_chunk_release(&scene->reactive);free(scene);dc_gpu_destroy(g);PASS();
}
