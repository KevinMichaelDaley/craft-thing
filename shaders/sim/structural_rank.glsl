/* A float pivot alone is not proof of a physical mechanism. Certify mobility
 * using rigid member clusters and exact attachment points; otherwise ambiguity
 * is numerical UNKNOWN. Counts are upper bounds, so cycles cannot fake proof. */
ivec2 eq_fixed_position(uint node) {
    uint n=graph()+NODES+node*12u;ivec2 p;
    view_point(pair(n+2u),pair(n+4u),ivec2(0),p);
    return (p<<16)+ivec2(DC_DATA(n+6u),DC_DATA(n+7u));
}
bool eq_mechanism_proven(uint owner) {
    uint g=graph(),cluster[32],clusters=0u,upper=0u;
    for(uint n=0u;n<DC_DATA(g);n++) cluster[n]=DC_DATA(state(n)+1u)==DC_DATA(state(owner)+1u)?n:0xffffffffu;
    for(uint iteration=0u;iteration<32u;iteration++) for(uint e=0u;e<DC_DATA(g+1u);e++) {
        if((DC_DATA(g+MEMBERS+e*8u+6u)&2u)!=0u) continue;
        uint a=uint(eq_a[e]),b=uint(eq_b[e]);
        if(cluster[a]!=0xffffffffu) {uint label=min(cluster[a],cluster[b]);cluster[a]=label;cluster[b]=label;}
    }
    bool any_direction=false,parallel=true;vec2 direction=vec2(0);
    for(uint a=0u;a<DC_DATA(g+3u);a++) if(eq_anchor[a]>=0&&cluster[eq_anchor[a]]!=0xffffffffu)
        for(uint axis=0u;axis<2u;axis++) if(eq_ak[a][axis]>0) {
            vec2 v=axis==0u?eq_normal[a]:vec2(-eq_normal[a].y,eq_normal[a].x);
            if(!any_direction) {any_direction=true;direction=v;}
            else if(body_cross(direction,v)!=0) parallel=false;
        }
    if(!any_direction||parallel) return true;
    for(uint c=0u;c<DC_DATA(g);c++) if(cluster[c]==c) {
        clusters++;uint anchors=0u;bool rotation=false,point_set=false,same_point=true;
        ivec2 point=ivec2(0);
        for(uint a=0u;a<DC_DATA(g+3u);a++) if(eq_anchor[a]>=0&&cluster[eq_anchor[a]]==c) {
            anchors+=uint(eq_ak[a].x>0)+uint(eq_ak[a].y>0)+uint(eq_ak[a].z>0);
            rotation=rotation||eq_ak[a].z>0;
            if(eq_ak[a].x>0||eq_ak[a].y>0) {
                ivec2 p=2*eq_fixed_position(uint(eq_anchor[a]));
                if(!point_set) {point=p;point_set=true;} else if(any(notEqual(point,p))) same_point=false;
            }
        }
        upper+=min(anchors,3u);
        for(uint j=0u;j<DC_DATA(g+2u);j++) {
            uint a=uint(eq_joint_nodes[j].x),b=uint(eq_joint_nodes[j].y);
            if(cluster[a]==cluster[b]||(cluster[a]!=c&&cluster[b]!=c)) continue;
            rotation=rotation||eq_joint_k[j].z>0;
            if(eq_joint_k[j].x>0||eq_joint_k[j].y>0) {
                ivec2 p=eq_fixed_position(a)+eq_fixed_position(b);
                if(!point_set) {point=p;point_set=true;} else if(any(notEqual(point,p))) same_point=false;
            }
        }
        if(!rotation&&point_set&&same_point) return true;
        for(uint other=c+1u;other<DC_DATA(g);other++) if(cluster[other]==other) {
            uint joints=0u;
            for(uint j=0u;j<DC_DATA(g+2u);j++) {
                uint a=cluster[eq_joint_nodes[j].x],b=cluster[eq_joint_nodes[j].y];
                if((a==c&&b==other)||(a==other&&b==c))
                    joints+=uint(eq_joint_k[j].x>0)+uint(eq_joint_k[j].y>0)+uint(eq_joint_k[j].z>0);
            }
            upper+=min(joints,3u);
        }
    }
    return upper<3u*clusters;
}
