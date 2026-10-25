#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/gas.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
/* Dimensionless reference coefficients, not a selected production sound speed. */
static fr_gas_model_t model(void) {
    fr_gas_model_t m={.kinetic_scale=1};
    double molecular[5]={32,28,44,18,0};
    for(uint32_t i=0;i<5;i++) {m.gas_constant[i]=molecular[i]?28./molecular[i]:0.;m.heat_capacity_cv[i]=molecular[i]?2.5*m.gas_constant[i]:1.;}
    return m;
}
static fr_gas_state_t air(fr_gas_model_t m,double mass,double temperature,double velocity) {
    fr_gas_state_t s={0};double oxygen=.21*32./(.21*32.+.79*28.);
    s.species[FR_GAS_OXYGEN]=mass*oxygen;s.species[FR_GAS_NITROGEN]=mass*(1.-oxygen);
    for(uint32_t i=0;i<5;i++) s.energy+=s.species[i]*m.heat_capacity_cv[i]*temperature;
    s.momentum[0]=mass*velocity;s.energy+=.5*m.kinetic_scale*mass*velocity*velocity;return s;
}
static void test_sealed_heating_stores_energy_and_raises_pressure_without_losing_oxygen(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s=air(m,1,293.15,0),out;fr_gas_thermo_t a,b;
    ASSERT_TRUE(fr_gas_thermodynamics(&m,&s,1,&a,err,sizeof(err)));ASSERT_TRUE(fabs(a.oxygen_mole_fraction-.21)<1e-12);
    double oxygen=s.species[FR_GAS_OXYGEN],before=s.energy;s.energy*=350./293.15;
    ASSERT_TRUE(fr_gas_thermodynamics(&m,&s,1,&b,err,sizeof(err)));
    ASSERT_TRUE(fabs(b.temperature_K-350)<1e-10&&fabs(b.pressure/a.pressure-350./293.15)<1e-12);
    double volume=1,dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,&s,&volume,1,1,1,.4,&dt,err,sizeof(err)));
    for(uint32_t i=0;i<20;i++) {ASSERT_TRUE(fr_gas_reference_step(&m,&s,&volume,1,1,1,dt,FR_GAS_FLUX_HLLC,&out,err,sizeof(err)));s=out;}
    ASSERT_EQ(s.species[FR_GAS_OXYGEN],oxygen);ASSERT_TRUE(fabs(s.energy-before*350./293.15)<1e-10);PASS();
}
static void test_reversible_compression_and_expansion_close_mass_energy_and_boundary_work(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s=air(m,1,293.15,0),compressed,expanded;double work=0,returned=0;
    ASSERT_TRUE(fr_gas_reference_compress(&m,&s,1,.5,&compressed,&work,err,sizeof(err)));
    fr_gas_thermo_t a,b;ASSERT_TRUE(fr_gas_thermodynamics(&m,&s,1,&a,err,sizeof(err))&&fr_gas_thermodynamics(&m,&compressed,.5,&b,err,sizeof(err)));
    ASSERT_TRUE(fabs(b.temperature_K/a.temperature_K-pow(2.,.4))<1e-10);
    ASSERT_TRUE(fabs(b.pressure/a.pressure-pow(2.,1.4))<1e-10);ASSERT_TRUE(work>0&&fabs(compressed.energy-s.energy-work)<1e-10);
    ASSERT_TRUE(fr_gas_reference_compress(&m,&compressed,.5,1,&expanded,&returned,err,sizeof(err)));
    ASSERT_TRUE(fabs(expanded.energy-s.energy)<1e-10&&fabs(work+returned)<1e-10);
    ASSERT_EQ(memcmp(s.species,expanded.species,sizeof(s.species)),0);PASS();
}
static void test_finite_colored_smoke_dilutes_oxygen_without_consuming_it(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t target=air(m,1,293.15,0),source={0},left,mixed;
    source.species[FR_GAS_EXHAUST]=2;source.energy=2*m.heat_capacity_cv[FR_GAS_EXHAUST]*330;source.pigment[0]=2;
    double oxygen=target.species[FR_GAS_OXYGEN],energy=target.energy+source.energy;
    ASSERT_TRUE(fr_gas_admit(&m,&source,&target,.25,&left,&mixed,err,sizeof(err)));
    ASSERT_EQ(left.species[FR_GAS_EXHAUST],1.5);ASSERT_EQ(mixed.species[FR_GAS_EXHAUST],.5);
    ASSERT_EQ(mixed.species[FR_GAS_OXYGEN],oxygen);ASSERT_TRUE(fabs(left.energy+mixed.energy-energy)<1e-10);
    fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,&mixed,1,&t,err,sizeof(err)));ASSERT_TRUE(t.oxygen_mole_fraction<.21);
    source=left;source.pigment[0]=0;source.pigment[2]=1.5;
    ASSERT_TRUE(fr_gas_admit(&m,&source,&mixed,1,&left,&target,err,sizeof(err)));
    ASSERT_EQ(target.pigment[0]/target.species[FR_GAS_EXHAUST],.25);ASSERT_EQ(target.pigment[2]/target.species[FR_GAS_EXHAUST],.75);
    ASSERT_EQ(left.energy,0.);for(uint32_t i=0;i<5;i++) ASSERT_EQ(left.species[i],0.);PASS();
}
static void test_stationary_pressure_balanced_contacts_do_not_diffuse_pigment_at_sound_speed(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t input[8],out[8];double volume[8];
    for(uint32_t i=0;i<8;i++) {
        double mass=i<4?1:2,temperature=i<4?300:150;
        input[i]=air(m,mass,temperature,0);volume[i]=1;
        input[i].species[FR_GAS_EXHAUST]=mass*.01;
        input[i].energy+=mass*.01*m.heat_capacity_cv[FR_GAS_EXHAUST]*temperature;
        input[i].pigment[i<4?0:2]=mass*.01;
    }
    double dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,input,volume,8,1,1,.4,&dt,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_reference_step(&m,input,volume,8,1,1,dt,FR_GAS_FLUX_HLLC,out,err,sizeof(err)));
    for(uint32_t i=0;i<8;i++) {
        ASSERT_TRUE(fabs(out[i].energy-input[i].energy)<1e-10);
        ASSERT_TRUE(fabs(out[i].species[0]-input[i].species[0])<1e-12&&fabs(out[i].momentum[0])<1e-12);
        for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(out[i].pigment[k]-input[i].pigment[k])<1e-12);
    }
    PASS();
}
static void test_two_dimensional_wall_and_open_vent_preserve_closed_inventory(void) {
    char err[256]={0};fr_gas_model_t m=model();enum {W=16,H=12,N=W*H};
    fr_gas_state_t cells[N],out[N];double volume[N],before[5]={0},energy=0,red=0;
    for(uint32_t y=0;y<H;y++) for(uint32_t x=0;x<W;x++) {
        uint32_t i=y*W+x;volume[i]=x==7?0:1;cells[i]=(fr_gas_state_t){0};if(x==7) continue;
        cells[i]=air(m,x<7?1:.125,x<7?300:240,0);
        cells[i].species[FR_GAS_EXHAUST]=.01;cells[i].energy+=.01*m.heat_capacity_cv[FR_GAS_EXHAUST]*300;
        cells[i].pigment[x<7?0:2]=.01;
        for(uint32_t k=0;k<5;k++) before[k]+=cells[i].species[k];
        energy+=cells[i].energy;red+=cells[i].pigment[0];
    }
    for(uint32_t stage=0;stage<2;stage++) {
        if(stage==1) volume[6*W+7]=1;
        for(uint32_t step=0;step<100;step++) {
            double dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,cells,volume,W,H,1,.4,&dt,err,sizeof(err)));
            ASSERT_TRUE(fr_gas_reference_step(&m,cells,volume,W,H,1,dt,FR_GAS_FLUX_HLLC,out,err,sizeof(err)));memcpy(cells,out,sizeof(cells));
        }
        double after[5]={0},heat=0,color=0,right_red=0;
        for(uint32_t y=0;y<H;y++) for(uint32_t x=0;x<W;x++) {
            uint32_t i=y*W+x;for(uint32_t k=0;k<5;k++) after[k]+=cells[i].species[k];
            heat+=cells[i].energy;color+=cells[i].pigment[0];if(x>7) right_red+=cells[i].pigment[0];
        }
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(after[k]-before[k])<1e-9);
        ASSERT_TRUE(fabs(heat-energy)<1e-7&&fabs(color-red)<1e-10);
        ASSERT_TRUE(stage==0?right_red==0:right_red>1e-6);
    }
    PASS();
}
/* Independent self-similar Sod solution from the ideal-gas wave curves;
 * this does not call either finite-volume flux under test. */
static void exact_sod(double xi,double *rho,double *velocity,double *pressure) {
    const double gamma=1.4,cl=sqrt(1.4),cr=sqrt(1.4*.1/.125);double lo=.1,hi=1.;
    for(uint32_t k=0;k<100;k++) {
        double p=.5*(lo+hi),a=2*cl/(gamma-1)*(pow(p,(gamma-1)/(2*gamma))-1);
        double b=(p-.1)*sqrt((2/((gamma+1)*.125))/(p+(gamma-1)/(gamma+1)*.1));
        if(a+b>0) hi=p;else lo=p;
    }
    double p=.5*(lo+hi),u=-2*cl/(gamma-1)*(pow(p,(gamma-1)/(2*gamma))-1);
    double left=pow(p,1/gamma),beta=(gamma-1)/(gamma+1),ratio=p/.1;
    double right=.125*(ratio+beta)/(beta*ratio+1),tail=u-sqrt(gamma*p/left);
    double shock=cr*sqrt((gamma+1)/(2*gamma)*ratio+(gamma-1)/(2*gamma));
    if(xi<-cl) {*rho=1;*velocity=0;*pressure=1;}
    else if(xi<tail) {
        double sound=2/(gamma+1)*(cl-(gamma-1)*xi/2);
        *velocity=2/(gamma+1)*(cl+xi);*rho=pow(sound/cl,2/(gamma-1));*pressure=pow(sound/cl,2*gamma/(gamma-1));
    } else if(xi<u) {*rho=left;*velocity=u;*pressure=p;}
    else if(xi<shock) {*rho=right;*velocity=u;*pressure=p;}
    else {*rho=.125;*velocity=0;*pressure=.1;}
}
static void test_both_fluxes_converge_to_an_independent_exact_pressure_release(void) {
    char err[256]={0};fr_gas_model_t m=model();
    for(uint32_t solver=0;solver<2;solver++) {
        double coarse=0;
        for(uint32_t n=128;n<=256;n*=2) {
            double dx=1./n,*volume=malloc(n*sizeof(*volume));fr_gas_state_t *cells=calloc(n,sizeof(*cells)),*out=calloc(n,sizeof(*out));ASSERT_TRUE(volume&&cells&&out);
            for(uint32_t i=0;i<n;i++) {
                volume[i]=1;cells[i].species[FR_GAS_NITROGEN]=(i<n/2?1:.125)*dx;
                cells[i].energy=(i<n/2?1:.1)/.4*dx;
            }
            double elapsed=0;
            while(elapsed<.15) {
                double dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,cells,volume,n,1,dx,.4,&dt,err,sizeof(err)));dt=fmin(dt,.15-elapsed);
                ASSERT_TRUE(fr_gas_reference_step(&m,cells,volume,n,1,dx,dt,solver,out,err,sizeof(err)));memcpy(cells,out,n*sizeof(*cells));elapsed+=dt;
            }
            double density_error=0,pressure_error=0,velocity_error=0;
            for(uint32_t i=0;i<n;i++) {
                fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,cells+i,dx,&t,err,sizeof(err)));
                double rho=0,u=0,p=0;exact_sod(((i+.5)*dx-.5)/.15,&rho,&u,&p);
                density_error+=fabs(t.mass/dx-rho)/n;pressure_error+=fabs(t.pressure-p)/n;
                velocity_error+=fabs(cells[i].momentum[0]/t.mass-u)/n;
            }
            printf("Sod solver%u n%u L1 density%.9g pressure%.9g velocity%.9g\n",solver,n,density_error,pressure_error,velocity_error);
            ASSERT_TRUE(density_error<.025&&pressure_error<.025&&velocity_error<.05);
            if(n==128) coarse=density_error;else ASSERT_TRUE(density_error<coarse);
            free(volume);free(cells);free(out);
        }
    }
    PASS();
}
static void test_closed_pressure_release_conserves_species_energy_and_pigment_for_both_solvers(void) {
    char err[256]={0};fr_gas_model_t m=model();
    for(uint32_t solver=0;solver<2;solver++) {
        fr_gas_state_t cells[32],out[32];double volume[32],before[5]={0},energy=0,pigment=0;
        for(uint32_t i=0;i<32;i++) {
            cells[i]=air(m,i<16?1:.125,i<16?300:240,0);volume[i]=1;
            cells[i].species[FR_GAS_EXHAUST]=.01;cells[i].energy+=.01*m.heat_capacity_cv[FR_GAS_EXHAUST]*300;
            cells[i].pigment[0]=i<16?.01:0;cells[i].pigment[2]=i<16?0:.01;
            for(uint32_t k=0;k<5;k++) before[k]+=cells[i].species[k];
            energy+=cells[i].energy;pigment+=cells[i].pigment[0];
        }
        for(uint32_t step=0;step<100;step++) {
            double dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,cells,volume,32,1,1,.4,&dt,err,sizeof(err)));
            ASSERT_TRUE(fr_gas_reference_step(&m,cells,volume,32,1,1,dt,solver,out,err,sizeof(err)));memcpy(cells,out,sizeof(cells));
        }
        double after[5]={0},heat=0,color=0;bool moved=false;
        for(uint32_t i=0;i<32;i++) {
            fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,cells+i,1,&t,err,sizeof(err))&&t.temperature_K>0);
            for(uint32_t k=0;k<5;k++) after[k]+=cells[i].species[k];
            heat+=cells[i].energy;color+=cells[i].pigment[0];moved|=fabs(cells[i].momentum[0])>1e-6;
        }
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(after[k]-before[k])<1e-10);
        ASSERT_TRUE(moved&&fabs(heat-energy)<1e-8&&fabs(color-pigment)<1e-10);
    }
    PASS();
}
static void test_cfl_violation_and_invalid_volume_or_admission_are_transactional(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s=air(m,1,300,20),out,saved,other;double volume=1,dt=0;
    memset(&out,0x5a,sizeof(out));saved=out;other=out;
    ASSERT_TRUE(fr_gas_reference_max_dt(&m,&s,&volume,1,1,1,.4,&dt,err,sizeof(err))&&dt>0);
    ASSERT_TRUE(!fr_gas_reference_step(&m,&s,&volume,1,1,1,dt*2,FR_GAS_FLUX_HLLC,&out,err,sizeof(err)));
    ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);
    ASSERT_TRUE(!fr_gas_admit(&m,&s,&s,1.01,&out,&other,err,sizeof(err)));ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);
    fr_gas_thermo_t t={.mass=91},copy=t;
    ASSERT_TRUE(!fr_gas_thermodynamics(&m,&s,0,&t,err,sizeof(err)));ASSERT_EQ(memcmp(&t,&copy,sizeof(t)),0);
    ASSERT_TRUE(fr_gas_reference_step(&m,&s,&volume,1,1,1,0,FR_GAS_FLUX_HLLC,&out,err,sizeof(err)));ASSERT_EQ(memcmp(&out,&s,sizeof(s)),0);PASS();
}
static void test_buoyancy_uses_temperature_and_composition_and_records_external_work(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t ambient=air(m,1,300,0),out;
    double reference=0,work=0;
    for(uint32_t k=0;k<5;k++) reference+=ambient.species[k]*m.gas_constant[k]*300;
    ASSERT_TRUE(fr_gas_reference_buoyancy(&m,&ambient,1,reference,900,.01,&out,&work,err,sizeof(err)));
    ASSERT_EQ(memcmp(&ambient,&out,sizeof(out)),0);ASSERT_EQ(work,0.);
    fr_gas_state_t hot=air(m,.5,600,0);hot.momentum[1]=2.;hot.energy+=.5*m.kinetic_scale*4./.5;
    ASSERT_TRUE(fr_gas_reference_buoyancy(&m,&hot,1,reference,900,.01,&out,&work,err,sizeof(err)));
    ASSERT_TRUE(fabs(out.momentum[1]+2.5)<1e-12);
    ASSERT_TRUE(fabs(out.energy-hot.energy-work)<1e-12&&fabs(work-2.25)<1e-12);
    ASSERT_EQ(memcmp(out.species,hot.species,sizeof(out.species)),0);
    fr_gas_state_t heavy={0};heavy.species[FR_GAS_EXHAUST]=1.;heavy.energy=m.heat_capacity_cv[FR_GAS_EXHAUST]*300;
    ASSERT_TRUE(fr_gas_reference_buoyancy(&m,&heavy,1,reference,900,.01,&out,&work,err,sizeof(err)));
    ASSERT_TRUE(out.momentum[1]>0&&work>0);
    fr_gas_state_t saved=out;double previous=work;
    ASSERT_TRUE(!fr_gas_reference_buoyancy(&m,&hot,1,0,900,.01,&out,&work,err,sizeof(err)));
    ASSERT_EQ(memcmp(&saved,&out,sizeof(out)),0);ASSERT_EQ(work,previous);
    ASSERT_TRUE(fr_gas_reference_buoyancy(&m,&hot,1,reference,900,0,&out,&work,err,sizeof(err)));
    ASSERT_EQ(memcmp(&hot,&out,sizeof(out)),0);ASSERT_EQ(work,0.);PASS();
}
int main(void) {
    RUN(test_sealed_heating_stores_energy_and_raises_pressure_without_losing_oxygen);
    RUN(test_reversible_compression_and_expansion_close_mass_energy_and_boundary_work);
    RUN(test_finite_colored_smoke_dilutes_oxygen_without_consuming_it);
    RUN(test_stationary_pressure_balanced_contacts_do_not_diffuse_pigment_at_sound_speed);
    RUN(test_closed_pressure_release_conserves_species_energy_and_pigment_for_both_solvers);
    RUN(test_cfl_violation_and_invalid_volume_or_admission_are_transactional);
    RUN(test_two_dimensional_wall_and_open_vent_preserve_closed_inventory);
    RUN(test_both_fluxes_converge_to_an_independent_exact_pressure_release);
    RUN(test_buoyancy_uses_temperature_and_composition_and_records_external_work);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
