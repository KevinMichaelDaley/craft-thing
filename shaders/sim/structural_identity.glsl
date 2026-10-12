/* Author transactions remap authoritative history by immutable ID/generation,
 * never by the caller's array order. Scratch is outside the impact ledger. */
void identity_stage(uint staged) {
    uint g=graph();
    for(uint member=0u;member<16u;member++) {
        uint target=g+39000u+member*16u;
        for(uint word=0u;word<16u;word++) data[target+word]=0u;
        if(member>=data[staged+1u]) continue;
        uint next=staged+MEMBERS+member*8u;
        for(uint old=0u;old<data[g+1u];old++) {
            uint previous=g+MEMBERS+old*8u;
            if(data[next]!=data[previous]||data[next+1u]!=data[previous+1u]) continue;
            for(uint word=0u;word<16u;word++) data[target+word]=data[g+24000u+old*16u+word];
        }
    }
    for(uint node=0u;node<32u;node++) {
        uint target=g+39300u+node*4u;
        for(uint word=0u;word<4u;word++) data[target+word]=0u;
        if(node>=data[staged]) continue;
        uint next=staged+NODES+node*12u;
        for(uint old=0u;old<data[g];old++) {
            uint previous=g+NODES+old*12u;
            if(data[next]!=data[previous]||data[next+1u]!=data[previous+1u]) continue;
            for(uint word=0u;word<4u;word++) data[target+word]=data[g+28000u+old*4u+word];
            for(uint sibling=0u;sibling<data[g];sibling++)
                if(data[g+NODES+sibling*12u+8u]==0u&&data[state(sibling)+1u]==data[state(old)+1u])
                    data[target]=max(data[target],data[g+28000u+sibling*4u]);
        }
    }
    for(uint attachment=0u;attachment<96u;attachment++) {
        data[g+39500u+attachment]=0u;
        bool anchor=attachment<32u;uint index=anchor?attachment:attachment-32u;
        uint count=data[staged+(anchor?3u:2u)],offset=anchor?ANCHORS:JOINTS,stride=anchor?24u:16u;
        if(index>=count) continue;
        uint next=staged+offset+index*stride;
        for(uint old=0u;old<data[g+(anchor?3u:2u)];old++) {
            uint previous=g+offset+old*stride;
            if(data[next]==data[previous]&&data[next+1u]==data[previous+1u])
                data[g+39500u+attachment]=data[g+28300u+(anchor?old:32u+old)];
        }
    }
}
void identity_commit() {
    uint g=graph();
    for(uint word=0u;word<16u*16u;word++) data[g+24000u+word]=data[g+39000u+word];
    for(uint word=0u;word<32u*4u;word++) data[g+28000u+word]=data[g+39300u+word];
    for(uint word=0u;word<96u;word++) data[g+28300u+word]=data[g+39500u+word];
}
