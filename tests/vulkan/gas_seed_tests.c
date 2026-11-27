#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/gas_gpu_state.h"

static int g_pass, g_fail;
static unsigned g_waits;
static char g_error[160];

#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %d: %s [%s]\n", __LINE__, #expr, g_error); \
    ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static struct {
    dc_gpu_t gpu;
    struct fr_gas_gpu gas;
    gas_packet_t packets[DC_CHUNK_CELLS];
    gas_packet_t before[DC_CHUNK_CELLS];
    fr_gas_checkpoint_chunk_t saved;
    uint32_t parameters[FR_GAS_MODEL_WORDS + 1u];
    uint32_t gates[DC_CHUNK_CELLS];
    fr_gas_state_t input[DC_CHUNK_CELLS];
    fr_gas_state_t output[DC_CHUNK_CELLS];
    double volumes[DC_CHUNK_CELLS];
} fixture;

VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueWaitIdle(VkQueue queue) {
    (void)queue;
    ++g_waits;
    return VK_SUCCESS;
}

static void reset_fixture(void) {
    memset(&fixture, 0, sizeof(fixture));
    g_waits = 0;
    g_error[0] = '\0';
    fixture.gpu.gas = &fixture.gas;
    fixture.gas.state[0].mapped = fixture.packets;
    fixture.gas.parameters.mapped = fixture.parameters;
    fixture.gas.gates.mapped = fixture.gates;
    fixture.gas.model = (fr_gas_model_t){.kinetic_scale = 1.,
        .gas_constant = {1., 1., 1., 1., 0.}, .heat_capacity_cv = {1., 1., 1., 1., 1.}};
    for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) fixture.volumes[i] = 1.;
}

static bool seed(void) {
    return fr_gas_gpu_seed_chunk(&fixture.gpu, 0, fixture.input, fixture.volumes,
                                  DC_CHUNK_CELLS, g_error, sizeof(g_error));
}

static void test_equal_pigment_uses_highest_float_within_compensated_carriers(void) {
    reset_fixture();
    fixture.input[0] = (fr_gas_state_t){.species = {[FR_GAS_EXHAUST] = .1},
                                      .energy = 30., .pigment = {.1, .08, .09}};
    fixture.input[1] = (fr_gas_state_t){.species = {[FR_GAS_EXHAUST] = .04, [FR_GAS_SOOT] = .06},
                                      .energy = 30., .pigment = {.1, 0., .1}};
    ASSERT_TRUE(seed());
    ASSERT_INT_EQ(g_waits, 1u);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(&fixture.gpu, 0, fixture.output, DC_CHUNK_CELLS,
                                      g_error, sizeof(g_error)));
    const float bounded = nextafterf(.1f, 0.f);
    for (unsigned i = 0; i < 2u; ++i) {
        const fr_gas_state_t *out = &fixture.output[i];
        double carrier = out->species[FR_GAS_EXHAUST] + out->species[FR_GAS_SOOT];
        ASSERT_TRUE(fabs(carrier - .1) < 1e-15);
        ASSERT_EQ(out->pigment[0], (double)bounded);
        ASSERT_TRUE(out->pigment[0] <= carrier && (double).1f > carrier);
    }
    ASSERT_EQ(fixture.output[0].pigment[1], (double).08f);
    ASSERT_EQ(fixture.output[0].pigment[2], (double).09f);
    ASSERT_EQ(fixture.output[1].pigment[2], (double)bounded);
    PASS();
}

static void test_over_capacity_input_cannot_publish_even_when_it_rounds_the_same(void) {
    reset_fixture();
    fixture.input[0] = (fr_gas_state_t){.species = {[FR_GAS_EXHAUST] = .1},
                                      .energy = 30., .pigment = {.08}};
    ASSERT_TRUE(seed());
    memcpy(fixture.before, fixture.packets, sizeof(fixture.before));
    fixture.input[0].energy = 40.;
    fr_gas_state_t *invalid = &fixture.input[DC_CHUNK_CELLS - 1u];
    *invalid = (fr_gas_state_t){.species = {[FR_GAS_EXHAUST] = .1},
                               .energy = 30., .pigment = {.1 + 1e-10}};
    ASSERT_EQ((float)invalid->pigment[0], .1f);
    ASSERT_TRUE(!seed());
    ASSERT_TRUE(strstr(g_error, "Gas pigment has no admitted carrier"));
    ASSERT_INT_EQ(g_waits, 1u);
    ASSERT_INT_EQ(memcmp(fixture.before, fixture.packets, sizeof(fixture.before)), 0);
    PASS();
}

static void test_checkpoint_round_trip_preserves_bounded_pigment_and_rejects_corruption(void) {
    reset_fixture();
    fixture.input[0] = (fr_gas_state_t){.species = {[FR_GAS_EXHAUST] = .1},
                                      .energy = 30., .pigment = {.1}};
    ASSERT_TRUE(seed());
    memcpy(fixture.before, fixture.packets, sizeof(fixture.before));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(&fixture.gpu, 0, &fixture.saved,
                                             g_error, sizeof(g_error)));
    memset(fixture.packets, 0, sizeof(fixture.packets));
    ASSERT_TRUE(fr_gas_gpu_import_checkpoint(&fixture.gpu, 0, &fixture.saved,
                                             g_error, sizeof(g_error)));
    ASSERT_INT_EQ(memcmp(fixture.packets, fixture.before, sizeof(fixture.before)), 0);

    fixture.saved.cells[0][8] = .1f;
    ASSERT_TRUE(!fr_gas_gpu_import_checkpoint(&fixture.gpu, 0, &fixture.saved,
                                              g_error, sizeof(g_error)));
    ASSERT_TRUE(strstr(g_error, "Gas pigment has no admitted carrier"));
    ASSERT_INT_EQ(g_waits, 3u);
    ASSERT_INT_EQ(memcmp(fixture.packets, fixture.before, sizeof(fixture.before)), 0);
    PASS();
}

int main(void) {
    RUN(test_equal_pigment_uses_highest_float_within_compensated_carriers);
    RUN(test_over_capacity_input_cannot_publish_even_when_it_rounds_the_same);
    RUN(test_checkpoint_round_trip_preserves_bounded_pigment_and_rejects_corruption);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
