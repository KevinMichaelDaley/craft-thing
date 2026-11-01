float coating_oil_fraction(Words entry,uint state) {
    uint word=entry.words[state+FR_STATE_COATING_REMAINDERS];
    uint remainder=(word>>FR_COATING_OIL_REM_SHIFT)&FR_COATING_REM_MASK;
    return float(remainder)/float(FR_COATING_MASS_DEN);
}
float coating_jelly_fraction(Words entry,uint state) {
    uint word=entry.words[state+FR_STATE_COATING_REMAINDERS];
    uint remainder=(word>>FR_COATING_JELLY_REM_SHIFT)&FR_COATING_REM_MASK;
    return float(remainder)/float(FR_COATING_MASS_DEN);
}
float coating_heat_capacity(Words entry,uint state) {
    float oil=float(entry.words[state+FR_STATE_COATING_OIL])+
        coating_oil_fraction(entry,state);
    float jelly=float(entry.words[state+FR_STATE_COATING_JELLY])+
        float(entry.words[state+FR_STATE_COATING_OXIDIZER])+
        coating_jelly_fraction(entry,state);
    return (oil*coefficient(FR_COATING_OIL_MATERIAL,FR_PROFILE_HEAT_CAPACITY_WORD)+
        jelly*coefficient(FR_COATING_JELLY_MATERIAL,FR_PROFILE_HEAT_CAPACITY_WORD))/65536.;
}
float coating_mass(Words entry,uint state) {
    return (float(entry.words[state+FR_STATE_COATING_OIL])+
        float(entry.words[state+FR_STATE_COATING_JELLY])+
        float(entry.words[state+FR_STATE_COATING_OXIDIZER])+
        coating_oil_fraction(entry,state)+coating_jelly_fraction(entry,state))/65536.;
}
