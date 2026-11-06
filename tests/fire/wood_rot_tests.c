#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/reactive_state.h"
#include "ferrum/fire/material_profile.h"
#include "dungeoncraft/combustion_parameters.h"

static int g_pass, g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define PASS() g_pass++

static void test_partial_immersion_accumulates_one_month_of_rot(void) {
    char err[256] = {0};
    fr_reactive_state_t wood, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 16384u,
                                          293.15f, 9100u, &wood, err,
                                          sizeof(err)));
    uint64_t film_before = wood.fuel_mass_fp;
    /* One simulated month (~2.628e6 s) fully inside the rot window spends
     * exactly one exposure stage: fuel converts and the accumulator wraps. */
    ASSERT_TRUE(fr_wood_rot_reference(&wood, 1.0f, 2628000.0, &out, err,
                                      sizeof(err)));
    ASSERT_TRUE(out.rot_exposure >= 0.f && out.rot_exposure < 1.0f);
    ASSERT_TRUE(out.fuel_mass_fp < film_before);
    ASSERT_TRUE(out.inert_mass_fp > wood.inert_mass_fp);
    /* Mass conserving across the conversion. */
    ASSERT_TRUE(fr_reactive_state_mass_fp(&out) ==
                fr_reactive_state_mass_fp(&wood));
    PASS();
}
static void test_dry_wood_never_rots(void) {
    char err[256] = {0};
    fr_reactive_state_t dry, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 0u,
                                          293.15f, 9101u, &dry, err,
                                          sizeof(err)));
    ASSERT_TRUE(fr_wood_rot_reference(&dry, 1.0f, 2628000.0, &out, err,
                                      sizeof(err)));
    ASSERT_EQ(out.rot_exposure, 0.f);
    ASSERT_EQ(out.fuel_mass_fp, dry.fuel_mass_fp);
    PASS();
}
static void test_fully_saturated_wood_pauses_rot(void) {
    char err[256] = {0};
    const fr_reactive_profile_t *wood = fr_reactive_profile(DC_MATERIAL_WOOD);
    uint32_t water = (uint32_t)(65536.0 * wood->moisture_capacity);
    fr_reactive_state_t soaked, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, water,
                                          293.15f, 9102u, &soaked, err,
                                          sizeof(err)));
    ASSERT_TRUE(fr_wood_rot_reference(&soaked, 1.0f, 2628000.0, &out, err,
                                      sizeof(err)));
    ASSERT_EQ(out.rot_exposure, 0.f);
    ASSERT_EQ(out.fuel_mass_fp, soaked.fuel_mass_fp);
    PASS();
}
static void test_rot_convert_fuel_to_inert_mass_conserving(void) {
    char err[256] = {0};
    fr_reactive_state_t wood, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 8192u,
                                          293.15f, 9103u, &wood, err,
                                          sizeof(err)));
    ASSERT_TRUE(fr_wood_rot_reference(&wood, 0.5f, 5256000.0, &out, err,
                                      sizeof(err)));
    ASSERT_TRUE(out.fuel_mass_fp < wood.fuel_mass_fp);
    ASSERT_TRUE(out.inert_mass_fp > wood.inert_mass_fp);
    ASSERT_TRUE(fr_reactive_state_mass_fp(&out) ==
                fr_reactive_state_mass_fp(&wood));
    PASS();
}
static void test_rot_accumulates_gradually_across_calls(void) {
    char err[256] = {0};
    fr_reactive_state_t wood, part = {0};
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 16384u,
                                          293.15f, 9104u, &wood, err,
                                          sizeof(err)));
    part = wood;
    for (uint32_t month = 0u; month < 12u; month++) {
        fr_reactive_state_t next;
        ASSERT_TRUE(fr_wood_rot_reference(&part, 1.0f, 2628000.0, &next, err,
                                          sizeof(err)));
        part = next;
    }
    /* Twelve months of exposure: several rot stages spent. */
    ASSERT_TRUE(part.rot_exposure < 1.0f);
    ASSERT_TRUE(part.fuel_mass_fp < wood.fuel_mass_fp);
    ASSERT_TRUE(fr_reactive_state_mass_fp(&part) ==
                fr_reactive_state_mass_fp(&wood));
    PASS();
}
static void test_invalid_rot_inputs_are_rejected(void) {
    char err[256] = {0};
    fr_reactive_state_t wood, out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 65536u, 0u,
                                          293.15f, 9105u, &wood, err,
                                          sizeof(err)));
    ASSERT_TRUE(!fr_wood_rot_reference(&wood, 1.0f, -1.0, &out, err,
                                       sizeof(err)));
    ASSERT_TRUE(!fr_wood_rot_reference(&wood, -0.5f, 1.0, &out, err,
                                       sizeof(err)));
    ASSERT_TRUE(!fr_wood_rot_reference(&wood, 1.0f, 0.0 / 0.0, &out, err,
                                       sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_valid(&wood, err, sizeof(err)));
    PASS();
}
int main(void) {
    printf("=== Wood Rot Tests ===\n\n");
    RUN(test_partial_immersion_accumulates_one_month_of_rot);
    RUN(test_dry_wood_never_rots);
    RUN(test_fully_saturated_wood_pauses_rot);
    RUN(test_rot_convert_fuel_to_inert_mass_conserving);
    RUN(test_rot_accumulates_gradually_across_calls);
    RUN(test_invalid_rot_inputs_are_rejected);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
