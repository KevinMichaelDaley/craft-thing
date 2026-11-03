#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/combustion.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/reactive_state.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define PASS() g_pass++

static void test_conductor_receives_declared_energy_exactly_once(void) {
    char err[256] = {0};
    fr_reactive_state_t water, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WATER, 0u, 65536u, 293.15f,
                                          9001u, &water, err, sizeof(err)));
    double before = fr_reactive_state_energy(&water);
    double deposited = -1;
    ASSERT_TRUE(fr_reactive_expose_shock(fr_reactive_profile(DC_MATERIAL_WATER),
                                         &water, 450.0, &out, &deposited, err,
                                         sizeof(err)));
    ASSERT_TRUE(fabs(deposited - 450.0) < 1e-6);
    ASSERT_TRUE(fabs((double)out.pending_shock_energy - 450.0) < 1e-3);
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&out) - before) < 1e-9);
    fr_combustion_products_t products;
    fr_reactive_state_t burned;
    ASSERT_TRUE(fr_combustion_reference(&out, 0.0, .21, 1.0, .01, &burned,
                                        &products, err, sizeof(err)));
    ASSERT_EQ(burned.pending_shock_energy, 0.f);
    ASSERT_TRUE(fr_reactive_state_energy(&burned) > before + 449.0);
    PASS();
}
static void test_insulating_stone_rejects_a_strike_and_stays_cold(void) {
    char err[256] = {0};
    fr_reactive_state_t stone, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE, 65536u, 0u,
                                          293.15f, 9002u, &stone, err,
                                          sizeof(err)));
    double before = fr_reactive_state_energy(&stone), deposited = -1;
    ASSERT_TRUE(fr_reactive_expose_shock(fr_reactive_profile(DC_MATERIAL_STONE),
                                         &stone, 450.0, &out, &deposited, err,
                                         sizeof(err)));
    ASSERT_TRUE(deposited >= 0.0 && deposited <= 0.01);
    ASSERT_TRUE((double)out.pending_shock_energy <= 0.01);
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&out) - before) < 1e-9);
    float temperature;
    ASSERT_TRUE(fr_reactive_state_temperature(&out, &temperature, err,
                                              sizeof(err)));
    ASSERT_TRUE(temperature < 294.f);
    PASS();
}
static void test_wet_wood_conducts_more_strike_energy_than_dry_wood(void) {
    char err[256] = {0};
    const fr_reactive_profile_t *wood = fr_reactive_profile(DC_MATERIAL_WOOD);
    ASSERT_TRUE(wood && wood->moisture_capacity > 0.f);
    fr_reactive_state_t dry, wet, dry_out, wet_out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 0u,
                                          293.15f, 9003u, &dry, err,
                                          sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u,
                                          (uint32_t)(65536.0 *
                                                     wood->moisture_capacity),
                                          293.15f, 9004u, &wet, err,
                                          sizeof(err)));
    double dry_deposited = -1, wet_deposited = -1;
    ASSERT_TRUE(fr_reactive_expose_shock(wood, &dry, 450.0, &dry_out,
                                         &dry_deposited, err, sizeof(err)));
    ASSERT_TRUE(fr_reactive_expose_shock(wood, &wet, 450.0, &wet_out,
                                         &wet_deposited, err, sizeof(err)));
    ASSERT_TRUE(dry_deposited > 0.0);
    ASSERT_TRUE(wet_deposited > dry_deposited * 2.0);
    ASSERT_TRUE(wet_deposited <= 450.0);
    PASS();
}
static void test_strike_energy_splits_and_consumes_once_after_reassembly(void) {
    char err[256] = {0};
    fr_reactive_state_t wood, first, second;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 0u,
                                          293.15f, 9005u, &wood, err,
                                          sizeof(err)));
    double deposited = -1;
    ASSERT_TRUE(fr_reactive_expose_shock(fr_reactive_profile(DC_MATERIAL_WOOD),
                                         &wood, 450.0, &wood, &deposited, err,
                                         sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_split(&wood, 32768u, 11u, 12u, &first,
                                        &second, err, sizeof(err)));
    double share = (double)first.pending_shock_energy +
                   (double)second.pending_shock_energy;
    ASSERT_TRUE(fabs(share - 450.0) < 1e-3);
    fr_combustion_products_t products;
    fr_reactive_state_t burned_first, burned_second;
    ASSERT_TRUE(fr_combustion_reference(&first, 0.0, .21, 1.0, .01,
                                        &burned_first, &products, err,
                                        sizeof(err)));
    ASSERT_TRUE(fr_combustion_reference(&second, 0.0, .21, 1.0, .01,
                                        &burned_second, &products, err,
                                        sizeof(err)));
    ASSERT_EQ(burned_first.pending_shock_energy, 0.f);
    ASSERT_EQ(burned_second.pending_shock_energy, 0.f);
    double total = fr_reactive_state_energy(&burned_first) +
                   fr_reactive_state_energy(&burned_second);
    double unburned = fr_reactive_state_energy(&first) +
                      fr_reactive_state_energy(&second);
    ASSERT_TRUE(total > unburned + 449.0);
    PASS();
}
static void test_invalid_strikes_are_rejected_transactionally(void) {
    char err[256] = {0};
    fr_reactive_state_t wood, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 0u,
                                          293.15f, 9006u, &wood, err,
                                          sizeof(err)));
    double deposited = -1;
    ASSERT_TRUE(!fr_reactive_expose_shock(NULL, &wood, 450.0, &out,
                                          &deposited, err, sizeof(err)));
    ASSERT_TRUE(!fr_reactive_expose_shock(fr_reactive_profile(DC_MATERIAL_WOOD),
                                          &wood, -1.0, &out, &deposited, err,
                                          sizeof(err)));
    ASSERT_TRUE(!fr_reactive_expose_shock(fr_reactive_profile(DC_MATERIAL_WOOD),
                                          &wood, NAN, &out, &deposited, err,
                                          sizeof(err)));
    ASSERT_TRUE(!isfinite(deposited) || deposited == -1.0);
    ASSERT_TRUE(fr_reactive_state_valid(&wood, err, sizeof(err)));
    PASS();
}
int main(void) {
    printf("=== Shock Exposure Tests ===\n\n");
    RUN(test_conductor_receives_declared_energy_exactly_once);
    RUN(test_insulating_stone_rejects_a_strike_and_stays_cold);
    RUN(test_wet_wood_conducts_more_strike_energy_than_dry_wood);
    RUN(test_strike_energy_splits_and_consumes_once_after_reassembly);
    RUN(test_invalid_strikes_are_rejected_transactionally);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
