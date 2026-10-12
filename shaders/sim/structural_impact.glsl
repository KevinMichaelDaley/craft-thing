void fracture_impacts() {
    if(gl_LocalInvocationID.x!=0u) return;
    uint g=graph(),profile=g+29000u;
    for(uint slot=0u;slot<BODY_CAPACITY;slot++) {
        uint p=g+38000u+slot*4u;Body b=bodies[slot];
        if(data[p+3u]!=b.id||b.enabled==0u||b.inverse_mass==0||b.material>1u||data[p+2u]==0u) continue;
        uint axis=data[p+2u]-1u;
        vec2 low=vec2(1e20),high=vec2(-1e20);
        for(uint part=0u;part<body_pieces(b);part++)
            for(uint vertex=0u;vertex<piece_vertices(b,part);vertex++) {
                vec2 v=piece_rest_vertex(b,part,vertex);low=min(low,v);high=max(high,v);
            }
        if(high[axis]-low[axis]<2.0) continue;
        float work=uintBitsToFloat(data[profile+6u])*(high[1u-axis]-low[1u-axis]);
        float available=float(min(data[p],data[p+1u]))*16.0;
        if(available<work) continue;
        uint generation=b.reserved1==0u?1u:b.reserved1;
        if(!fragment_body(slot,axis,(low[axis]+high[axis])*.5,generation)) continue;
        uint ledger=g+38500u+slot*4u;
        data[ledger]=floatBitsToUint(available);
        data[ledger+1u]=floatBitsToUint(work);
        data[ledger+2u]=floatBitsToUint(available-work);
        data[ledger+3u]=b.id;
        data[p]=0u;data[p+1u]=0u;data[p+2u]=0u;
    }
}
