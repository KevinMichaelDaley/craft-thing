uint bonfire_glyph(uint c) {
    const uint font[36]=uint[36](23530u,27566u,14627u,27502u,31143u,18855u,15203u,23533u,29847u,10825u,23469u,31012u,23549u,24573u,11114u,18862u,16234u,23470u,25251u,9367u,31597u,11117u,24557u,23213u,9389u,30863u,31599u,29874u,31183u,29647u,5101u,29671u,31719u,4687u,31727u,29679u);
    if(c>=65u&&c<=90u) return font[c-65u];
    if(c>=48u&&c<=57u) return font[26u+c-48u];
    if(c==47u) return 1u|(1u<<3u)|(2u<<6u)|(4u<<9u)|(4u<<12u);
    if(c==37u) return 5u|(1u<<3u)|(2u<<6u)|(4u<<9u)|(5u<<12u);
    if(c==45u) return 7u<<6u;
    return 0u;
}
uint bonfire_character(uint c) {
    uint side=hud[3],value=0u,digit=0u;
    if(c>=1u&&c<=4u) {value=c<=2u?bitCount(hud[20u+side]):4u;digit=(c==1u||c==3u)?1u:0u;}
    else if(c>=5u&&c<=7u) {value=bitCount(hud[18u+side]);digit=7u-c;}
    else if(c>=8u&&c<=10u) {value=3u;digit=10u-c;}
    else if(c>=11u&&c<=17u) {
        uint index=hud[12],at=atlas(ivec2(index%params.width,index/params.width));uvec2 address;
        float temperature=0.,oxygen=-1.;
        if(floatBitsToUint(samples[index+params.width*params.height].x)==4u&&samples[index].y>0.) temperature=samples[index].x/samples[index].y;
        else if(solid_address(at,address)) {Words e=Words(address);float capacity=solid_capacity(e);
            if(capacity>0.) temperature=(uintBitsToFloat(e.words[FR_ENTRY_STATE+17u])+uintBitsToFloat(e.words[FR_ENTRY_STATE+25u])+uintBitsToFloat(e.words[FR_ENTRY_STATE+26u]))/capacity;}
        else if(gas_cell(at)) {Gas gas=local_gas(at);float capacity=gas_capacity(gas),mass=gas_mass(gas);
            if(capacity>0.) temperature=gas_internal(gas)/capacity;if(mass>0.) oxygen=100.*gas.species.x/mass;}
        if(c<=14u) {value=uint(clamp(temperature,0.,9999.));digit=14u-c;}
        else {if(oxygen<0.) return 45u;value=uint(round(clamp(oxygen,0.,100.)));digit=17u-c;}
    } else return c;
    uint divisor=digit==3u?1000u:(digit==2u?100u:(digit==1u?10u:1u));
    return 48u+(value/divisor)%10u;
}
void bonfire_hud(uint mode,uint i,ivec2 p,uint at) {
    if(mode==6u) return;
    if(mode==7u) {
        if(abs(p.x-int(hud[4]))>28||p.y<int(hud[5])||p.y>int(hud[5])+16) return;
        vec4 meta=samples[i+params.width*params.height];if(floatBitsToUint(meta.x)!=4u) return;
        uint entry_index=floatBitsToUint(meta.y);if(entry_index>=root[2]) return;
        Words e=Words(reactive_entry_address(uvec2(root[4],root[5]),entry_index));
        if(e.words[FR_ENTRY_OWNER]!=2u||e.words[FR_ENTRY_GENERATION]!=floatBitsToUint(meta.w)) return;
        uint s=FR_ENTRY_STATE,m=e.words[s];
        uint initial=uint(round(float(e.words[s+8u])*coefficient(m,9u)));
        if(e.words[s+9u]>=initial) return;
        uint side=hud[3],origin=(e.words[s+4u]-1u)&4095u;int y=int(origin/64u),x=int(origin%64u);
        if(m==7u||m==8u) {
            if(y>=16&&y<=18) atomicOr(hud[18u+side],1u);
            if(y>=21&&y<=23) atomicOr(hud[18u+side],1u<<uint(x<32?1:2));
            if(y==26) {
                int stick=x>=20&&x<=22?0:(x>=26&&x<=28?1:(x>=34&&x<=36?2:(x>=40&&x<=42?3:-1)));
                if(stick>=0) atomicOr(hud[20u+side],1u<<uint(stick));
            }
        }
        return;
    }
    int x=p.x-int(hud[6]),y=p.y-int(hud[7]);if(x<0||x>=80||y<0||y>=42) return;
    uint c=bonfire_character(hud[32u+uint(y/6)*20u+uint(x/4)]),bits=bonfire_glyph(c);
    bool ink=x%4<3&&y%6<5&&((bits>>uint((y%6)*3+2-x%4))&1u)!=0u;
    pixels[i]=ink?0xffe9f3ffu:0xff15100cu;
}
