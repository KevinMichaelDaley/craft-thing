/* Timers use integer fixed physics ticks, never wall time or fluid step duration.
 * Sound members reuse their reserved collision record without a second copy. */
const uint STRUCTURAL_RETIRED=2u;
uint failure_clock(uint node) { return graph()+28000u+node*4u; }
bool member_active(uint member) {
    return (data[graph()+MEMBERS+member*8u+6u]&STRUCTURAL_RETIRED)==0u;
}
void failure() {
    if(gl_LocalInvocationID.x!=0u) return;
    uint g=graph();
    for(uint owner=0u;owner<data[g];owner++) {
        uint node=g+NODES+owner*12u;
        if(data[node+8u]!=0u||data[node]!=data[state(owner)+1u]) continue;
        uint status=data[state(owner)+2u],clock=failure_clock(owner);
        if(status==3u) continue;
        uint ticks=0u,members=0u,chosen=0u;
        for(uint m=0u;m<data[g+1u];m++) if(member_active(m)) {
            uint entry=g+MEMBERS+m*8u;
            int a=node_index(g,data[entry+2u]);
            if(data[state(uint(a))+1u]!=data[node]) continue;
            members++;chosen=m;
            float grace=uintBitsToFloat(data[g+29000u+data[entry+4u]*12u+7u]);
            ticks=max(ticks,uint(ceil(grace*60.0)));
        }
        if(status==0u) {data[clock]=0u;continue;}
        if(status!=2u) continue;
        data[clock]=min(data[clock]+1u,ticks);
        if(data[clock]<ticks) continue;
        /* Multi-member assemblies await the bounded dynamic bond reservation.
         * Keep all proxies owned and stationary until the whole island commits. */
        if(members!=1u) {
            for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[node]) data[state(n)+2u]=4u;
            data[g+24400u+owner*16u+1u]=4u;
            continue;
        }
        uint m=g+MEMBERS+chosen*8u,slot=data[m+7u];
        Body b=bodies[slot];
        if(b.enabled==0u||b.reserved0!=STRUCTURAL_SOURCE||b.reserved1!=chosen+1u) continue;
        b.motion_flags=0u;b.motion_ready=0u;b.reserved0=0u;b.reserved1=0u;
        b.vx_fp=0;b.vy_fp=0;b.angular_velocity=0;
        b.rest_ticks=0u;body_properties(b);bodies[slot]=b;
        data[m+6u]|=STRUCTURAL_RETIRED;
        data[g+24000u+chosen*16u+3u]=5u;
        for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[node]) {
            data[g+NODES+n*12u+8u]=1u;data[state(n)+2u]=5u;
        }
        data[g+24400u+owner*16u+1u]=5u;
        data[base()+3u]++;
    }
}
