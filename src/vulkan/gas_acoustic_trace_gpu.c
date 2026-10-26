#include <stdio.h>

#include "gas_acoustic_gpu.h"

void fr_gas_acoustic_trace_transport(dc_gpu_t *g) {
    uint64_t phases[10],initial[2],setup[6];double scale=g->timestamp_period/1e6;
    if(vkGetQueryPoolResults(g->device,g->timestamp_pool,4,10,sizeof(phases),phases,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS) return;
    fprintf(stderr,"Gas transport GPU ms:");
    for(uint32_t i=1;i<10;i++) fprintf(stderr," %.3f",(phases[i]-phases[i-1u])*scale);
    fprintf(stderr," (X factor/forward/back/apply, Y factor/forward/back/apply, metadata)\n");
    if(vkGetQueryPoolResults(g->device,g->timestamp_pool,0,2,sizeof(initial),initial,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS||
       vkGetQueryPoolResults(g->device,g->timestamp_pool,16,6,sizeof(setup),setup,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS) return;
    fprintf(stderr,"Gas setup GPU tick%.3f geometry%.3f clear%.3f activity%.3f rhs%.3f ms\n",
        (setup[1]-setup[0])*scale,(setup[2]-initial[0])*scale,((setup[3]-setup[2])+(setup[5]-setup[4]))*scale,
        ((setup[4]-setup[3])+(phases[0]-setup[5]))*scale,(initial[1]-phases[9])*scale);
}

void fr_gas_acoustic_trace_times(dc_gpu_t *g,uint32_t tail_query) {
    uint64_t times[4],tail[2];double scale=g->timestamp_period/1e6;
    if(vkGetQueryPoolResults(g->device,g->timestamp_pool,0,4,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS) return;
    fprintf(stderr,"Acoustic GPU init%.3f pressure-group%.3f residual%.3f ms\n",(times[1]-times[0])*scale,
        (times[2]-times[1])*scale,(times[3]-times[2])*scale);
    if(vkGetQueryPoolResults(g->device,g->timestamp_pool,tail_query,2,sizeof(tail),tail,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)==VK_SUCCESS)
        fprintf(stderr,"Acoustic GPU tail finish%.3f commit%.3f ms\n",(tail[0]-times[3])*scale,(tail[1]-tail[0])*scale);
}
