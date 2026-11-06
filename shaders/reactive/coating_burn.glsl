uint coating_credit(uint packed,uint shift) {
    return (packed>>shift)&FR_COATING_REACTION_REM_MASK;
}
uint coating_set_credit(uint packed,uint shift,uint value) {
    uint mask=FR_COATING_REACTION_REM_MASK<<shift;
    return (packed&~mask)|((value&FR_COATING_REACTION_REM_MASK)<<shift);
}
void burn_coating(Words entry,inout Gas gas,inout float admitted,
                  inout float source_temperature,inout uint sources) {
    uint s=FR_ENTRY_STATE,oil=entry.words[s+FR_STATE_COATING_OIL];
    if(oil==0u) return;
    uint jelly=entry.words[s+FR_STATE_COATING_JELLY];
    bool mixed=jelly>0u;
    uint profile=mixed?FR_COATING_GREEK_MATERIAL:FR_COATING_OIL_MATERIAL;
    float capacity=solid_capacity(entry);
    if(capacity<=0.) return;
    float temperature=reactive_parcel_energy(entry,s)/capacity;
    float wet=entry.words[s+FR_STATE_DRY_MASS]>0u?
        min(1.,float(entry.words[s+FR_STATE_BOUND_WATER])/
            float(entry.words[s+FR_STATE_DRY_MASS])):0.;
    float dt=uintBitsToFloat(params.seconds);
    float threshold=uintBitsToFloat(entry.words[s+FR_STATE_COATING_THRESHOLD]);
    if(threshold==0.) threshold=ignition_threshold(
        entry.words[s+4u]^0x7f4a7c15u,entry.words[s+5u]);
    float integral=uintBitsToFloat(entry.words[s+FR_STATE_COATING_INTEGRAL]);
    if(temperature>=coefficient(profile,FR_PROFILE_IGNITION_WORD))
        integral=min(1e20,integral+coefficient(profile,FR_PROFILE_HAZARD_WORD)*dt);
    entry.words[s+FR_STATE_COATING_THRESHOLD]=floatBitsToUint(threshold);
    entry.words[s+FR_STATE_COATING_INTEGRAL]=floatBitsToUint(integral);
    uint stock=mixed?entry.words[s+FR_STATE_COATING_OXIDIZER]:
        uint(floor(max(0.,gas.species.x)*65536.));
    /* The magical mixed compound keeps reacting while its bound stock
     * lasts — under water, cold, regardless — once its fuse has crossed
     * the threshold. Ordinary oil still needs heat and stays douseable. */
    bool lit=integral>=threshold&&stock>0u;
    bool viable=mixed?lit:(temperature>=coefficient(profile,FR_PROFILE_IGNITION_WORD)&&
        wet<.8&&stock>0u);
    uint flags=entry.words[s+FR_STATE_FLAGS];
    if(!viable) flags&=~4u;
    else if(integral>=threshold) flags|=4u;
    entry.words[s+FR_STATE_FLAGS]=flags;
    if(!viable||(flags&4u)==0u) return;
    uint packed=entry.words[s+FR_STATE_COATING_REMAINDERS],total=oil+jelly;
    float fuel_credit=float(coating_credit(packed,FR_COATING_REACTION_REM_SHIFT))/1024.;
    float oxygen_credit=float(coating_credit(packed,FR_COATING_OXIDIZER_REM_SHIFT))/1024.;
    float demand=coefficient(profile,FR_PROFILE_OXYGEN_DEMAND_WORD);
    /* Greek fire is magical: touching water does not douse it. The film
     * boils the water off as vapor and roars — the reaction rate grows
     * with wetness and the steam grants heat beyond conservation. */
    float feed_heat=0.;
    if(mixed&&wet>0.) {
        /* The boil is magical: the water is consumed outright rather than
         * injected as vapor, because a submerged film would otherwise dump
         * a cell-breaking steam spike into its one gas neighbor. */
        uint fed=min(entry.words[s+FR_STATE_BOUND_WATER],
            uint(floor(float(FR_GREEK_WATER_FEED_FP_PER_SECOND)*dt*wet)));
        if(fed>0u) {
            entry.words[s+FR_STATE_BOUND_WATER]-=fed;
            feed_heat=float(fed)*float(FR_GREEK_WATER_FEED_HEAT_PER_FP)/65536.;
        }
    }
    /* The roar tracks the live steam production: the evaporation fraction
     * is the water the film boiled this tick, so actively steaming film
     * burns at up to the full roar multiplier while dry film runs at base. */
    float steam=uintBitsToFloat(entry.words[s+FR_STATE_EVAPORATION_FRACTION]);
    float roar=mixed?1.+min(1.,steam*8.)*float(FR_GREEK_WATER_ROAR_WET_BONUS)
                     :(1.-wet)*(1.-wet);
    float rate=coefficient(profile,FR_PROFILE_BURN_RATE_WORD)*dt*
        (mixed?roar:(1.-wet)*(1.-wet));
    float requested=min(float(total)-fuel_credit,float(total)*(1.-exp(-rate)));
    float funded=min(requested,max(0.,(float(stock)+oxygen_credit)/demand-fuel_credit));
    float progress=max(0.,fuel_credit+funded);
    uint reacted=min(total,uint(floor(progress)));
    packed=coating_set_credit(packed,FR_COATING_REACTION_REM_SHIFT,
        uint(floor(min(1023./1024.,max(0.,progress-float(reacted)))*1024.)));
    if(reacted==0u) {entry.words[s+FR_STATE_COATING_REMAINDERS]=packed;return;}
    float needed=float(reacted)*demand;
    uint oxidizer=uint(ceil(max(0.,needed-oxygen_credit)-.00001));
    if(oxidizer>stock) return;
    float remaining_credit=min(1023./1024.,max(0.,oxygen_credit+float(oxidizer)-needed));
    packed=coating_set_credit(packed,FR_COATING_OXIDIZER_REM_SHIFT,
        uint(floor(remaining_credit*1024.)));
    entry.words[s+FR_STATE_COATING_REMAINDERS]=packed;
    uint oil_used=mixed?min(oil,uint(floor(float(reacted)*float(oil)/float(total)))):reacted;
    uint jelly_used=reacted-oil_used;
    uint ash=uint(floor(float(reacted)*coefficient(profile,FR_PROFILE_ASH_YIELD_WORD)));
    uint soot=uint(floor(float(reacted)*coefficient(profile,FR_PROFILE_SOOT_YIELD_WORD)));
    uint exhaust=reacted+oxidizer-ash-soot;
    entry.words[s+FR_STATE_COATING_OIL]=oil-oil_used;
    entry.words[s+FR_STATE_COATING_JELLY]=jelly-jelly_used;
    if(mixed) entry.words[s+FR_STATE_COATING_OXIDIZER]-=oxidizer;
    else gas.species.x=max(0.,gas.species.x-float(oxidizer)/65536.);
    entry.words[s+FR_STATE_INERT_MASS]+=ash;
    float heat=(float(oil_used)*coefficient(FR_COATING_OIL_MATERIAL,FR_PROFILE_PRIMARY_HEAT_WORD)+
        float(jelly_used)*coefficient(FR_COATING_JELLY_MATERIAL,FR_PROFILE_PRIMARY_HEAT_WORD)-
        float(soot)*float(FR_COMBUSTION_AIRBORNE_ENERGY))/65536.;
    float removed=(float(oil_used)*coefficient(FR_COATING_OIL_MATERIAL,FR_PROFILE_HEAT_CAPACITY_WORD)+
        float(jelly_used+(mixed?oxidizer:0u))*coefficient(FR_COATING_JELLY_MATERIAL,FR_PROFILE_HEAT_CAPACITY_WORD))*
        temperature/65536.;
    float retained=float(ash)*coefficient(entry.words[s],FR_PROFILE_HEAT_CAPACITY_WORD)*
        temperature/65536.;
    add_energy(entry,.7*heat-removed+retained+feed_heat);
    /* A submerged film burns on its bound oxidizer and the products
     * dissolve: no gas commit into a zero-volume cell, where the finish
     * normalization would divide by nothing. The heat still drives the
     * substrate's corrosion stock. */
    if(gas.geometry.y>0.) {
        gas.species.z+=float(exhaust)/65536.;
        gas.dynamics.x+=float(soot)/65536.;
        gas.dynamics.y+=.3*heat+removed-retained+feed_heat;
        gas.visual.rgb+=(mixed?vec3(.10,.20,.14):vec3(.18,.16,.14))*
            float(exhaust+soot)/65536.;
    }
    admitted+=heat;source_temperature=max(source_temperature,temperature);
    sources|=FR_FIRE_EMISSION_SURFACE;
    if(mixed) sources|=FR_FIRE_EMISSION_MAGIC|(FR_GREEK_FIRE_RGB8<<8u);
    if(entry.words[s+FR_STATE_COATING_OIL]==0u||
       (mixed&&(entry.words[s+FR_STATE_COATING_JELLY]==0u||
                entry.words[s+FR_STATE_COATING_OXIDIZER]==0u)))
        entry.words[s+FR_STATE_FLAGS]&=~4u;
}
