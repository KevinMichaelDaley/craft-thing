float impact_cut_work(Body body,uint axis,float cut) {
    if(pixel_body(body)) return pixel_cut_work(body,axis,cut);
    float low[4],high[4],toughness[4];uint side[4];float work=0;
    uint count=body_pieces(body),g=graph();
    for(uint piece=0u;piece<count;piece++) {
        uint material=body.piece_count==0u?body.material:body.pieces[piece].material;
        uint profile=g+29000u+(material==2u||material==4u?1u:material==3u?2u:0u)*12u;
        toughness[piece]=uintBitsToFloat(DC_SOLVER_DATA(profile+6u));
        if((material==2u||material==4u)&&axis==1u) toughness[piece]*=uintBitsToFloat(DC_SOLVER_DATA(profile+9u));
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
shared uint impact_color_cursor,impact_color_pending;
bool fracture_impact_step() {
    uint g=graph();
    while(impact_color_cursor<BODY_CAPACITY) {
        uint slot=impact_color_cursor++;
        uint p=body_impact_pending(slot);Body b=DC_BODY(slot);
        if(DC_SOLVER_DATA(p+3u)!=b.id||b.enabled==0u||b.inverse_mass==0||
           (!pixel_body(b)&&b.material>1u)||DC_SOLVER_DATA(p+2u)==0u) continue;
        uint axis=(DC_SOLVER_DATA(p+2u)&255u)-1u;
        vec2 low=vec2(1e20),high=vec2(-1e20);
        for(uint part=0u;part<body_pieces(b);part++)
            for(uint vertex=0u;vertex<piece_vertices(b,part);vertex++) {
                vec2 v=piece_rest_vertex(b,part,vertex);low=min(low,v);high=max(high,v);
            }
        if(high[axis]-low[axis]<2.0) continue;
        float cut=pixel_body(b)?round((low[axis]+high[axis])*.5):round((low[axis]+high[axis])*.5*65536.)/65536.;
        float work=impact_cut_work(b,axis,cut);
        if(work<=0) continue;
        float available=float(min(DC_SOLVER_DATA(p),DC_SOLVER_DATA(p+1u)))*16.0;
        if(available<work) continue;
        uint generation=b.reserved1==0u?1u:b.reserved1;
        if(pixel_body(b)?!pixel_fragment_body(slot,axis,cut,generation):!fragment_body(slot,axis,cut,generation)) continue;
        uint ledger=body_impact_ledger(slot);
        DC_SOLVER_DATA(ledger)=floatBitsToUint(available);
        DC_SOLVER_DATA(ledger+1u)=floatBitsToUint(work);
        DC_SOLVER_DATA(ledger+2u)=floatBitsToUint(available-work);
        DC_SOLVER_DATA(ledger+3u)=b.id;
        DC_SOLVER_DATA(p)=0u;DC_SOLVER_DATA(p+1u)=0u;DC_SOLVER_DATA(p+2u)=0u;
        if(pixel_body(b)) return true;
    }
    return false;
}
void fracture_impacts() {
    uint lane=gl_LocalInvocationID.x;
    leaf_impact_breaks();
    if(lane==0u) impact_color_cursor=0u;
    barrier();
    for(;;) {
        if(lane==0u) impact_color_pending=uint(fracture_impact_step());
        memoryBarrierBuffer();barrier();
        if(impact_color_pending==0u) break;
        uint pool=graph()+40000u;
        uint first=BODY_STORAGE_WORDS+pool+16u+DC_SOLVER_DATA(pool)*color_capacity();
        for(uint word=lane;word<DC_SOLVER_DATA(pool+1u);word+=32u) shape_color_resolve(first+word);
        memoryBarrierBuffer();barrier();
    }
}
