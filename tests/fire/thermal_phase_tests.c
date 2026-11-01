#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %s:%d: %s [%s]\n",__FILE__,__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static fr_gas_model_t model(void) {
    return (fr_gas_model_t){.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
}
static fr_gas_state_t air(const fr_gas_model_t *m) {
    fr_gas_state_t gas={0};gas.species[FR_GAS_OXYGEN]=.0003;
    gas.species[FR_GAS_NITROGEN]=.0009;
    gas.energy=300.*(gas.species[FR_GAS_OXYGEN]*m->heat_capacity_cv[FR_GAS_OXYGEN]+
        gas.species[FR_GAS_NITROGEN]*m->heat_capacity_cv[FR_GAS_NITROGEN]);
    return gas;
}
static double reference_offset(const fr_gas_model_t *m) {
    return 2257.0+(4.18-m->heat_capacity_cv[FR_GAS_VAPOR]-
        m->gas_constant[FR_GAS_VAPOR])*373.15;
}
static void test_bound_water_enters_vapor_with_finite_mass_and_total_energy(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t gas=air(&m),next_gas;
    fr_reactive_state_t wood,next_wood;uint32_t moved=0;double offset=0;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,500,1,&wood,err,sizeof(err)));
    wood.ignition_threshold=.7f;wood.ignition_integral=.4f;
    ASSERT_TRUE(fr_thermal_vapor_reference_energy(&m,&offset,err,sizeof(err)));
    ASSERT_TRUE(fabs(offset-reference_offset(&m))<1e-3);
    double before=fr_reactive_state_energy(&wood)+gas.energy+gas.species[FR_GAS_VAPOR]*offset;
    ASSERT_TRUE(fr_thermal_evaporate_pair(&wood,&gas,&m,1000,&next_wood,&next_gas,&moved,err,sizeof(err)));
    ASSERT_EQ(moved,1000u);ASSERT_EQ(next_wood.bound_water_fp,9000u);
    ASSERT_TRUE(fabs(next_gas.species[FR_GAS_VAPOR]-1000./65536.)<1e-12);
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&next_wood)+next_gas.energy+
        next_gas.species[FR_GAS_VAPOR]*offset-before)<1e-5);
    ASSERT_EQ(next_wood.parcel_id_low,wood.parcel_id_low);
    ASSERT_EQ(next_wood.ignition_integral,wood.ignition_integral);
    ASSERT_TRUE(fr_reactive_state_energy(&next_wood)/fr_reactive_state_heat_capacity(&next_wood)<500);
    PASS();
}
static void test_overdraw_pause_and_invalid_gas_leave_inventories_safe(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t gas=air(&m),next_gas;
    fr_reactive_state_t wood,next_wood;uint32_t moved=99;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,300,1,&wood,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_evaporate_pair(&wood,&gas,&m,0,&next_wood,&next_gas,&moved,err,sizeof(err)));
    ASSERT_EQ(moved,0u);ASSERT_TRUE(memcmp(&next_wood,&wood,sizeof(wood))==0);
    ASSERT_TRUE(memcmp(&next_gas,&gas,sizeof(gas))==0);
    ASSERT_TRUE(fr_thermal_evaporate_pair(&wood,&gas,&m,UINT32_MAX,&next_wood,&next_gas,&moved,err,sizeof(err)));
    ASSERT_TRUE(moved>0&&moved<wood.bound_water_fp);
    ASSERT_EQ(next_wood.bound_water_fp+moved,wood.bound_water_fp);
    ASSERT_TRUE(fr_reactive_state_valid(&next_wood,err,sizeof(err)));
    fr_reactive_state_t saved_wood=next_wood;fr_gas_state_t saved_gas=next_gas;uint32_t saved_moved=moved;
    gas.species[FR_GAS_OXYGEN]=-1;
    ASSERT_TRUE(!fr_thermal_evaporate_pair(&wood,&gas,&m,1000,&next_wood,&next_gas,&moved,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&next_wood,&saved_wood,sizeof(saved_wood))==0);
    ASSERT_TRUE(memcmp(&next_gas,&saved_gas,sizeof(saved_gas))==0);ASSERT_EQ(moved,saved_moved);
    PASS();
}
int main(void) {
    RUN(test_bound_water_enters_vapor_with_finite_mass_and_total_energy);
    RUN(test_overdraw_pause_and_invalid_gas_leave_inventories_safe);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
