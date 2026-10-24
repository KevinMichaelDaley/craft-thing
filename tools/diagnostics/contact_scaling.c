/* Contacts-only diagnostic: no lighting, fluid, raster, or solver submissions. */
#define main pixel_fixture_main
#include "../../tests/vulkan/pixel_component_tests.c"
#undef main
#include "../../src/vulkan/contact_shader_contract.h"
static double last_contact_ms;
static uint32_t last_emitted;
static uint32_t last_visited;
static uint32_t last_workgroups;
static uint32_t last_rejected;
static uint32_t last_plain_visited,last_plain_emitted;
static uint64_t last_contact_digest;
static float contact_test_angle;

static int compare_contact_bytes(const void *a,const void *b) {
    return memcmp(a,b,sizeof(dc_gpu_contact_t));
}

bool __real_dc_gpu_load_shader_module(dc_gpu_t *, const char *, VkShaderModule *, char *, uint32_t);
bool __wrap_dc_gpu_load_shader_module(dc_gpu_t *gpu, const char *path, VkShaderModule *module,
                                      char *err, uint32_t cap) {
    const char *candidate = getenv("DC_CONTACT_SHADER");
    if(candidate&&strcmp(candidate,"scalar-reference")==0)
        candidate=gpu->runtime_body_consumers?"build/diagnostics/runtime_contacts_scalar_reference.spv":
            "build/diagnostics/contacts_scalar_reference.spv";
#ifdef DC_CONTACT_TERRAIN_PROFILE
    if (!candidate) candidate=gpu->runtime_body_consumers?
        "build/diagnostics/runtime_contacts_terrain_scan.comp.spv":"build/diagnostics/contacts_terrain_scan.comp.spv";
#endif
    if (candidate && (strcmp(path,"build/shaders/contacts.comp.spv")==0||
        strcmp(path,"build/shaders/contacts_pixel.comp.spv")==0)) {
        if(!dc_gpu_contact_shader_compatible(candidate,DC_GPU_TYPED_CONTACT_ACTIVITY_WORD,err,cap)) return false;
        path = candidate;
    }
    return __real_dc_gpu_load_shader_module(gpu, path, module, err, cap);
}

static bool occupied(unsigned kind, unsigned x, unsigned y, unsigned side) {
    if (kind == 0) return true;
    if (kind == 1) return x == 0 || y == 0 || x + 1 == side || y + 1 == side;
    return x == 0 || y % 4 < 2;
}

static bool fixture_solid(unsigned kind,int x,int y,unsigned side) {
    return x>=0&&y>=0&&(unsigned)x<side&&(unsigned)y<side&&occupied(kind,(unsigned)x,(unsigned)y,side);
}
static bool cached_boundaries_match_fixture(dc_gpu_t *g,unsigned kind,unsigned side) {
    if(!g->runtime_body_consumers) return true;
    const uint32_t *words=g->body_mapped;
    const dc_gpu_body_record_t *body=dc_gpu_body_record(g,0);
    if(!(body->piece_count&DC_GPU_PIXEL_BODY_FLAG)) body=dc_gpu_body_record(g,1);
    uint32_t pointer=body->compound_reserved,tiles=words[pointer+1u];
    uint32_t directory=words[pointer+2u]*words[pointer+3u];
    uint32_t low=dc_gpu_pixel_pool_word_offset(g)+DC_GPU_PIXEL_POOL_HEADER_WORDS;
    uint32_t pairs=g->runtime_pair_capacity?g->runtime_pair_capacity:DC_GPU_BROADPHASE_PAIR_CAPACITY;
    uint32_t cache=dc_gpu_contact_work_word_offset(g)+4u+pairs*DC_CONTACT_PIXEL_GROUPS*2u+
        ((g->width+63u)/64u)*((g->height+63u)/64u);
    for(uint32_t tile=0;tile<tiles;tile++) {
        uint32_t start=pointer+8u+directory+tile*33u,coord=words[start];
        for(uint32_t word=0;word<32;word++) {
            int x=(int)((coord&65535u)*16u+(word%2u)*8u)-(int)words[pointer+4u];
            int y=(int)((coord>>16u)*16u+word/2u)-(int)words[pointer+5u];
            uint32_t expected=0u;
            for(int bit=0;bit<8;bit++) if(fixture_solid(kind,x+bit,y,side)&&
                (!fixture_solid(kind,x+bit-1,y,side)||!fixture_solid(kind,x+bit+1,y,side)||
                 !fixture_solid(kind,x+bit,y-1,side)||!fixture_solid(kind,x+bit,y+1,side)))
                expected|=1u<<(bit*4);
            uint32_t actual=words[cache+start+1u+word-low];
            if(actual!=expected) {
                printf("cached boundary tile%u word%u expected%08x actual%08x\n",tile,word,expected,actual);
                return false;
            }
        }
    }
    printf("verified packed boundary cache %u words\n",tiles*32u);return true;
}

static void measure(unsigned side, unsigned kind, unsigned pair_kind) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(side >= 8 && side <= 96 && kind <= 2 && pair_kind <= 4);
    ASSERT_TRUE(dc_gpu_create(&g, 256, 256, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    unsigned area = 0, boundary = 0, perimeter = 0;
    for (unsigned cy = 0; cy < 4; cy++)
        for (unsigned cx = 0; cx < 4; cx++) {
            *chunk = (dc_chunk_t){.coord = {cx, cy}};
            for (unsigned y = 0; y < 64; y++)
                for (unsigned x = 0; x < 64; x++) {
                    unsigned px = cx * 64 + x, py = cy * 64 + y;
                    if (px < 32 || py < 32 || px >= 32 + side || py >= 32 + side)
                        continue;
                    unsigned sx = px - 32, sy = py - 32;
                    if (!occupied(kind, sx, sy, side)) continue;
                    chunk->cells[y * 64 + x].material = DC_MATERIAL_WOOD;
                    area++;
                    boundary += sx == 0 || sy == 0 || sx + 1 == side || sy + 1 == side ||
                                !occupied(kind, sx - 1, sy, side) || !occupied(kind, sx + 1, sy, side) ||
                                !occupied(kind, sx, sy - 1, side) || !occupied(kind, sx, sy + 1, side);
                    perimeter += (sx == 0 || !occupied(kind, sx - 1, sy, side)) +
                                 (sx + 1 == side || !occupied(kind, sx + 1, sy, side)) +
                                 (sy == 0 || !occupied(kind, sx, sy - 1, side)) +
                                 (sy + 1 == side || !occupied(kind, sx, sy + 1, side));
                }
            unsigned slot = cy * 4 + cx;
            ASSERT_TRUE(dc_gpu_upload_chunk(g, slot, chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(g, cx, cy, slot, err, sizeof(err)));
        }
    free(chunk);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_record_t *bodies = g->body_mapped;
    ASSERT_TRUE(bodies[0].piece_count & DC_GPU_PIXEL_BODY_FLAG);
    bodies[1] = bodies[0];
    bodies[1].body.id = 0x70000001u;
    if (pair_kind < 2) {
        unsigned legacy = pair_kind == 0 ? 0 : 1;
        const uint32_t *words = g->body_mapped;
        unsigned low_x = words[bodies[legacy].compound_reserved + 4];
        unsigned low_y = words[bodies[legacy].compound_reserved + 5];
        bodies[legacy].piece_count = 0;
        bodies[legacy].shape.count = 0;
        bodies[legacy].body.width = side;
        bodies[legacy].body.height = side;
        bodies[legacy].body.x_fp += (int32_t)(low_x + side / 2) * 65536 + 16384;
        bodies[legacy].local_x_fp += (int32_t)(low_x + side / 2) * 65536 + 16384;
        bodies[legacy].body.y_fp += (int32_t)low_y * 65536;
        bodies[legacy].local_y_fp += (int32_t)low_y * 65536;
    } else if (pair_kind == 2) {
        bodies[1].body.x_fp += (int32_t)side * 65536 - 16384;
        bodies[1].local_x_fp += (int32_t)side * 65536 - 16384;
    }
    g->body_count = 2;
    bodies[0].angle=contact_test_angle;bodies[1].angle=-contact_test_angle*.7f;
    if (pair_kind >= 3) {
        dc_chunk_t *terrain = calloc(1, sizeof(*terrain));
        ASSERT_TRUE(terrain);
        terrain->coord = (dc_chunk_coord_t){1, 1};
        for (unsigned i = 0; i < (pair_kind == 4 ? 4096u : 1u); i++)
            terrain->cells[i].material = DC_MATERIAL_STONE;
        ASSERT_TRUE(dc_gpu_upload_chunk(g, 5, terrain, err, sizeof(err)));
        free(terrain);
        bodies[1].body.active = 0;
        g->body_count = 1;
    }
    uint32_t *pairs = dc_gpu_broadphase_data(g);
    unsigned pair_base = DC_GPU_BROADPHASE_HEADER_WORDS +
                         2 * dc_gpu_broadphase_buckets(g);
    pairs[0] = 1; pairs[2] = 0;
    pairs[pair_base] = bodies[0].body.id;
    pairs[pair_base + 1] = bodies[1].body.id;
    for (unsigned i = 2; i < 8; i++) pairs[pair_base + i] = 0;
    if (pair_kind >= 3) {
        pairs[pair_base + 1] = 0;
        pairs[pair_base + 2] = 1;
        pairs[pair_base + 4] = 1;
        pairs[pair_base + 6] = 1;
    }
    double total_ms = 0;
    for (unsigned sample = 0; sample < 10; sample++) {
        g->component_profile = sample == 9;
        ASSERT_EQ(vkResetCommandBuffer(g->command, 0), VK_SUCCESS);
        VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        ASSERT_EQ(vkBeginCommandBuffer(g->command, &begin), VK_SUCCESS);
        vkCmdResetQueryPool(g->command, g->timestamp_pool, 0, 2);
        dc_gpu_runtime_record_body_index(g);
        vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g->timestamp_pool, 0);
        dc_gpu_record_contacts(g);
        vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g->timestamp_pool, 1);
        ASSERT_EQ(vkEndCommandBuffer(g->command), VK_SUCCESS);
        VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1, .pCommandBuffers = &g->command};
        ASSERT_EQ(vkQueueSubmit(g->queue, 1, &submit, VK_NULL_HANDLE), VK_SUCCESS);
        ASSERT_EQ(vkQueueWaitIdle(g->queue), VK_SUCCESS);
        if(sample==8u) {
            last_plain_visited=dc_gpu_contact_data(g)[9];
            last_plain_emitted=dc_gpu_contact_data(g)[0];
        }
        uint64_t times[2];
        ASSERT_EQ(vkGetQueryPoolResults(g->device, g->timestamp_pool, 0, 2, sizeof(times), times,
                                       sizeof(uint64_t), VK_QUERY_RESULT_64_BIT), VK_SUCCESS);
        if (sample > 0 && sample < 9)
            total_ms += (times[1] - times[0]) * g->timestamp_period / 1e6;
    }
    const uint32_t *contacts = dc_gpu_contact_data(g);
    printf("contact scaling side%u shape%u pair%u area%u boundary%u perimeter%u "
           "visited%u boundaryA%u boundaryB%u emitted%u overflow%u mean%.6fms\n",
           side, kind, pair_kind, area, boundary, perimeter, contacts[9], contacts[10], contacts[11],
           contacts[0], contacts[2], total_ms / 8);
    ASSERT_EQ(contacts[2], 0u);
    dc_gpu_contact_t *ordered=malloc((contacts[0]?contacts[0]:1u)*sizeof(*ordered));
    ASSERT_TRUE(ordered);
    memcpy(ordered,contacts+DC_GPU_CONTACT_HEADER_WORDS,(size_t)contacts[0]*sizeof(*ordered));
    qsort(ordered,contacts[0],sizeof(*ordered),compare_contact_bytes);
    last_contact_digest=1469598103934665603ull;
    const unsigned char *bytes=(const unsigned char *)ordered;
    for(size_t i=0;i<(size_t)contacts[0]*sizeof(*ordered);i++)
        last_contact_digest=(last_contact_digest^bytes[i])*1099511628211ull;
    free(ordered);
    printf("canonical contact digest %016llx\n",(unsigned long long)last_contact_digest);
    if (pair_kind >= 3)
        ASSERT_EQ(contacts[0], 0u);
    last_contact_ms = total_ms / 8;
    last_emitted = contacts[0];
    last_visited = contacts[9];
    last_rejected = contacts[5];
    uint32_t work=dc_gpu_pixel_pool_word_offset(g)+DC_GPU_PIXEL_POOL_HEADER_WORDS+2u*dc_gpu_pixel_pool_capacity(g);
    last_workgroups=DC_CONTACT_PIXEL_GROUPS;
    if(g->runtime_body_consumers&&dc_gpu_body_storage_bytes(g)>(VkDeviceSize)work*4u)
        last_workgroups=((const uint32_t *)g->body_mapped)[work];
    printf("pixel contact workgroups %u\n",last_workgroups);
    ASSERT_TRUE(cached_boundaries_match_fixture(g,kind,side));
    dc_gpu_destroy(g);
    PASS();
}

#ifndef DC_CONTACT_SCALING_NO_MAIN
static void test_legacy_pixel_order_does_not_serialize_pixel_query(void) {
    measure(64, 0, 0);
    double legacy_first = last_contact_ms;
    uint32_t emitted = last_emitted;
    measure(64, 0, 1);
    ASSERT_EQ(emitted, last_emitted);
    ASSERT_TRUE(legacy_first <= last_contact_ms * 4 + .1);
    PASS();
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "balance") == 0) {
        RUN(test_legacy_pixel_order_does_not_serialize_pixel_query);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail != 0;
    }
    if (argc != 4) return 2;
    measure((unsigned)strtoul(argv[1], NULL, 10), (unsigned)strtoul(argv[2], NULL, 10),
            (unsigned)strtoul(argv[3], NULL, 10));
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
#endif
