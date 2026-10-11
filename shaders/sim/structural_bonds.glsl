const uint STRUCTURAL_BOND_CAPACITY=192u,STRUCTURAL_BOND_WORDS=20u;
uint bond_record(uint i) { return structural_graph()+30000u+i*STRUCTURAL_BOND_WORDS; }
bool bond_live(uint p) {
    uint a=data[p+1u],b=data[p+2u];
    return data[p]!=0u&&a<BODY_CAPACITY&&b<BODY_CAPACITY&&
        bodies[a].enabled!=0u&&bodies[b].enabled!=0u&&
        bodies[a].id==data[p+3u]&&bodies[b].id==data[p+4u];
}
