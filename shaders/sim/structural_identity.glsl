/* Author transactions remap authoritative history by immutable ID/generation,
 * never by the caller's array order. Scratch is outside the impact ledger. */
void identity_stage(uint staged) {
    uint g=graph();
    for(uint member=0u;member<16u;member++) {
        uint target=g+39000u+member*16u;
        for(uint word=0u;word<16u;word++) DC_DATA(target+word)=0u;
        if(member>=DC_DATA(staged+1u)) continue;
        uint next=staged+MEMBERS+member*8u;
        for(uint old=0u;old<DC_DATA(g+1u);old++) {
            uint previous=g+MEMBERS+old*8u;
            if(DC_DATA(next)!=DC_DATA(previous)||DC_DATA(next+1u)!=DC_DATA(previous+1u)) continue;
            for(uint word=0u;word<16u;word++) DC_DATA(target+word)=DC_DATA(g+24000u+old*16u+word);
        }
    }
    for(uint node=0u;node<32u;node++) {
        uint target=g+39300u+node*4u;
        for(uint word=0u;word<4u;word++) DC_DATA(target+word)=0u;
        if(node>=DC_DATA(staged)) continue;
        uint next=staged+NODES+node*12u;
        for(uint old=0u;old<DC_DATA(g);old++) {
            uint previous=g+NODES+old*12u;
            if(DC_DATA(next)!=DC_DATA(previous)||DC_DATA(next+1u)!=DC_DATA(previous+1u)) continue;
            for(uint word=0u;word<4u;word++) DC_DATA(target+word)=DC_DATA(g+28000u+old*4u+word);
            for(uint sibling=0u;sibling<DC_DATA(g);sibling++)
                if(DC_DATA(g+NODES+sibling*12u+8u)==0u&&DC_DATA(state(sibling)+1u)==DC_DATA(state(old)+1u))
                    DC_DATA(target)=max(DC_DATA(target),DC_DATA(g+28000u+sibling*4u));
        }
    }
    for(uint attachment=0u;attachment<96u;attachment++) {
        DC_DATA(g+39500u+attachment)=0u;
        bool anchor=attachment<32u;uint index=anchor?attachment:attachment-32u;
        uint count=DC_DATA(staged+(anchor?3u:2u)),offset=anchor?ANCHORS:JOINTS,stride=anchor?24u:16u;
        if(index>=count) continue;
        uint next=staged+offset+index*stride;
        for(uint old=0u;old<DC_DATA(g+(anchor?3u:2u));old++) {
            uint previous=g+offset+old*stride;
            if(DC_DATA(next)==DC_DATA(previous)&&DC_DATA(next+1u)==DC_DATA(previous+1u))
                DC_DATA(g+39500u+attachment)=DC_DATA(g+28300u+(anchor?old:32u+old));
        }
    }
}
void identity_commit() {
    uint g=graph();
    for(uint word=0u;word<16u*16u;word++) DC_DATA(g+24000u+word)=DC_DATA(g+39000u+word);
    for(uint word=0u;word<32u*4u;word++) DC_DATA(g+28000u+word)=DC_DATA(g+39300u+word);
    for(uint word=0u;word<96u;word++) DC_DATA(g+28300u+word)=DC_DATA(g+39500u+word);
}
