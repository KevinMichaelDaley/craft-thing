#include "cached_chunk.glsl"
#include "rigid_shape.glsl"

const uint CACHED_HYDRO_PAGE=0x40000000u;
bool cached_domain;
ivec2 cached_low,cached_high;

void select_hydro_domain(Body b) {
    ivec2 view=ivec2(params.width,params.height);
    cached_domain=b.visible==0u || any(lessThan(b.bounds_low,ivec2(17))) ||
        any(greaterThan(b.bounds_high,view-17));
    if(!cached_domain) return;
    ivec2 tile=ivec2(b.x_fp,b.y_fp)>>22;
    uint registry=structural_base(),count=DC_DATA(registry+1u);
    ivec2 first=tile,last=tile;
    for(uint slot=0u;slot<count;slot++) {
        uint r=registry+32u+slot*8u;
        if(DC_DATA(r+5u)==0u) continue;
        ivec2 t;
        if(!relative_chunk(uvec2(DC_DATA(r),DC_DATA(r+1u)),params.origin_x,t.x) ||
           !relative_chunk(uvec2(DC_DATA(r+2u),DC_DATA(r+3u)),params.origin_y,t.y)) continue;
        // A contiguous column of resident pages cannot exceed the slot count.
        if(any(lessThan(t,tile-int(count))) || any(greaterThan(t,tile+int(count)))) continue;
        first=min(first,t);last=max(last,t);
    }
    cached_low=first*64;cached_high=(last+1)*64;
    if(b.visible!=0u) {cached_low=min(cached_low,ivec2(0));cached_high=max(cached_high,view);}
    if(hydro_control[DC_HYDRO_PEER_WIDTH]!=0u) {
        cached_low=min(cached_low,-hydro_offset());
        cached_high=max(cached_high,hydro_peer_size()-hydro_offset());
    }
}

// The primary pixel kernel specializes its shape accessors. Neighboring
// occupancy can also belong to an independently authored convex collider.
bool cached_convex_overlap(Body b,vec2 low) {
    vec2 high=low+1;
    for(uint piece=0u;piece<max(1u,b.piece_count);piece++) {
        uint n=b.piece_count==0u?body_vertices(b):b.pieces[piece].count;
        vec2 minp=vec2(1e20),maxp=vec2(-1e20);
        bool separated=false;
        for(uint i=0u;i<n;i++) {
            uint j=(i+1u)%n;
            vec2 a=b.piece_count==0u?body_vertex(b,i):
                b.center+body_rotate(vec2(b.pieces[piece].vertices[i])/65536.-b.center,b.angle);
            vec2 z=b.piece_count==0u?body_vertex(b,j):
                b.center+body_rotate(vec2(b.pieces[piece].vertices[j])/65536.-b.center,b.angle);
            if(piece_edge_separates_cell(a,z,low,high)) {separated=true;break;}
            minp=min(minp,a);maxp=max(maxp,a);
        }
        if(!separated && all(greaterThan(high,minp)) && all(lessThan(low,maxp))) return true;
    }
    return false;
}

uint cached_hydro_occupant(uint atlas) {
    uint r=structural_base()+32u+(atlas/4096u)*8u;
    uvec2 x=uvec2(DC_DATA(r),DC_DATA(r+1u)),y=uvec2(DC_DATA(r+2u),DC_DATA(r+3u));
    ivec2 local=ivec2(atlas&63u,(atlas>>6u)&63u);
    for(uint i=0u;i<params.body_count;i++) {
        Body b=DC_BODY(i);
        if(b.enabled==0u) continue;
        ivec2 delta;
        if(!relative_chunk(x,b.chunk_x,delta.x) || !relative_chunk(y,b.chunk_y,delta.y) ||
           any(lessThan(delta,ivec2(-512))) || any(greaterThan(delta,ivec2(511)))) continue;
        ivec2 cell=delta*64+local;
        b.x_fp=b.local_fp.x;b.y_fp=b.local_fp.y;
        if((b.piece_count&PIXEL_BODY_FLAG)!=0u) {
            if(convex_cell_overlap(b,cell)) return b.id;
        } else if(cached_convex_overlap(b,vec2(cell)-vec2(b.local_fp)/65536.)) return b.id;
    }
    return 0u;
}
