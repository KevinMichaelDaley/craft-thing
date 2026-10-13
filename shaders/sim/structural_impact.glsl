float impact_cut_work(Body body,uint axis,float cut) {
    if(pixel_body(body)) return pixel_cut_work(body,axis,cut);
    float low[4],high[4],toughness[4];uint side[4];float work=0;
    uint count=body_pieces(body),g=graph();
    for(uint piece=0u;piece<count;piece++) {
        uint material=body.piece_count==0u?body.material:body.pieces[piece].material;
        uint profile=g+29000u+(material==2u||material==4u?1u:material==3u?2u:0u)*12u;
        toughness[piece]=uintBitsToFloat(data[profile+6u]);
        if((material==2u||material==4u)&&axis==1u) toughness[piece]*=uintBitsToFloat(data[profile+9u]);
        float minimum=1e20,maximum=-1e20;low[piece]=1e20;high[piece]=-1e20;
        for(uint vertex=0u;vertex<piece_vertices(body,piece);vertex++) {
            vec2 a=piece_rest_vertex(body,piece,vertex),b=piece_rest_vertex(body,piece,(vertex+1u)%piece_vertices(body,piece));
            minimum=min(minimum,a[axis]);maximum=max(maximum,a[axis]);
            float da=a[axis]-cut,db=b[axis]-cut;
            if(abs(da)<.5/65536.) {low[piece]=min(low[piece],a[1u-axis]);high[piece]=max(high[piece],a[1u-axis]);}
            if(da*db<0) {
                float coordinate=mix(a[1u-axis],b[1u-axis],da/(da-db));
                low[piece]=min(low[piece],coordinate);high[piece]=max(high[piece],coordinate);
            }
        }
        side[piece]=cut>minimum+.5/65536.&&cut<maximum-.5/65536.?0u:
            abs(cut-maximum)<.5/65536.?1u:abs(cut-minimum)<.5/65536.?2u:3u;
        if(side[piece]==0u) work+=toughness[piece]*max(0.,high[piece]-low[piece]);
    }
    for(uint a=0u;a<count;a++) for(uint b=a+1u;b<count;b++)
        if((side[a]==1u&&side[b]==2u)||(side[a]==2u&&side[b]==1u))
            work+=min(toughness[a],toughness[b])*max(0.,min(high[a],high[b])-max(low[a],low[b]));
    return work;
}
void fracture_impacts() {
    if(gl_LocalInvocationID.x!=0u) return;
    uint g=graph();
    for(uint slot=0u;slot<BODY_CAPACITY;slot++) {
        uint p=g+38000u+slot*4u;Body b=bodies[slot];
        if(data[p+3u]!=b.id||b.enabled==0u||b.inverse_mass==0||
           (!pixel_body(b)&&b.material>1u)||data[p+2u]==0u) continue;
        uint axis=data[p+2u]-1u;
        vec2 low=vec2(1e20),high=vec2(-1e20);
        for(uint part=0u;part<body_pieces(b);part++)
            for(uint vertex=0u;vertex<piece_vertices(b,part);vertex++) {
                vec2 v=piece_rest_vertex(b,part,vertex);low=min(low,v);high=max(high,v);
            }
        if(high[axis]-low[axis]<2.0) continue;
        float cut=pixel_body(b)?round((low[axis]+high[axis])*.5):round((low[axis]+high[axis])*.5*65536.)/65536.;
        float work=impact_cut_work(b,axis,cut);
        if(work<=0) continue;
        float available=float(min(data[p],data[p+1u]))*16.0;
        if(available<work) continue;
        uint generation=b.reserved1==0u?1u:b.reserved1;
        if(pixel_body(b)?!pixel_fragment_body(slot,axis,cut,generation):!fragment_body(slot,axis,cut,generation)) continue;
        uint ledger=g+38500u+slot*4u;
        data[ledger]=floatBitsToUint(available);
        data[ledger+1u]=floatBitsToUint(work);
        data[ledger+2u]=floatBitsToUint(available-work);
        data[ledger+3u]=b.id;
        data[p]=0u;data[p+1u]=0u;data[p+2u]=0u;
    }
}
