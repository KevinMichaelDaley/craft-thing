#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/char_enthalpy.h"
#include "dungeoncraft/coating_layout.h"
#include "dungeoncraft/reactive_state_layout.h"
#include "ferrum/fire/material_profile.h"
#include "dungeoncraft/combustion_parameters.h"
#include "ferrum/fire/reactive_state.h"

_Static_assert(sizeof(fr_reactive_state_t)==FR_STATE_WORDS*4u,"Reactive state ABI changed");
_Static_assert(offsetof(fr_reactive_state_t,thermal_energy)==FR_STATE_ENERGY*4u,"Reactive energy layout changed");
_Static_assert(offsetof(fr_reactive_state_t,thermal_energy_residual)==FR_STATE_ENERGY_RESIDUAL*4u,"Reactive energy remainder layout changed");
_Static_assert(offsetof(fr_reactive_state_t,thermal_energy_correction)==FR_STATE_ENERGY_CORRECTION*4u,"Reactive energy correction layout changed");
_Static_assert(offsetof(fr_reactive_state_t,dry_mass_fp)==FR_STATE_MASS_FIRST*4u,"Reactive species layout changed");
_Static_assert(offsetof(fr_reactive_state_t,pending_shock_energy)==FR_STATE_SHOCK*4u,"Reactive shock layout changed");
_Static_assert(offsetof(fr_reactive_state_t,oxidized_fraction)==FR_STATE_OXIDIZED_FRACTION*4u,"Reactive oxidation layout changed");
_Static_assert(offsetof(fr_reactive_state_t,corroded_fraction)==FR_STATE_CORRODED_FRACTION*4u,"Reactive corrosion layout changed");
static bool error(char *err,uint32_t cap,const char *reason) {
    if(err&&cap) snprintf(err,cap,"%s",reason);
    return false;
}
static uint64_t identity(uint32_t low,uint32_t high) {return (uint64_t)low|((uint64_t)high<<32);}
static uint32_t coating_remainder(uint32_t word,uint32_t shift,uint32_t mask) {
    return (word>>shift)&mask;
}
static double capacity(const fr_reactive_state_t *s) {
    const fr_reactive_profile_t *p=fr_reactive_profile(s->material);
    double core=(double)s->fuel_mass_fp+s->inert_mass_fp+s->core_oxidizer_fp;
    return (core*p->heat_capacity+
        (double)s->char_mass_fp*fr_reactive_profile(DC_MATERIAL_CHAR)->heat_capacity+
        (double)s->bound_water_fp*fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity+
        ((double)s->coating_oil_fuel_fp+coating_remainder(s->coating_remainders,
            FR_COATING_OIL_REM_SHIFT,FR_COATING_REM_MASK)/(double)FR_COATING_MASS_DEN)*
            fr_reactive_profile(DC_MATERIAL_OIL)->heat_capacity+
        ((double)s->coating_jelly_fuel_fp+s->coating_oxidizer_fp+
            coating_remainder(s->coating_remainders,FR_COATING_JELLY_REM_SHIFT,
                FR_COATING_REM_MASK)/(double)FR_COATING_MASS_DEN)*
            fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY)->heat_capacity)/DC_FLUID_FULL;
}
double fr_reactive_state_heat_capacity(const fr_reactive_state_t *s) {
    if(!s||!fr_reactive_profile(s->material)) return 0;
    double base=capacity(s),energy=fr_reactive_state_energy(s);
    return energy>0&&base>0?fr_char_mixture_secant_capacity((float)energy,(float)base,
        (float)s->char_mass_fp/DC_FLUID_FULL):base;
}
double fr_reactive_state_energy_at_temperature(const fr_reactive_state_t *s,float temperature) {
    if(!s||!fr_reactive_profile(s->material)||!isfinite(temperature)||temperature<=0) return 0;
    double char_mass=(double)s->char_mass_fp/DC_FLUID_FULL;
    return capacity(s)*temperature+char_mass*(fr_char_specific_enthalpy(temperature)-
        FR_CHAR_BASE_HEAT_CAPACITY*temperature);
}
double fr_reactive_state_energy(const fr_reactive_state_t *s) {
    return s?(double)s->thermal_energy+s->thermal_energy_residual+s->thermal_energy_correction:0;
}
static bool set_energy(fr_reactive_state_t *s,double energy) {
    if(!isfinite(energy)||energy<0||energy>FLT_MAX) return false;
    s->thermal_energy=(float)energy;
    double residual=energy-s->thermal_energy;
    s->thermal_energy_residual=(float)residual;
    s->thermal_energy_correction=(float)(residual-s->thermal_energy_residual);return true;
}
bool fr_reactive_state_add_heat(fr_reactive_state_t *s,double energy,char *err,uint32_t cap) {
    if(!isfinite(energy)||!fr_reactive_state_valid(s,err,cap)) return error(err,cap,"Invalid compensated heat input");
    fr_reactive_state_t value=*s;
    if(!set_energy(&value,fr_reactive_state_energy(s)+energy)||!fr_reactive_state_valid(&value,err,cap))
        return error(err,cap,"Compensated heat exceeds its physical storage");
    *s=value;return true;
}

bool fr_reactive_state_default(uint32_t material,uint32_t mass,uint32_t water,
                                float temperature,uint64_t id,fr_reactive_state_t *out,
                                char *err,uint32_t cap) {
    const fr_reactive_profile_t *p=fr_reactive_profile(material);
    if(!p||!out||id==0||!isfinite(temperature)||temperature<=0)
        return error(err,cap,"Invalid reactive parcel initialization");
    fr_reactive_state_t s={.material=material,.parcel_id_low=(uint32_t)id,
        .parcel_id_high=(uint32_t)(id>>32),.origin_id_low=(uint32_t)id,
        .origin_id_high=(uint32_t)(id>>32),.dry_mass_fp=mass,.bound_water_fp=water};
    uint32_t fuel=(uint32_t)llround((double)mass*p->fuel_fraction);
    uint32_t oxidizer=(uint32_t)llround((double)mass*p->bound_oxidizer_fraction);
    if(fuel>mass) fuel=mass;
    if(oxidizer>mass-fuel) oxidizer=mass-fuel;
    if(material==DC_MATERIAL_CHAR) s.char_mass_fp=fuel;else s.fuel_mass_fp=fuel;
    s.core_oxidizer_fp=oxidizer;s.inert_mass_fp=mass-fuel-oxidizer;
    if(material==DC_MATERIAL_CORROSION_RESIDUE) {
        s.corroded_fraction=1.f;
        s.flags|=FR_REACTIVE_SPENT;
    }
    s.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&s,temperature);
    if(!fr_reactive_state_valid(&s,err,cap)) return false;
    *out=s;return true;
}
bool fr_reactive_state_from_particle(const dc_mpm_particle_t *p,float t,uint64_t id,
                                      fr_reactive_state_t *out,char *err,uint32_t cap) {
    const fr_reactive_profile_t *profile=p?fr_reactive_profile(p->material):NULL;
    if(!p||!profile||!out||p->mass_fp==0||p->mass_fp==UINT32_MAX||
       profile->phase!=FR_REACTIVE_GRANULAR||(p->id_lo==0&&p->id_hi==0))
        return error(err,cap,"Invalid granular reactive parcel");
    bool mirror=p->material==DC_MATERIAL_DIRT||p->material==DC_MATERIAL_SAND;
    fr_reactive_state_t state;
    if(!fr_reactive_state_default(p->material,p->mass_fp,
        mirror?(p->flags&DC_MPM_MOISTURE_MASK):0,t,id,&state,err,cap)) return false;
    if(mirror) state.flags|=FR_REACTIVE_WATER_MIRROR;
    *out=state;return true;
}
bool fr_reactive_state_valid(const fr_reactive_state_t *s,char *err,uint32_t cap) {
    if(!s||!fr_reactive_profile(s->material)||(s->flags&~31u)!=0||
       identity(s->parcel_id_low,s->parcel_id_high)==0||identity(s->origin_id_low,s->origin_id_high)==0)
        return error(err,cap,"Invalid reactive identity/flags");
    if(!isfinite(s->evaporation_fraction)||s->evaporation_fraction<0.f||
       s->evaporation_fraction>=1.f||
       (s->bound_water_fp==0u&&s->evaporation_fraction!=0.f))
        return error(err,cap,"Invalid evaporation progress");
    uint32_t oil_rem=coating_remainder(s->coating_remainders,
        FR_COATING_OIL_REM_SHIFT,FR_COATING_REM_MASK);
    uint32_t jelly_rem=coating_remainder(s->coating_remainders,
        FR_COATING_JELLY_REM_SHIFT,FR_COATING_REM_MASK);
    uint32_t fuel_rem=coating_remainder(s->coating_remainders,
        FR_COATING_JELLY_FUEL_REM_SHIFT,FR_COATING_FUEL_REM_MASK);
    if((s->coating_remainders&~FR_COATING_REM_USED_MASK)!=0u||
       oil_rem>=FR_COATING_MASS_DEN||jelly_rem>=FR_COATING_MASS_DEN||
       fuel_rem>=FR_COATING_JELLY_FUEL_DEN)
        return error(err,cap,"Invalid coating mass remainder");
    if(!isfinite(s->reaction_fraction)||s->reaction_fraction<0||s->reaction_fraction>=1)
        return error(err,cap,"Invalid fractional reaction progress");
    float step=s->thermal_energy-nextafterf(s->thermal_energy,0.f);
    float next=nextafterf(s->thermal_energy,INFINITY);
    if(isfinite(next)) step=next-s->thermal_energy;
    if(!isfinite(s->thermal_energy_residual)||fabsf(s->thermal_energy_residual)>step*.5f)
        return error(err,cap,"Invalid reactive energy remainder");
    float residual_step=nextafterf(fabsf(s->thermal_energy_residual),INFINITY)-fabsf(s->thermal_energy_residual);
    if(!isfinite(s->thermal_energy_correction)||fabsf(s->thermal_energy_correction)>residual_step*.5f)
        return error(err,cap,"Invalid reactive energy accumulation correction");
    float values[8];memcpy(values,&s->thermal_energy,sizeof(values));
    for(unsigned i=0;i<8;i++) if(!isfinite(values[i])||values[i]<0)
        return error(err,cap,"Invalid reactive energy/exposure");
    if(s->oxidized_fraction>1||s->corroded_fraction>1||
       (s->ignition_threshold==0&&s->ignition_integral!=0)||
       (s->coating_threshold==0&&s->coating_integral!=0))
        return error(err,cap,"Invalid oxidation/ignition progress");
    if((s->material==DC_MATERIAL_CORROSION_RESIDUE&&
        (s->corroded_fraction!=1.f||(s->flags&FR_REACTIVE_SPENT)==0u))||
       ((s->material==DC_MATERIAL_GOLD||s->material==DC_MATERIAL_MAGIC_INGREDIENT)&&
        s->corroded_fraction>=1.f)||
       (s->material==DC_MATERIAL_ASH&&s->corroded_fraction>0.f&&
        (s->corroded_fraction!=1.f||(s->flags&FR_REACTIVE_SPENT)==0u)))
        return error(err,cap,"Invalid spent corrosion lineage");
    double c=capacity(s);
    if((c==0&&(s->thermal_energy!=0||s->pending_shock_energy!=0))||
       (c>0&&s->thermal_energy<=0)) return error(err,cap,"Reactive energy has no valid heat capacity");
    if(!isfinite(s->rot_exposure)||s->rot_exposure<0.f)
        return error(err,cap,"Invalid rot exposure");
    return true;
}
bool fr_wood_rot_reference(const fr_reactive_state_t *state,
    float immersed_fraction,double seconds,fr_reactive_state_t *out,
    char *err,uint32_t cap) {
    if(!state||!out||!isfinite(immersed_fraction)||immersed_fraction<0.f||
       immersed_fraction>1.f||!isfinite(seconds)||seconds<0||
       !fr_reactive_state_valid(state,err,cap))
        return error(err,cap,"Invalid wood rot input");
    fr_reactive_state_t next=*state;
    const fr_reactive_profile_t *p=fr_reactive_profile(state->material);
    if(!p) return error(err,cap,"Missing wood material profile");
    double capacity=(double)state->dry_mass_fp*p->moisture_capacity;
    float wet=capacity>0.?(float)((double)state->bound_water_fp/capacity):0.f;
    /* The rot window is the air-water interface: partly wet, partly dry. */
    bool interface=wet>0.05f&&wet<0.95f;
    if(interface&&seconds>0.)
        next.rot_exposure+=(float)(FR_ROT_EXPOSURE_PER_SECOND*seconds*
            fmin(1.,(double)immersed_fraction));
    /* Each full exposure unit converts one rot stage of fuel to inert rot
     * residue, mass conserving: the wood weakens and its draft changes. */
    double stages=floor(next.rot_exposure);
    if(stages>0.&&(next.flags&FR_REACTIVE_SPENT)==0u&&next.fuel_mass_fp>0u) {
        uint32_t convert=(uint32_t)stages*FR_ROT_FUEL_CONVERT_FP;
        if(convert>next.fuel_mass_fp) convert=next.fuel_mass_fp;
        next.fuel_mass_fp-=convert;
        next.inert_mass_fp+=convert;
        next.rot_exposure-=(float)stages;
    }
    if(!fr_reactive_state_valid(&next,err,cap))
        return error(err,cap,"Wood rot corrupted the parcel");
    *out=next;return true;
}
uint64_t fr_reactive_state_mass_fp(const fr_reactive_state_t *s) {
    uint32_t values[9];memcpy(values,&s->dry_mass_fp,sizeof(values));
    uint64_t mass=0;for(unsigned i=1;i<9;i++) mass+=values[i];return mass;
}
double fr_reactive_state_mass_exact_fp(const fr_reactive_state_t *s) {
    if(!s) return 0;
    return (double)fr_reactive_state_mass_fp(s)+
        ((double)coating_remainder(s->coating_remainders,
            FR_COATING_OIL_REM_SHIFT,FR_COATING_REM_MASK)+
         (double)coating_remainder(s->coating_remainders,
            FR_COATING_JELLY_REM_SHIFT,FR_COATING_REM_MASK))/
        (double)FR_COATING_MASS_DEN;
}
double fr_reactive_state_pending_oxygen_fp(const fr_reactive_state_t *s) {
    if(!s) return 0;
    bool char_burn=s->fuel_mass_fp==0&&s->char_mass_fp>0;
    const fr_reactive_profile_t *p=
        fr_reactive_profile(char_burn?DC_MATERIAL_CHAR:s->material);
    return p?(double)s->reaction_fraction*(double)p->oxygen_per_fuel:0;
}
bool fr_reactive_state_temperature(const fr_reactive_state_t *s,float *out,char *err,uint32_t cap) {
    if(!out||!fr_reactive_state_valid(s,err,cap)) return false;
    double c=capacity(s);
    if(c<=0) return error(err,cap,"An empty parcel has no material temperature");
    float t=fr_char_mixture_temperature((float)fr_reactive_state_energy(s),(float)c,
        (float)s->char_mass_fp/DC_FLUID_FULL);
    if(!isfinite(t)) return error(err,cap,"Reactive temperature overflow");
    *out=t;return true;
}
bool fr_reactive_state_split(const fr_reactive_state_t *s,uint32_t weight,uint64_t a,uint64_t b,
                              fr_reactive_state_t *left,fr_reactive_state_t *right,char *err,uint32_t cap) {
    if(!left||!right||left==right||weight>DC_FLUID_FULL||a==0||b==0||a==b||
       !fr_reactive_state_valid(s,err,cap)) return error(err,cap,"Invalid reactive split");
    fr_reactive_state_t first=*s,second=*s;
    uint32_t source[9],low[9],high[9];memcpy(source,&s->dry_mass_fp,sizeof(source));
    for(unsigned i=0;i<9;i++) {
        low[i]=(uint32_t)(((uint64_t)source[i]*weight)>>16);
        high[i]=source[i]-low[i];
    }
    memcpy(&first.dry_mass_fp,low,sizeof(low));memcpy(&second.dry_mass_fp,high,sizeof(high));
    first.coating_remainders=0u;second.coating_remainders=0u;
    const uint32_t shifts[]={FR_COATING_OIL_REM_SHIFT,FR_COATING_JELLY_REM_SHIFT,
        FR_COATING_JELLY_FUEL_REM_SHIFT,FR_COATING_REACTION_REM_SHIFT,
        FR_COATING_OXIDIZER_REM_SHIFT};
    const uint32_t masks[]={FR_COATING_REM_MASK,FR_COATING_REM_MASK,
        FR_COATING_FUEL_REM_MASK,FR_COATING_REACTION_REM_MASK,
        FR_COATING_OXIDIZER_REM_MASK};
    for(uint32_t i=0u;i<5u;i++) {
        uint32_t rem=coating_remainder(s->coating_remainders,shifts[i],masks[i]);
        uint32_t left_rem=(uint32_t)(((uint64_t)rem*weight)>>16);
        first.coating_remainders|=left_rem<<shifts[i];
        second.coating_remainders|=(rem-left_rem)<<shifts[i];
    }
    first.parcel_id_low=(uint32_t)a;first.parcel_id_high=(uint32_t)(a>>32);
    second.parcel_id_low=(uint32_t)b;second.parcel_id_high=(uint32_t)(b>>32);
    double total=capacity(s),fraction=total>0?capacity(&first)/total:0;
    double energy=fr_reactive_state_energy(s);
    if(!set_energy(&first,energy*fraction)||!set_energy(&second,energy-fr_reactive_state_energy(&first)))
        return error(err,cap,"Reactive split energy overflow");
    first.pending_shock_energy=(float)(s->pending_shock_energy*fraction);
    second.pending_shock_energy=s->pending_shock_energy-first.pending_shock_energy;
    uint32_t first_active=s->fuel_mass_fp?first.fuel_mass_fp:first.char_mass_fp;
    uint32_t second_active=s->fuel_mass_fp?second.fuel_mass_fp:second.char_mass_fp;
    double active=(double)first_active+second_active;
    first.reaction_fraction=active>0.?(float)(s->reaction_fraction*first_active/active):0.f;
    second.reaction_fraction=s->reaction_fraction-first.reaction_fraction;
    first.evaporation_fraction=s->bound_water_fp>0u?
        (float)((double)s->evaporation_fraction*first.bound_water_fp/s->bound_water_fp):0.f;
    second.evaporation_fraction=s->evaporation_fraction-first.evaporation_fraction;
    if(!fr_reactive_state_valid(&first,err,cap)||!fr_reactive_state_valid(&second,err,cap)) return false;
    *left=first;*right=second;return true;
}
fr_reactive_owner_t fr_reactive_cell_owner(dc_chunk_coord_t c,uint32_t cell) {
    uint64_t x=(uint64_t)c.x,y=(uint64_t)c.y;
    return (fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_CELL,
        .key={(uint32_t)x,(uint32_t)(x>>32),(uint32_t)y,(uint32_t)(y>>32),cell}};
}
bool fr_reactive_owner_valid(const fr_reactive_owner_t *o,char *err,uint32_t cap) {
    if(!o) return error(err,cap,"Missing reactive owner");
    if(o->kind==FR_REACTIVE_OWNER_CELL)
        return o->key[4]<DC_CHUNK_CELLS||error(err,cap,"Invalid local reactive cell");
    if(o->kind==FR_REACTIVE_OWNER_BODY)
        return (o->key[0]!=0&&o->key[1]!=0&&o->key[2]<=INT32_MAX&&o->key[3]<=INT32_MAX&&o->key[4]==0)||
            error(err,cap,"Invalid reactive body generation/rest coordinate");
    if(o->kind==FR_REACTIVE_OWNER_PARTICLE)
        return ((o->key[0]!=0||o->key[1]!=0)&&o->key[4]==0)||
            error(err,cap,"Invalid reactive particle identity");
    return error(err,cap,"Unknown reactive owner kind");
}
bool fr_reactive_expose_shock(const fr_reactive_profile_t *profile,
    const fr_reactive_state_t *state, double declared_energy,
    fr_reactive_state_t *out, double *deposited, char *err, uint32_t cap) {
    if(!profile||!state||!out||!deposited||!isfinite(declared_energy)||
       declared_energy<0||!fr_reactive_state_valid(state,err,cap))
        return error(err,cap,"Invalid electrical exposure");
    *out=*state;
    const fr_reactive_profile_t *water=fr_reactive_profile(DC_MATERIAL_WATER);
    if(!water) return error(err,cap,"Missing water conductivity profile");
    double conductivity=profile->electrical_conductivity;
    double saturation=profile->moisture_capacity>0&&state->dry_mass_fp?
        (double)state->bound_water_fp/((double)state->dry_mass_fp*
        profile->moisture_capacity):0.;
    if(saturation>1.) saturation=1.;
    double effective=(1.-saturation)*conductivity+saturation*water->electrical_conductivity;
    if(effective<0.) effective=0.;
    double admitted=declared_energy*effective;
    *out=*state;
    if(admitted>0.) {
        float share=(float)admitted;
        if(!isfinite(share)||share<=0.f)
            return error(err,cap,"Electrical exposure underflow");
        out->pending_shock_energy+=share;
        if(!fr_reactive_state_valid(out,err,cap))
            return error(err,cap,"Electrical exposure corrupted the parcel");
        *deposited=(double)share;
    } else {
        *deposited=0.;
    }
    return true;
}
