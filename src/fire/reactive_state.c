#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/reactive_state_layout.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/reactive_state.h"

_Static_assert(sizeof(fr_reactive_state_t)==FR_STATE_WORDS*4u,"Reactive state ABI changed");
_Static_assert(offsetof(fr_reactive_state_t,thermal_energy)==FR_STATE_ENERGY*4u,"Reactive energy layout changed");
_Static_assert(offsetof(fr_reactive_state_t,dry_mass_fp)==FR_STATE_MASS_FIRST*4u,"Reactive species layout changed");
_Static_assert(offsetof(fr_reactive_state_t,pending_shock_energy)==FR_STATE_SHOCK*4u,"Reactive shock layout changed");
static bool error(char *err,uint32_t cap,const char *reason) {
    if(err&&cap) snprintf(err,cap,"%s",reason);
    return false;
}
static uint64_t identity(uint32_t low,uint32_t high) {return (uint64_t)low|((uint64_t)high<<32);}
static double capacity(const fr_reactive_state_t *s) {
    const fr_reactive_profile_t *p=fr_reactive_profile(s->material);
    double core=(double)s->fuel_mass_fp+s->inert_mass_fp+s->core_oxidizer_fp;
    return (core*p->heat_capacity+
        (double)s->char_mass_fp*fr_reactive_profile(DC_MATERIAL_CHAR)->heat_capacity+
        (double)s->bound_water_fp*fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity+
        (double)s->coating_oil_fuel_fp*fr_reactive_profile(DC_MATERIAL_OIL)->heat_capacity+
        ((double)s->coating_jelly_fuel_fp+s->coating_oxidizer_fp)*
            fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY)->heat_capacity)/DC_FLUID_FULL;
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
    s.thermal_energy=(float)(capacity(&s)*temperature);
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
    if(s->identity_reserved[0]||s->identity_reserved[1]||s->reserved[0]||s->reserved[1]||s->reserved[2])
        return error(err,cap,"Nonzero reactive reserved fields");
    float values[8];memcpy(values,&s->thermal_energy,sizeof(values));
    for(unsigned i=0;i<8;i++) if(!isfinite(values[i])||values[i]<0)
        return error(err,cap,"Invalid reactive energy/exposure");
    if(s->oxidized_fraction>1||s->corroded_fraction>1||
       (s->ignition_threshold==0&&s->ignition_integral!=0)||
       (s->coating_threshold==0&&s->coating_integral!=0))
        return error(err,cap,"Invalid oxidation/ignition progress");
    double c=capacity(s);
    if((c==0&&(s->thermal_energy!=0||s->pending_shock_energy!=0))||
       (c>0&&s->thermal_energy<=0)) return error(err,cap,"Reactive energy has no valid heat capacity");
    return true;
}
uint64_t fr_reactive_state_mass_fp(const fr_reactive_state_t *s) {
    if(!s) return 0;
    uint32_t values[9];memcpy(values,&s->dry_mass_fp,sizeof(values));
    uint64_t mass=0;for(unsigned i=1;i<9;i++) mass+=values[i];return mass;
}
bool fr_reactive_state_temperature(const fr_reactive_state_t *s,float *out,char *err,uint32_t cap) {
    if(!out||!fr_reactive_state_valid(s,err,cap)) return false;
    double c=capacity(s);
    if(c<=0) return error(err,cap,"An empty parcel has no material temperature");
    float t=(float)(s->thermal_energy/c);
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
    first.parcel_id_low=(uint32_t)a;first.parcel_id_high=(uint32_t)(a>>32);
    second.parcel_id_low=(uint32_t)b;second.parcel_id_high=(uint32_t)(b>>32);
    double total=capacity(s),fraction=total>0?capacity(&first)/total:0;
    first.thermal_energy=(float)(s->thermal_energy*fraction);
    second.thermal_energy=s->thermal_energy-first.thermal_energy;
    first.pending_shock_energy=(float)(s->pending_shock_energy*fraction);
    second.pending_shock_energy=s->pending_shock_energy-first.pending_shock_energy;
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
