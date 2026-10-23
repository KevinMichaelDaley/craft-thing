const uint STRUCTURAL_BOND_CAPACITY=192u,STRUCTURAL_BOND_WORDS=20u;
uint bond_record(uint i) { return structural_graph()+30000u+i*STRUCTURAL_BOND_WORDS; }
bool bond_live(uint p) {
    uint a=DC_DATA(p+1u),b=DC_DATA(p+2u);
    return DC_DATA(p)!=0u&&a<BODY_CAPACITY&&b<BODY_CAPACITY&&
        DC_BODY(a).enabled!=0u&&DC_BODY(b).enabled!=0u&&
        DC_BODY(a).id==DC_DATA(p+3u)&&DC_BODY(b).id==DC_DATA(p+4u);
}
