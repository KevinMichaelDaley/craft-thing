#include "bonfire.h"
#include <string.h>
static fr_structural_gpu_node_t node(uint32_t id,int32_t x,int32_t y) {
    return (fr_structural_gpu_node_t){.id=id,.generation=1,.chunk={x/(64*65536),y/(64*65536)},.x_fp=x%(64*65536),.y_fp=y%(64*65536)};
}
static void piece(dc_bonfire_plan_t *out,uint32_t side,uint32_t kind,uint32_t x,uint32_t y,uint32_t width,uint32_t height) {
    uint32_t index=out->count++;
    out->pieces[index]=(dc_bonfire_piece_t){.object=index+1,.kind=kind,.x=x+side*192,.y=y,
        .width=width,.height=height,.wet=side!=0,.temperature_K=kind==DC_BONFIRE_STARTER?1400.f:293.15f};
}
bool dc_bonfire_plan(dc_bonfire_plan_t *out) {
    if(!out) return false;
    memset(out,0,sizeof(*out));out->pit.node_count=16;out->pit.member_count=10;out->pit.anchor_count=12;
    for(uint32_t side=0;side<2;side++) {
        uint32_t x=142+side*192,n=side*8,m=side*5;
        int32_t floor_y=94*65536+32768,left_x=(int32_t)x*65536-98304,right_x=(int32_t)(x+36)*65536+98304;
        for(uint32_t i=0;i<4;i++) out->pit.nodes[n+i]=node(n+i+1,(x+12*i)*65536,floor_y);
        out->pit.nodes[n+4]=node(n+5,left_x,82*65536);out->pit.nodes[n+5]=node(n+6,left_x,96*65536-16);
        out->pit.nodes[n+6]=node(n+7,right_x,82*65536);out->pit.nodes[n+7]=node(n+8,right_x,96*65536-16);
        for(uint32_t i=0;i<3;i++) out->pit.members[m+i]=(fr_structural_gpu_member_t){
            .id=m+i+1,.generation=1,.a=n+i+1,.b=n+i+2,.material=FR_STRUCTURAL_STONE,.depth_fp=3*65536};
        out->pit.members[m+3]=(fr_structural_gpu_member_t){.id=m+4,.generation=1,.a=n+5,.b=n+6,.material=FR_STRUCTURAL_STONE,.depth_fp=3*65536};
        out->pit.members[m+4]=(fr_structural_gpu_member_t){.id=m+5,.generation=1,.a=n+7,.b=n+8,.material=FR_STRUCTURAL_STONE,.depth_fp=3*65536};
        for(uint32_t end=0;end<6;end++) {
            uint32_t ax=end<4?x+12*end:(end==4?x-2:x+37),anchor_node=end<4?n+1+end:n+6+2*(end-4);
            out->pit.anchors[side*6+end]=(fr_structural_gpu_anchor_t){.id=side*6+end+1,.generation=1,.node=anchor_node,
                .kind=FR_JOINT_MOMENT,.chunk={(ax-1)/64,1},.x=(ax-1)%64,.y=32,.width=3,.height=1,
                .terrain_material=DC_MATERIAL_STONE,.normal_stiffness=1e8,.shear_stiffness=1e8,.rotational_stiffness=1e8,
                .bearing_capacity=1e7,.pullout_capacity=1e7,.shear_capacity=1e7,.moment_capacity=1e7,.friction=.6f,.normal_angle=-1.57079632679f};
        }
        piece(out,side,DC_BONFIRE_KINDLING,148,90,3,1);piece(out,side,DC_BONFIRE_KINDLING,154,90,3,1);
        piece(out,side,DC_BONFIRE_STARTER,158,90,3,1);
        piece(out,side,DC_BONFIRE_KINDLING,162,90,3,1);piece(out,side,DC_BONFIRE_KINDLING,168,90,3,1);
        piece(out,side,DC_BONFIRE_LOG,146,85,10,3);piece(out,side,DC_BONFIRE_LOG,164,85,10,3);
        piece(out,side,DC_BONFIRE_LOG,146,80,16,3);
    }
    return true;
}
