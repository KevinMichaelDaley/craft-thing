#ifndef GAS_SUBGROUP_CASES_H
#define GAS_SUBGROUP_CASES_H
#include "../../src/vulkan/subgroup_gpu.h"
static void test_native_sixteen_lane_transport_preserves_stock_owners_clock_and_quality(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_SUBGROUP16");char *saved=old?SDL_strdup(old):NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));double *volume=malloc(4096*sizeof(*volume));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*quiet=malloc(4096*sizeof(*quiet)),*dormant=malloc(4096*sizeof(*dormant)),*reference=malloc(12288*sizeof(*reference)),*out=malloc(12288*sizeof(*out));
    ASSERT_TRUE(chunk&&volume&&input&&quiet&&dormant&&reference&&out);fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(m,.0012,300,100);quiet[i]=reference_air(m,.0012,300,.001);volume[i]=1.;}
    for(uint32_t y=24;y<32u;y++) for(uint32_t x=20;x<28u;x++) {
        fr_gas_state_t *s=input+y*64u+x;s->species[1]-=.00001;s->species[2]=.00001;s->pigment[0]=.000005;
        s->energy+=.00001*(m.heat_capacity_cv[2]-m.heat_capacity_cv[1])*300;
    }
    uint32_t configured[2]={0},actual[2]={0},work[2]={0};
    for(uint32_t path=0;path<2u;path++) {
        ASSERT_EQ(SDL_setenv("DC_GAS_SUBGROUP16",path?"1":"0",1),0);dc_gpu_t *g=NULL;memset(chunk,0,sizeof(*chunk));
        ASSERT_TRUE(context(&g,err,sizeof(err)));
        if(!dc_gpu_transport_subgroup_size(g->physical)) {printf("Sixteen-lane control unavailable; driver fallback retained\n");dc_gpu_destroy(g);PASS();return;}
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
            fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
            fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,2,input,volume,4096,err,sizeof(err)));
        chunk->coord.x=8;ASSERT_TRUE(dc_gpu_upload_chunk(g,1,chunk,err,sizeof(err))&&dc_gpu_set_page(g,8,0,1,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,1,quiet,volume,4096,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,1,quiet,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,2,dormant,4096,err,sizeof(err)));
        for(uint32_t step=0;step<8u;step++) {
            if(step==0u||step==6u) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,true,err,sizeof(err)));
            if(step==3u) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,false,err,sizeof(err)));
            g_direct_dispatches=g_pressure_dispatches=0;ASSERT_TRUE(fr_gas_gpu_step(g,.04f,err,sizeof(err)));
        }
        work[path]=g_direct_dispatches+g_pressure_dispatches;ASSERT_TRUE(fr_gas_gpu_transport_subgroup(g,configured+path,actual+path));
        fr_gas_state_t *target=path?out:reference;
        for(uint32_t slot=0;slot<3u;slot++) ASSERT_TRUE(fr_gas_gpu_read_chunk(g,slot,target+slot*4096u,4096,err,sizeof(err)));
        ASSERT_EQ(memcmp(target+4096,quiet,4096*sizeof(*quiet)),0);ASSERT_EQ(memcmp(target+8192,dormant,4096*sizeof(*dormant)),0);
        fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock)&&clock.physical_steps==8u&&fabs(clock.advanced_seconds-.32)<1e-7);
        ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);
    }
    double mass=0.,density=0.,energy=0.,energy_error=0.,pigment=0.,color_error=0.,velocity=0.,stock[5]={0},final[5]={0};
    for(uint32_t i=0;i<4096u;i++) {
        fr_gas_thermo_t a,b;ASSERT_TRUE(fr_gas_thermodynamics(&m,reference+i,1,&a,err,sizeof(err))&&fr_gas_thermodynamics(&m,out+i,1,&b,err,sizeof(err)));
        ASSERT_TRUE(b.temperature_K>0.&&b.temperature_K<5000.&&b.pressure<1e5);
        mass+=a.mass;density+=fabs(a.mass-b.mass);energy+=reference[i].energy;energy_error+=fabs(reference[i].energy-out[i].energy);
        for(uint32_t k=0;k<5u;k++) {stock[k]+=input[i].species[k];final[k]+=out[i].species[k];}
        for(uint32_t k=0;k<3u;k++) {pigment+=reference[i].pigment[k];color_error+=fabs(reference[i].pigment[k]-out[i].pigment[k]);}
        for(uint32_t k=0;k<2u;k++) {double d=reference[i].momentum[k]/a.mass-out[i].momentum[k]/b.mass;velocity+=a.mass*d*d;}
    }
    printf("Transport subgroup configured%u/%u actual%u/%u dispatches%u/%u densityL1%.9g energyL1%.9g pigmentL1%.9g velocityRMS%.9g\n",
        configured[1],configured[0],actual[1],actual[0],work[1],work[0],density/mass,energy_error/energy,color_error/pigment,sqrt(velocity/mass));
    for(uint32_t k=0;k<5u;k++) {
        printf("Subgroup stock%u drift%.9g relative%.9g\n",k,final[k]-stock[k],stock[k]>0.?(final[k]-stock[k])/stock[k]:0.);
        ASSERT_TRUE(fabs(final[k]-stock[k])<fmax(1e-9,stock[k]*.0002));
    }
    ASSERT_TRUE(density/mass<1e-3&&energy_error/energy<1e-3&&color_error/pigment<.003&&sqrt(velocity/mass)<.1);
    ASSERT_TRUE(work[0]==work[1]&&configured[0]==0u&&actual[0]==0u&&configured[1]==16u&&actual[1]==16u);
    SDL_setenv("DC_GAS_SUBGROUP16",saved?saved:"0",1);SDL_free(saved);free(chunk);free(volume);free(input);free(quiet);free(dormant);free(reference);free(out);PASS();
}
#endif
