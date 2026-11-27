#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/gas_gpu_state.h"

static int g_pass, g_fail;
static VkResult queue_status;
static unsigned queue_waits;
static char err[128];

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(condition) do { if (!(condition)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, err); \
    g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() g_pass++

VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueWaitIdle(VkQueue queue) {
    (void)queue;
    queue_waits++;
    return queue_status;
}

static dc_gpu_t gpu;
static struct fr_gas_gpu gas;
static uint32_t pages[2];
static gas_packet_t packets[2u * DC_CHUNK_CELLS];

enum {PROBE_X = 7, PROBE_Y = 12};

static gas_packet_t *setup(void) {
    memset(packets, 0, sizeof(packets));
    queue_status = VK_SUCCESS;
    queue_waits = 0u;
    err[0] = '\0';
    pages[0] = 2u;
    pages[1] = 1u;
    gas = (struct fr_gas_gpu){.direct_publish = true,
        .model = {.gas_constant = {.5, 1., .25, .25, 0.},
                  .heat_capacity_cv = {2., 2., 2., 2., 2.}, .kinetic_scale = .5}};
    gas.state[1].mapped = packets;
    gpu = (dc_gpu_t){.width = 2u * DC_CHUNK_SIDE, .height = DC_CHUNK_SIDE,
        .page_width = 2u, .page_height = 1u, .page_mapped = pages, .gas = &gas};
    gas_packet_t *packet = &packets[DC_CHUNK_CELLS + PROBE_Y * DC_CHUNK_SIDE + PROBE_X];
    packet->words[FR_GAS_OXYGEN] = 1.f;
    packet->words[FR_GAS_NITROGEN] = 2.f;
    packet->words[5] = 1800.75f;
    packet->words[6] = 3.f;
    packet->words[11] = .000001f;
    packet->words[12] = .25f;
    packet->words[15] = -.125f;
    return packet;
}

static void test_mapped_probe_uses_live_model_void_and_compensated_stock(void) {
    setup();
    fr_gas_gpu_probe_t sample;
    ASSERT_TRUE(fr_gas_gpu_probe_cell(&gpu, PROBE_X, PROBE_Y, &sample, err, sizeof(err)));
    ASSERT_EQ(queue_waits, 1u);
    ASSERT_EQ(sample.volume, .25);
    ASSERT_EQ(sample.thermo.mass, 3.);
    ASSERT_EQ(sample.heat_capacity, 6.);
    ASSERT_EQ(sample.work, -.125);
    ASSERT_TRUE(fabs(sample.thermo.oxygen_mole_fraction - .2) < 1e-12);
    ASSERT_TRUE(fabs(sample.internal_energy - 1800.000001) < 1e-10);
    ASSERT_TRUE(fabs(sample.thermo.temperature_K - 1800.000001 / 6.) < 1e-9);
    ASSERT_TRUE(fabs(sample.thermo.pressure - sample.thermo.temperature_K * 20.) < 1e-10);
    fr_gas_state_t chunk[DC_CHUNK_CELLS];
    ASSERT_TRUE(fr_gas_gpu_read_chunk(&gpu, 1u, chunk, DC_CHUNK_CELLS, err, sizeof(err)));
    ASSERT_EQ(queue_waits, 2u);
    ASSERT_TRUE(fabs(chunk[PROBE_Y * DC_CHUNK_SIDE + PROBE_X].energy - 1800.750001) < 1e-10);
    PASS();
}

static void test_unresident_and_out_of_bounds_probes_keep_destination(void) {
    setup();
    fr_gas_gpu_probe_t sample = {.volume = .5, .thermo = {.temperature_K = 310.}};
    ASSERT_TRUE(!fr_gas_gpu_probe_cell(&gpu, gpu.width, PROBE_Y, &sample, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "coordinates") != NULL);
    pages[0] = 0u;
    ASSERT_TRUE(!fr_gas_gpu_probe_cell(&gpu, PROBE_X, PROBE_Y, &sample, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "resident") != NULL);
    ASSERT_EQ(sample.volume, .5);
    ASSERT_EQ(sample.thermo.temperature_K, 310.);
    ASSERT_EQ(queue_waits, 0u);
    PASS();
}

static void test_unavailable_queue_and_gas_void_keep_destination(void) {
    gas_packet_t *packet = setup();
    fr_gas_gpu_probe_t sample = {.volume = .5, .internal_energy = 17., .heat_capacity = .25};
    queue_status = VK_ERROR_DEVICE_LOST;
    ASSERT_TRUE(!fr_gas_gpu_probe_cell(&gpu, PROBE_X, PROBE_Y, &sample, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "snapshot") != NULL);
    queue_status = VK_SUCCESS;
    packet->words[12] = 0.f;
    ASSERT_TRUE(!fr_gas_gpu_probe_cell(&gpu, PROBE_X, PROBE_Y, &sample, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "void") != NULL);
    *packet = (gas_packet_t){.words[12] = 1.f};
    ASSERT_TRUE(!fr_gas_gpu_probe_cell(&gpu, PROBE_X, PROBE_Y, &sample, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "empty") != NULL);
    ASSERT_EQ(sample.volume, .5);
    ASSERT_EQ(sample.internal_energy, 17.);
    ASSERT_EQ(sample.heat_capacity, .25);
    PASS();
}

int main(void) {
    RUN(test_mapped_probe_uses_live_model_void_and_compensated_stock);
    RUN(test_unresident_and_out_of_bounds_probes_keep_destination);
    RUN(test_unavailable_queue_and_gas_void_keep_destination);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
