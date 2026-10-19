/* Brush transactions reuse the fracture reservation/publication gate. */
uint carve_rest_material(Body b,vec2 point) {
    for(uint piece=0u;piece<body_pieces(b);piece++) {
        bool inside=true;
        for(uint vertex=0u;vertex<piece_vertices(b,piece);vertex++) {
            vec2 a=piece_rest_vertex(b,piece,vertex);
            vec2 z=piece_rest_vertex(b,piece,(vertex+1u)%piece_vertices(b,piece));
            if(body_cross(z-a,point-a)<0.) inside=false;
        }
        if(inside) return max(1u,piece_material(b,piece));
    }
    return 0u;
}
bool carve_raster_source(Body original,out Body source) {
    source=original;
    if(pixel_body(source)) return true;
    uint dw=(source.width+15u)/16u,dh=(source.height+15u)/16u;
    uint pool=graph()+40000u,capacity=data[pool+2u],used=data[pool+1u];
    uint tiles=dw*dh,length=8u+tiles+tiles*33u;
    if(length>capacity-used) return false;
    uint target=pool+16u+data[pool]*capacity+used;
    for(uint word=0u;word<length;word++) {
        data[target+word]=0u;
        for(uint nibble=0u;nibble<8u;nibble++) shape_color_set(BODY_STORAGE_WORDS+target+word,nibble,vec4(0));
    }
    data[target]=length;data[target+1u]=tiles;data[target+2u]=dw;data[target+3u]=dh;
    data[target+6u]=source.width;data[target+7u]=source.height;
    uint count=0u;
    for(uint tile=0u;tile<tiles;tile++) {
        uint offset=8u+tiles+tile*33u;
        data[target+8u+tile]=offset;data[target+offset]=(tile/dw)<<16u|(tile%dw);
        for(uint cell=0u;cell<256u;cell++) {
            uvec2 p=uvec2(tile%dw,tile/dw)*16u+uvec2(cell%16u,cell/16u);
            uint material=any(greaterThanEqual(p,uvec2(source.width,source.height)))?0u:
                carve_rest_material(original,vec2(p)+.5);
            data[target+offset+1u+cell/8u]|=material<<((cell%8u)*4u);count+=uint(material!=0u);
        }
    }
    source.compound_reserved=BODY_STORAGE_WORDS+target;source.piece_count=PIXEL_BODY_FLAG|count;
    return true;
}
void carve_retire_member(uint slot) {
    uint g=graph();
    for(uint member=0u;member<data[g+1u];member++) {
        uint m=g+MEMBERS+member*8u;
        if(data[m+7u]==slot) data[m+6u]|=2u;
    }
}
shared uint carve_changed,carve_pending,carve_hits,carve_leaf_hits,carve_cursor,carve_ids[64];
shared Body carve_source;
void carve_bodies() {
    uint lane=gl_LocalInvocationID.x;
    for(uint slot=lane;slot<BODY_CAPACITY;slot+=32u) carve_ids[slot]=bodies[slot].id;
    if(lane==0u) {
        carve_cursor=0u;
        uint pool=graph()+40000u;
        if(data[pool+2u]==0u) data[pool+2u]=color_capacity();
    }
    memoryBarrierBuffer();barrier();
    for(;;) {
        uint slot=carve_cursor;if(slot>=BODY_CAPACITY) break;
        if(lane==0u) {
            carve_changed=0u;carve_pending=0u;carve_hits=0u;carve_leaf_hits=0u;Body source=bodies[slot];
            if(source.enabled!=0u&&source.id==carve_ids[slot]) {
                vec2 low,high;body_bounds(source,low,high);
                vec2 position=vec2(source.x_fp,source.y_fp)/65536.;
                vec2 point=clamp(vec2(params.origin_x)+.5,position+low,position+high);
                vec2 delta=point-(vec2(params.origin_x)+.5);
                float radius=float(params.origin_y.x)+1.;
                if(dot(delta,delta)<=radius*radius) {
                    Body pixels;
                    if(carve_raster_source(source,pixels)) {
                        carve_source=pixels;carve_pending=1u;
                    }
                }
            }
        }
        memoryBarrierBuffer();barrier();
        if(carve_pending!=0u) {
            for(uint pixel=lane;pixel<body_pieces(carve_source);pixel+=32u) {
                uint material=piece_material(carve_source,pixel);
                bool hit=material!=0u&&carve_pixel(carve_source,pixel);
                fracture_labels[pixel]=hit?0xffffffffu:0u;
                if(hit) atomicOr(carve_hits,1u);
                if(hit&&material==5u) atomicOr(carve_leaf_hits,1u);
            }
        }
        memoryBarrierBuffer();barrier();
        if(carve_leaf_hits!=0u) leaf_reserve(carve_source);
        memoryBarrierBuffer();barrier();
        if(lane==0u&&carve_hits!=0u) {
            if(pixel_fragment_source(slot,carve_source,2u,0.,max(1u,carve_source.reserved1))) {
                carve_retire_member(slot);carve_changed=1u;
            }
        }
        memoryBarrierBuffer();barrier();
        if(carve_leaf_hits!=0u) leaf_finish(carve_source,carve_changed!=0u);
        memoryBarrierBuffer();barrier();
        if(carve_changed!=0u) {
            for(uint bond=lane;bond<192u;bond+=32u) {
                uint p=graph()+30000u+bond*20u;
                if(data[p+1u]==slot||data[p+2u]==slot) data[p]=0u;
            }
            uint pool=graph()+40000u;
            uint first=BODY_STORAGE_WORDS+pool+16u+data[pool]*color_capacity();
            for(uint word=lane;word<data[pool+1u];word+=32u) shape_color_resolve(first+word);
        }
        if(lane==0u) carve_cursor++;
        memoryBarrierBuffer();barrier();
    }
}
