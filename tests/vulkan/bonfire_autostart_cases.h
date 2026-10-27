#include "../../src/app/bonfire.h"
static void test_finite_ember_automatically_establishes_the_dry_bonfire(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_coord_t coord={2,1};
    dc_thermal_bench_chunk_t *scene=calloc(1,sizeof(*scene));ASSERT_TRUE(scene);
    ASSERT_TRUE(dc_bonfire_build(&coord,1,scene,err,sizeof(err))&&context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,scene->chunk,err,sizeof(err))&&dc_gpu_set_page(g,2,2,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_import_chunk(g,0,scene->chunk,&scene->reactive,err,sizeof(err)));
    dc_chunk_t *surrounding=calloc(1,sizeof(*surrounding));ASSERT_TRUE(surrounding);uint32_t slot=1;
    for(uint32_t y=0;y<5;y++) for(uint32_t x=0;x<8;x++) if(x!=2||y!=2) {
        memset(surrounding,0,sizeof(*surrounding));surrounding->coord=(dc_chunk_coord_t){x,(int64_t)y-1};
        for(uint32_t i=0;i<4096;i++) if(y*64+i/64>=160) surrounding->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,surrounding,err,sizeof(err))&&dc_gpu_set_page(g,x,y,slot,err,sizeof(err)));slot++;
    }
    free(surrounding);
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_mark_bonfire_world(g));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_gravity(g,900,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_overlay(g,0));
    bool burned[4096]={0};uint32_t wood_count=0,ever_burned=0;bool sustained=false,source_spent=false;
    for(uint32_t i=0;i<4096;i++) if(scene->chunk->cells[i].material==DC_MATERIAL_WOOD&&i%64<56) wood_count++;
    for(uint32_t step=0;step<12000;step++) {
        ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err))&&fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err)));
        if(step%600!=599) continue;
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
        source_spent=true;float ember_K=0;uint32_t ember_fuel=0,tinder_flaming=0;float tinder_K=0;
        for(uint32_t y=26;y<=27;y++) for(uint32_t x=31;x<=33;x++) {
            ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,y*64+x,&record,&found,err,sizeof(err)));
            if(found&&record.state.char_mass_fp>0) {
                float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&record.state,&temperature,err,sizeof(err)));
                if(temperature>ember_K) ember_K=temperature;
                ember_fuel+=record.state.char_mass_fp;source_spent=false;
            }
        }
        for(uint32_t y=22;y<=25;y++) {
            ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,y*64+32,&record,&found,err,sizeof(err)));
            if(found) {float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&record.state,&temperature,err,sizeof(err)));
                if(temperature>tinder_K) tinder_K=temperature;
                if(record.state.flags&FR_REACTIVE_FLAMING) tinder_flaming++;}
        }
        if(source_spent&&flaming) sustained=true;
        printf("Automatic bonfire %.1fs: burned%u/%u flaming%u max%.2fK ember%s\n",(step+1)/60.,
            ever_burned,wood_count,flaming,hottest,source_spent?" spent":" active");fflush(stdout);
        printf("  charcoal %uQ16 %.2fK, tinder %u flaming %.2fK\n",ember_fuel,ember_K,tinder_flaming,tinder_K);fflush(stdout);
        ASSERT_TRUE(present(g,err,sizeof(err)));
        if(step==599||step==3599||step==7199||step==11999) {
            uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels&&dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
            mkdir("build/screenshots",0777);char path[128];snprintf(path,sizeof(path),"build/screenshots/bonfire_automatic_%us.bmp",(step+1)/60);
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
            ASSERT_TRUE(surface&&SDL_SaveBMP(surface,path)==0);SDL_FreeSurface(surface);free(pixels);
        }
        if(sustained&&ever_burned==wood_count) break;
    }
    ASSERT_TRUE(ever_burned==wood_count&&source_spent&&sustained);
    free(scene->chunk);free(scene->colors);fr_reactive_chunk_release(&scene->reactive);free(scene);dc_gpu_destroy(g);PASS();
}
