#define _POSIX_C_SOURCE 200809L
#define main bonfire_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include "../../src/app/level.c"
#include "../../src/vulkan/reactive_storage_gpu.h"
#include <signal.h>
#include <sys/stat.h>

/* Six authored fixture identities; no gameplay owner lists or body quota. */
typedef struct {uint32_t origins[48],count;uint64_t initial,current;float temperature;} fixture_log_t;
static volatile sig_atomic_t stopped;
static double wall_bound=30.;
static uint64_t diagnostic_char;
static float diagnostic_kindling;
static uint32_t diagnostic_burning_char;
static void stop_run(int signal_number) {(void)signal_number;stopped=1;}
static int log_for_origin(const fixture_log_t logs[6],uint32_t origin) {
    for(uint32_t i=0;i<6;i++) for(uint32_t j=0;j<logs[i].count;j++) if(logs[i].origins[j]==origin) return (int)i;
    return -1;
}
static bool inspect_logs(dc_level_view_t *view,fixture_log_t logs[6],bool initial,float *ember,char *err,uint32_t cap) {
    fr_reactive_record_t *records=NULL;uint32_t count=0;uint64_t next=0;
    if(!fr_gpu_reactive_export_bodies(view->gpu,&records,&count,&next,err,cap)) return false;
    for(uint32_t i=0;i<6;i++) {logs[i].current=0;logs[i].temperature=0;}
    *ember=0;
    diagnostic_char=0;diagnostic_kindling=0;diagnostic_burning_char=0;
    for(uint32_t i=0;i<count;i++) {
        fr_reactive_record_t *r=records+i;float temperature=0;
        if(!fr_reactive_state_temperature(&r->state,&temperature,err,cap)) {free(records);return false;}
        if(r->state.char_mass_fp&&temperature>*ember) *ember=temperature;
        diagnostic_char+=r->state.char_mass_fp;
        if(r->state.char_mass_fp&&(r->state.flags&FR_REACTIVE_FLAMING)) diagnostic_burning_char++;
        if(((r->state.origin_id_low-1u)%4096u)/64u==26u&&r->state.fuel_mass_fp&&temperature>diagnostic_kindling) diagnostic_kindling=temperature;
        int index=log_for_origin(logs,r->state.origin_id_low);
        if(initial) {
            uint32_t origin=r->state.origin_id_low-1u,cell=origin%4096u,slot=origin/4096u,y=cell/64u,x=cell%64u;
            if(!((y>=16u&&y<=18u)||(y>=21u&&y<=23u))) continue;
            if(slot>=view->table.capacity) {free(records);return false;}
            int64_t chunk_x=view->table.slots[slot].coord.x;
            if(chunk_x!=2&&chunk_x!=5) {free(records);return false;}
            uint32_t side=chunk_x==5?1u:0u;
            index=(int)(side*3u+(y<20u?0u:(x<32u?1u:2u)));
            if(logs[index].count>=48u||temperature>294.f||(r->state.flags&FR_REACTIVE_FLAMING)) {free(records);return false;}
            logs[index].origins[logs[index].count++]=r->state.origin_id_low;logs[index].initial+=r->state.fuel_mass_fp;
        }
        if(index<0) continue;
        logs[index].current+=r->state.fuel_mass_fp;
        if(temperature>logs[index].temperature) logs[index].temperature=temperature;
    }
    free(records);return true;
}
static void test_every_cold_dry_rigid_log_autonomously_consumes_fuel(void) {
    char err[256]={0},directory[]="build/rigid_bonfire_acceptance_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    printf("FULLSCREEN rigid bonfire: 30s wall bound; finite ember; no probe or scripted ignition\n");fflush(stdout);
    dc_level_view_t *view=dc_level_view_create_bonfire(directory,7,err,sizeof(err));ASSERT_TRUE(view);
    signal(SIGINT,stop_run);signal(SIGTERM,stop_run);
    fixture_log_t logs[6]={0};float ember=0;
    ASSERT_TRUE(inspect_logs(view,logs,true,&ember,err,sizeof(err)));
    for(uint32_t i=0;i<6;i++) ASSERT_TRUE(logs[i].count==(i%3u==0u?48u:30u)&&logs[i].initial>0);
    uint64_t began=SDL_GetPerformanceCounter(),previous=began,report=began;double frequency=SDL_GetPerformanceFrequency();
    uint32_t dry=0,wet=0;double elapsed=0;
    while(!stopped&&elapsed<wall_bound&&dry<3u) {
        uint64_t now=SDL_GetPerformanceCounter();float dt=(float)((now-previous)/frequency);
        if(dt<1.f/60.f) {SDL_Delay(1);continue;}previous=now;
        ASSERT_TRUE(dt<=.25f);
        SDL_Event event;while(SDL_PollEvent(&event)) if(event.type==SDL_QUIT||
            (event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_ESCAPE)) stopped=1;
        ASSERT_TRUE(dc_level_view_bonfire_presentation(view,0,false)&&dc_level_view_step_timed(view,dt,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
        elapsed=(SDL_GetPerformanceCounter()-began)/frequency;
        if((SDL_GetPerformanceCounter()-report)/frequency>=1.) {
            report=SDL_GetPerformanceCounter();ASSERT_TRUE(inspect_logs(view,logs,false,&ember,err,sizeof(err)));dry=wet=0;
            for(uint32_t i=0;i<6;i++) if(logs[i].initial>=logs[i].current+32u) {if(i<3u) dry++;else wet++;}
            fr_gas_gpu_clock_t clock={0};ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&clock));
            fr_gas_gpu_audit_t audit;ASSERT_TRUE(fr_gas_gpu_audit(view->gpu,&audit,err,sizeof(err)));
            uint32_t counters[14];ASSERT_TRUE(fr_combustion_gpu_bonfire_counters(view->gpu,counters));float target_oxygen=0,surface_temperature=0;
            memcpy(&target_oxygen,counters+9u,sizeof(target_oxygen));memcpy(&surface_temperature,counters+11u,sizeof(surface_temperature));
            printf("BONFIRE wall%.1fs sim%.1fs dry%u/3 wet%u/3 max-log-K %.1f %.1f %.1f ember%.1fK fuel-used %llu %llu %llu\n",
                elapsed,clock.advanced_seconds,dry,wet,logs[0].temperature,logs[1].temperature,logs[2].temperature,ember,
                (unsigned long long)(logs[0].initial-logs[0].current),(unsigned long long)(logs[1].initial-logs[1].current),(unsigned long long)(logs[2].initial-logs[2].current));fflush(stdout);
            printf("THERMAL kindling%.1fK max-gas%.1fK remaining-char%llu burning-char%u\n",diagnostic_kindling,
                audit.metrics[FR_GAS_AUDIT_MAX_TEMPERATURE],(unsigned long long)diagnostic_char,diagnostic_burning_char);fflush(stdout);
            printf("BODY exposure chosen%u viableO2%u occupancy%u gathered%u reacted%u targetO2mass%.9g surface%.1fK\n",
                counters[4],counters[5],counters[6],counters[7],counters[8],target_oxygen,surface_temperature);fflush(stdout);
        }
    }
    mkdir("build/screenshots",0777);uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels);
    uint32_t slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));ASSERT_TRUE(gas&&fr_gas_gpu_read_chunk(view->gpu,slot,gas,4096,err,sizeof(err)));
    for(uint32_t y=26;y<=29;y++) {printf("COLD gas local row%u:",y);
        for(uint32_t x=28;x<=34;x++) {fr_gas_state_t *g=gas+y*64u+x;
            double mass=0;for(uint32_t s=0;s<FR_GAS_SPECIES;s++) mass+=g->species[s];
            printf(" x%u O2%.3f M%.3g",x,mass>0?g->species[FR_GAS_OXYGEN]/mass:0,mass);}
        printf("\n");}
    free(gas);
    ASSERT_TRUE(dc_gpu_readback(view->gpu,pixels,480u*270u,err,sizeof(err)));
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    ASSERT_TRUE(image&&SDL_SaveBMP(image,"build/screenshots/rigid_bonfire_all_logs.bmp")==0);SDL_FreeSurface(image);free(pixels);
    bool saved=dc_level_view_destroy(view,err,sizeof(err));
    if(!saved) printf("Bonfire save also failed: %s\n",err);
    ASSERT_TRUE(!stopped&&dry==3u&&wet==0u);ASSERT_TRUE(saved);PASS();
}
int main(int argc,char **argv) {
    if(argc==2&&strcmp(argv[1],"--diagnostic-10s")==0) wall_bound=10.;
    RUN(test_every_cold_dry_rigid_log_autonomously_consumes_fuel);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
