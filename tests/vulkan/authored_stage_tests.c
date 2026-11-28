#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/assets/prop.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "ferrum/structural/authoring.h"
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/runtime_body_gpu.h"
#include "../../src/vulkan/runtime_body_internal.h"

static bool fail_descriptor_pool;
VKAPI_ATTR VkResult VKAPI_CALL __real_vkCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo *,
    const VkAllocationCallbacks *, VkDescriptorPool *);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *info,
    const VkAllocationCallbacks *allocator, VkDescriptorPool *pool) {
    if (fail_descriptor_pool) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    return __real_vkCreateDescriptorPool(device, info, allocator, pool);
}

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() g_pass++

static uint8_t physical[8 * 8 * 8], appearance[8 * 8 * 64];

static fr_authored_request_t request(void) {
    memset(physical, 0, sizeof(physical));
    memset(appearance, 0, sizeof(appearance));
    for (unsigned i = 0; i < 64; i++) {
        float normal = 1;
        memcpy(appearance + i * 64 + 24, &normal, sizeof(normal));
    }
    return (fr_authored_request_t){
        .physical = physical, .appearance = appearance,
        .physical_bytes = sizeof(physical), .appearance_bytes = sizeof(appearance),
        .width = 8, .height = 8, .state_variant_count = 1,
        .asset_uuid = {1}, .transaction_id = 9};
}

static void pixel(unsigned x, unsigned y, unsigned material, unsigned role,
                   unsigned object) {
    unsigned i = (y * 8 + x) * 8;
    physical[i] = (uint8_t)material;
    physical[i + 2] = (uint8_t)role;
    physical[i + 4] = (uint8_t)object;
}

static dc_gpu_t *context(void) {
    char err[256];
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)))
        return NULL;
    return gpu;
}

static void test_mixed_concavities_keep_exact_role_object_connectivity(void) {
    fr_authored_request_t r = request();
    pixel(1,1,1,3,7); pixel(1,2,2,3,7); pixel(1,3,4,3,7); pixel(2,3,1,3,7);
    pixel(4,1,2,3,7); pixel(4,2,1,3,7); pixel(5,2,4,3,7);
    pixel(6,2,1,3,8);
    pixel(2,4,1,1,7);
    pixel(7,7,0,4,7);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    uint32_t labels[64];
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, labels, 64, err, sizeof(err)));
    ASSERT_EQ(d.failures, 0u); ASSERT_EQ(d.rigid_components, 3u);
    ASSERT_EQ(d.rigid_pixels, 8u); ASSERT_EQ(d.terrain_pixels, 1u);
    ASSERT_EQ(d.decoration_pixels, 1u);
    ASSERT_EQ(labels[9], labels[26]);
    ASSERT_TRUE(labels[9] != labels[12]);
    ASSERT_TRUE(labels[21] != labels[22]);
    ASSERT_EQ(labels[34], FR_AUTHORED_INVALID_OUTPUT);
    fr_authored_component_plan_t plans[3];
    uint32_t count = 0;
    ASSERT_TRUE(!fr_gpu_read_authored_components(gpu, plans, 2, &count, err, sizeof(err)));
    ASSERT_EQ(count, 3u);
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu, plans, 3, &count, err, sizeof(err)));
    ASSERT_EQ(count, 3u); ASSERT_EQ(plans[0].source_root, 9u);
    ASSERT_EQ(plans[0].object, 7u); ASSERT_EQ(plans[0].occupied, 4u);
    ASSERT_EQ(plans[0].low_x, 1u); ASSERT_EQ(plans[0].low_y, 1u);
    ASSERT_EQ(plans[0].width, 2u); ASSERT_EQ(plans[0].height, 3u);
    ASSERT_EQ(plans[0].tiles, 1u); ASSERT_EQ(plans[0].geometry_words, 42u);
    ASSERT_EQ(plans[1].source_root, 12u); ASSERT_EQ(plans[2].source_root, 22u);
    ASSERT_EQ(plans[0].body_slot, 0u); ASSERT_EQ(plans[1].body_slot, 1u);
    ASSERT_EQ(plans[2].body_slot, 2u);
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body(gpu, &body, err, sizeof(err)));
    ASSERT_EQ(body.active, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_diagonal_pixels_remain_independent(void) {
    fr_authored_request_t r = request();
    pixel(1,1,2,3,0); pixel(2,2,2,3,0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(d.rigid_components, 2u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_invalid_role_material_and_fabricated_structure_are_diagnosed(void) {
    fr_authored_request_t r = request(); pixel(3,3,5,3,0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    ASSERT_EQ(d.first_invalid_pixel, 27u);
    r = request(); pixel(3,3,1,2,0); physical[27 * 8 + 3] = 1;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    dc_gpu_destroy(gpu); PASS();
}

static void test_each_declared_member_requires_owned_structural_pixels(void) {
    fr_authored_request_t r = request();
    fr_structural_gpu_graph_t graph = {.node_count=3, .member_count=2};
    for (unsigned i=0; i<3; ++i)
        graph.nodes[i]=(fr_structural_gpu_node_t){.id=i+1,.generation=1,.x_fp=(int32_t)i*65536};
    for (unsigned i=0; i<2; ++i)
        graph.members[i]=(fr_structural_gpu_member_t){.id=11+i,.generation=1,.a=i+1,.b=i+2,
            .material=FR_STRUCTURAL_STONE,.depth_fp=65536};
    uint32_t owners[64]={11};
    r.structure=&graph;
    r.member_ownership=(const uint8_t *)owners;
    r.member_ownership_bytes=sizeof(owners);
    pixel(0,0,1,FR_AUTHORED_STRUCTURE,1);
    dc_gpu_t *gpu=context(); ASSERT_TRUE(gpu);
    char err[256]; fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    owners[1]=12; pixel(1,0,1,FR_AUTHORED_STRUCTURE,1);
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
    ASSERT_EQ(d.failures,0u);
    ASSERT_EQ(d.structural_pixels,2u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_structural_pixels_use_registered_mechanical_profiles(void) {
    fr_authored_request_t r=request();
    fr_structural_gpu_graph_t graph={.node_count=2,.member_count=1};
    graph.nodes[0]=(fr_structural_gpu_node_t){.id=1,.generation=1};
    graph.nodes[1]=(fr_structural_gpu_node_t){.id=2,.generation=1,.x_fp=65536};
    graph.members[0]=(fr_structural_gpu_member_t){.id=17,.generation=1,.a=1,.b=2,.depth_fp=65536};
    uint32_t owners[64]={17};
    r.structure=&graph;
    r.member_ownership=(const uint8_t *)owners;
    r.member_ownership_bytes=sizeof(owners);
    dc_gpu_t *gpu=context(); ASSERT_TRUE(gpu);
    char err[256]; fr_authored_stage_diagnostic_t d;
    for (uint32_t material=1; material<=12; ++material) {
        fr_structural_material_t profile=FR_STRUCTURAL_WOOD;
        bool supported=fr_structural_material_map(false,fr_prop_body_material(material),&profile);
        pixel(0,0,material,FR_AUTHORED_STRUCTURE,1);
        for (uint32_t declared=0; declared<FR_STRUCTURAL_MATERIAL_COUNT; ++declared) {
            graph.members[0].material=declared;
            bool accepted=supported && profile==declared;
            ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
            ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
            if (d.failures!=(accepted ? 0u : FR_AUTHORED_FAIL_INPUT) ||
                d.structural_pixels!=(uint32_t)accepted)
                printf("Canonical material %u with profile %u: failure bits %u, structural pixels %u\n",
                       material,declared,d.failures,d.structural_pixels);
            ASSERT_EQ(d.failures,accepted ? 0u : FR_AUTHORED_FAIL_INPUT);
            ASSERT_EQ(d.structural_pixels,(uint32_t)accepted);
        }
    }
    dc_gpu_destroy(gpu); PASS();
}

static void test_staging_resize_releases_allocation_and_bad_envelope_keeps_prior(void) {
    fr_authored_request_t r = request(); pixel(1,1,2,3,0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    dc_gpu_memory_stats_t large, small;
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &large));
    r.width = 1; r.height = 1;
    r.physical_bytes = 8; r.appearance_bytes = 64;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &small));
    ASSERT_TRUE(small.mapped_local_bytes + small.mapped_system_bytes <
                large.mapped_local_bytes + large.mapped_system_bytes);
    r.width = UINT32_MAX;
    ASSERT_TRUE(!fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(d.failures, 0u); ASSERT_EQ(d.rigid_pixels, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_structural_references_require_existing_nodes(void) {
    fr_authored_request_t r=request();
    fr_structural_gpu_graph_t graph={.node_count=2,.member_count=1,.joint_count=1,.anchor_count=1};
    graph.nodes[0]=(fr_structural_gpu_node_t){.id=46,.generation=2};
    graph.nodes[1]=(fr_structural_gpu_node_t){.id=9,.generation=3,.x_fp=65536};
    graph.members[0]=(fr_structural_gpu_member_t){.id=300,.generation=4,.a=46,.b=9,.depth_fp=65536};
    graph.joints[0]=(fr_structural_gpu_joint_t){.id=16,.generation=3,.a=46,.b=9,.kind=FR_JOINT_PINNED};
    graph.anchors[0]=(fr_structural_gpu_anchor_t){.id=20,.generation=1,.node=46,.kind=FR_JOINT_PINNED,
        .width=1,.height=1,.terrain_material=DC_MATERIAL_STONE};
    uint32_t owners[64]={300};
    r.structure=&graph; r.member_ownership=(const uint8_t *)owners; r.member_ownership_bytes=sizeof(owners);
    pixel(0,0,1,FR_AUTHORED_STRUCTURE,1);
    dc_gpu_t *gpu=context(); ASSERT_TRUE(gpu);
    char err[256]; fr_authored_stage_diagnostic_t d;
    uint32_t *references[]={&graph.members[0].b,&graph.joints[0].a,&graph.anchors[0].node};
    for(unsigned i=0;i<sizeof(references)/sizeof(references[0]);++i) {
        uint32_t original=*references[i];
        *references[i]=301;
        ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
        ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
        ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
        *references[i]=original;
    }
    graph.nodes[1].id=46;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    graph.nodes[1].id=9;
    graph.anchors[0].generation=0;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    graph.anchors[0].generation=1;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
    ASSERT_EQ(d.failures,0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_invalid_interface_mechanics_rejected_before_placement(void) {
    fr_authored_request_t r=request();
    fr_structural_gpu_graph_t graph={.node_count=2,.member_count=1,.joint_count=1,.anchor_count=1};
    graph.nodes[0]=(fr_structural_gpu_node_t){.id=1,.generation=1};
    graph.nodes[1]=(fr_structural_gpu_node_t){.id=2,.generation=1,.x_fp=65536};
    graph.members[0]=(fr_structural_gpu_member_t){.id=8,.generation=1,.a=1,.b=2,.depth_fp=65536};
    fr_structural_gpu_joint_t *joint=&graph.joints[0];
    *joint=(fr_structural_gpu_joint_t){.id=4,.generation=1,.a=1,.b=2,.kind=FR_JOINT_BONDED,.normal_angle=-.5f};
    fr_structural_gpu_anchor_t *anchor=&graph.anchors[0];
    *anchor=(fr_structural_gpu_anchor_t){.id=5,.generation=1,.node=1,.kind=FR_JOINT_PINNED,
        .width=1,.height=1,.terrain_material=DC_MATERIAL_STONE,.normal_angle=-1.f};
    uint32_t owners[64]={8};
    r.structure=&graph; r.member_ownership=(const uint8_t *)owners; r.member_ownership_bytes=sizeof(owners);
    pixel(0,0,1,FR_AUTHORED_STRUCTURE,1);
    dc_gpu_t *gpu=context(); ASSERT_TRUE(gpu);
    char err[256]; fr_authored_stage_diagnostic_t d;
    struct {float *field; float invalid;} cases[]={
        {&joint->normal_stiffness,NAN}, {&joint->moment_capacity,-1.f}, {&joint->normal_angle,INFINITY},
        {&anchor->friction,-.1f}, {&anchor->pullout_capacity,INFINITY}, {&anchor->normal_angle,NAN}};
    for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        float saved=*cases[i].field;
        *cases[i].field=cases[i].invalid;
        ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
        ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
        ASSERT_EQ(d.failures,FR_AUTHORED_FAIL_INPUT);
        *cases[i].field=saved;
    }
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&d,NULL,0,err,sizeof(err)));
    ASSERT_EQ(d.failures,0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_absent_normal_uses_renderer_default_not_raw_placeholder(void) {
    fr_authored_request_t r = request();
    memset(appearance + 16, 0, 12);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(d.failures, 0u);
    appearance[60] = 1;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    ASSERT_EQ(d.first_invalid_pixel, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_parallel_plans_span_workgroups_and_sparse_tiles(void) {
    fr_authored_request_t r = request();
    enum { WIDTH=64, HEIGHT=19, PIXELS=WIDTH*HEIGHT, SHAPE_WIDTH=33, SHAPE_HEIGHT=17 };
    uint8_t *p = calloc(PIXELS, 8u), *a = calloc(PIXELS, 64u);
    ASSERT_TRUE(p && a);
    r.width = WIDTH; r.height = HEIGHT; r.physical = p; r.appearance = a;
    r.physical_bytes = PIXELS * 8u; r.appearance_bytes = PIXELS * 64u;
    uint32_t words=(SHAPE_WIDTH*SHAPE_HEIGHT+7)/8;
    uint32_t *shape=calloc(words,sizeof(*shape));
    ASSERT_TRUE(shape);
    for (unsigned y=0;y<SHAPE_HEIGHT;++y) for (unsigned x=0;x<SHAPE_WIDTH;++x) {
        bool bottom_branch=y==SHAPE_HEIGHT-1 && x<=12;
        bool inner_prong=x==12 && y>=4;
        if (x && y && !bottom_branch && !inner_prong) continue;
        unsigned texel=(y+1)*WIDTH+x+1, local=y*SHAPE_WIDTH+x;
        p[texel*8]=1; p[texel*8+2]=FR_AUTHORED_RIGID;
        shape[local/8]|=1u<<((local%8)*4);
    }
    p[(5u * 64u + 60u) * 8u] = 2; p[(5u * 64u + 60u) * 8u + 2u] = 3;
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_component_plan_t plans[2]; uint32_t count = 0;
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu, plans, 2, &count, err, sizeof(err)));
    ASSERT_EQ(count, 2u); ASSERT_EQ(plans[0].source_root, 65u);
    dc_gpu_body_record_t bodies[DC_GPU_BODY_CAPACITY]={0};
    bodies[0].body=(dc_gpu_body_t){.width=SHAPE_WIDTH,.height=SHAPE_HEIGHT,.active=1};
    bodies[0].piece_count=DC_GPU_PIXEL_BODY_FLAG | 73u;
    ASSERT_TRUE(dc_gpu_upgrade_dense_pixels(gpu,bodies,&shape,&words));
    ASSERT_EQ(shape[1],4u); ASSERT_EQ(words,146u);
    ASSERT_EQ(plans[0].occupied,73u);
    ASSERT_EQ(plans[0].width,SHAPE_WIDTH); ASSERT_EQ(plans[0].height,SHAPE_HEIGHT);
    printf("Sparse shape runtime/planned: tiles %u/%u, words %u/%u\n",
           shape[1],plans[0].tiles,words,plans[0].geometry_words);
    ASSERT_EQ(plans[0].tiles,shape[1]); ASSERT_EQ(plans[0].geometry_words,words);
    ASSERT_EQ(plans[1].source_root, 380u); ASSERT_EQ(plans[1].body_slot, 1u);
    uint32_t pool_offset=dc_gpu_pixel_pool_word_offset(gpu);
    ((uint32_t *)gpu->body_mapped)[pool_offset]=1u;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_EQ(plans[0].geometry_word,pool_offset+DC_GPU_PIXEL_POOL_HEADER_WORDS+dc_gpu_pixel_pool_capacity(gpu));
    ASSERT_EQ(plans[1].geometry_word,plans[0].geometry_word+words);
    dc_gpu_destroy(gpu); free(shape); free(p); free(a); PASS();
}

static void test_failed_resize_preserves_ready_component_plan(void) {
    fr_authored_request_t r = request();
    pixel(2, 1, 1, 3, 7);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    dc_gpu_memory_stats_t before, after;
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &before));
    r.width = r.height = 1;
    r.physical_bytes = 8; r.appearance_bytes = 64;
    fail_descriptor_pool = true;
    bool prepared = fr_gpu_prepare_authored(gpu, &r, err, sizeof(err));
    fail_descriptor_pool = false;
    ASSERT_TRUE(!prepared);
    ASSERT_TRUE(strstr(err, "descriptor pool"));
    fr_authored_component_plan_t plan;
    uint32_t count;
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu, &plan, 1, &count, err, sizeof(err)));
    ASSERT_EQ(count, 1u); ASSERT_EQ(plan.source_root, 10u); ASSERT_EQ(plan.object, 7u);
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &after));
    ASSERT_EQ(before.mapped_system_bytes, after.mapped_system_bytes);
    ASSERT_EQ(before.mapped_local_bytes, after.mapped_local_bytes);
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    dc_gpu_destroy(gpu); PASS();
}

static void test_component_plan_only_offers_available_live_slots(void) {
    fr_authored_request_t r=request();
    pixel(1,1,1,FR_AUTHORED_RIGID,1);
    pixel(6,6,2,FR_AUTHORED_RIGID,2);
    dc_gpu_t *gpu=context(); ASSERT_TRUE(gpu);
    char err[256];
    uint32_t original=dc_gpu_body_capacity(gpu);
    for (uint32_t i=0;i<original-1;++i) {
        dc_gpu_body_t body={.active=1,.width=1,.height=1,.id=100+i};
        ASSERT_TRUE(dc_gpu_spawn_body(gpu,body,err,sizeof(err)));
    }
    fr_authored_stage_diagnostic_t diagnostic;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&diagnostic,NULL,0,err,sizeof(err)));
    ASSERT_EQ(diagnostic.failures,FR_AUTHORED_FAIL_CAPACITY);
    ASSERT_EQ(diagnostic.rigid_components,2u);
    fr_authored_component_plan_t plans[2]={{.body_slot=UINT32_MAX}};
    uint32_t count=0;
    ASSERT_TRUE(!fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_TRUE(strstr(err,"free body slots"));
    ASSERT_EQ(count,2u); ASSERT_EQ(plans[0].body_slot,UINT32_MAX);
    ASSERT_TRUE(!dc_gpu_body_record(gpu,original-1)->body.active);

    ASSERT_TRUE(dc_gpu_runtime_bodies_reserve(gpu,original*8,err,sizeof(err)));
    dc_gpu_body_owner(gpu,original)->state=DC_BODY_OWNER_RESERVED;
    dc_gpu_body_owner(gpu,original+1)->generation=UINT32_MAX;
    dc_gpu_body_record(gpu,original+2)->reserved[0]=DC_GPU_STRUCTURAL_TAG;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_EQ(count,2u); ASSERT_EQ(plans[0].body_slot,original-1);
    ASSERT_EQ(plans[1].body_slot,original+3);
    ASSERT_EQ(dc_gpu_body_owner(gpu,original)->state,DC_BODY_OWNER_RESERVED);
    ASSERT_EQ(dc_gpu_body_owner(gpu,original+1)->generation,UINT32_MAX);
    ASSERT_EQ(dc_gpu_body_record(gpu,original+2)->reserved[0],DC_GPU_STRUCTURAL_TAG);
    ASSERT_EQ(dc_gpu_body_record(gpu,0)->body.id,100u);
    uint32_t next=original*5+3;
    for (uint32_t slot=original+3;slot<next;++slot)
        dc_gpu_body_owner(gpu,slot)->state=DC_BODY_OWNER_RESERVED;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_EQ(count,2u); ASSERT_EQ(plans[0].body_slot,original-1);
    ASSERT_EQ(plans[1].body_slot,next);
    printf("Sparse body admission retained live ownership and proposed slots %u and %u\n",
           plans[0].body_slot,plans[1].body_slot);
    dc_gpu_destroy(gpu); PASS();
}

static void test_component_plan_respects_used_geometry_and_aggregate_demand(void) {
    enum { WIDTH=1920, HEIGHT=3, PIXELS=WIDTH*HEIGHT };
    fr_authored_request_t r=request();
    uint8_t *physical_map=calloc(PIXELS,FR_AUTHORED_PHYSICAL_STRIDE);
    uint8_t *colors=calloc(PIXELS,FR_AUTHORED_APPEARANCE_STRIDE);
    ASSERT_TRUE(physical_map && colors);
    for (unsigned i=0;i<PIXELS;++i) if (i/WIDTH!=1u) {
        physical_map[i*8u]=1;
        physical_map[i*8u+2u]=FR_AUTHORED_RIGID;
    }
    r.width=WIDTH; r.height=HEIGHT; r.physical=physical_map; r.appearance=colors;
    r.physical_bytes=PIXELS*FR_AUTHORED_PHYSICAL_STRIDE;
    r.appearance_bytes=PIXELS*FR_AUTHORED_APPEARANCE_STRIDE;
    dc_gpu_t *gpu=context(); ASSERT_TRUE(gpu);
    char err[256]; fr_authored_stage_diagnostic_t diagnostic;
    fr_authored_component_plan_t plans[2]; uint32_t count=0;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_EQ(count,2u); ASSERT_EQ(plans[0].geometry_words,4088u);
    ASSERT_EQ(plans[1].geometry_words,4088u);
    const uint32_t slots=dc_gpu_slot_capacity(gpu)*DC_CHUNK_CELLS;
    uint32_t *objects=calloc(slots,sizeof(*objects));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    ASSERT_TRUE(objects && chunk);
    chunk->cells[100].material=DC_MATERIAL_WOOD;
    objects[100]=11;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_admit_rigid_objects(gpu,1,objects,slots,err,sizeof(err)));
    uint32_t *pool=(uint32_t *)gpu->body_mapped+dc_gpu_pixel_pool_word_offset(gpu);
    uint32_t capacity=dc_gpu_pixel_pool_capacity(gpu), header[DC_GPU_PIXEL_POOL_HEADER_WORDS];
    ASSERT_EQ(pool[1],42u);
    ASSERT_TRUE(8176u <= capacity && 8176u > capacity-pool[1]);
    memcpy(header,pool,sizeof(header));
    uint32_t occupied[42];
    uint32_t *bank=pool+DC_GPU_PIXEL_POOL_HEADER_WORDS+pool[0]*capacity;
    memcpy(occupied,bank,sizeof(occupied));
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu,&diagnostic,NULL,0,err,sizeof(err)));
    ASSERT_EQ(diagnostic.failures,FR_AUTHORED_FAIL_CAPACITY);
    ASSERT_TRUE(!fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_TRUE(strstr(err,"geometry")); ASSERT_EQ(count,2u);
    ASSERT_EQ(memcmp(header,pool,sizeof(header)),0);
    ASSERT_EQ(memcmp(occupied,bank,sizeof(occupied)),0);
    ASSERT_TRUE(dc_gpu_runtime_geometry_reserve(gpu,8176u+pool[1],err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu,&r,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu,plans,2,&count,err,sizeof(err)));
    ASSERT_EQ(count,2u); ASSERT_EQ(plans[0].geometry_words+plans[1].geometry_words,8176u);
    pool=(uint32_t *)gpu->body_mapped+dc_gpu_pixel_pool_word_offset(gpu);
    bank=pool+DC_GPU_PIXEL_POOL_HEADER_WORDS+pool[0]*dc_gpu_pixel_pool_capacity(gpu);
    ASSERT_EQ(pool[1],42u); ASSERT_EQ(memcmp(occupied,bank,sizeof(occupied)),0);
    ASSERT_EQ(plans[0].geometry_word,(uint32_t)(bank-(uint32_t *)gpu->body_mapped)+pool[1]);
    ASSERT_EQ(plans[1].geometry_word,plans[0].geometry_word+plans[0].geometry_words);
    dc_gpu_destroy(gpu); free(chunk); free(objects); free(physical_map); free(colors); PASS();
}

static void test_unpublished_water_keeps_dry_world_asleep(void) {
    fr_authored_request_t r = request();
    pixel(1, 1, FR_PROP_MATERIAL_WATER, 1, 0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(!gpu->fluid_work_possible);
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(!gpu->fluid_work_possible);
    dc_gpu_destroy(gpu); PASS();
}

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--geometry-capacity")) {
        RUN(test_component_plan_respects_used_geometry_and_aggregate_demand);
        return g_fail!=0;
    }
    if(argc==2 && !strcmp(argv[1],"--body-slots")) {
        RUN(test_component_plan_only_offers_available_live_slots);
        return g_fail!=0;
    }
    if(argc==2 && !strcmp(argv[1],"--sparse-plan")) {
        RUN(test_parallel_plans_span_workgroups_and_sparse_tiles);
        return g_fail!=0;
    }
    RUN(test_structural_references_require_existing_nodes);
    RUN(test_invalid_interface_mechanics_rejected_before_placement);
    RUN(test_each_declared_member_requires_owned_structural_pixels);
    RUN(test_structural_pixels_use_registered_mechanical_profiles);
    if(argc==2 && !strcmp(argv[1],"--structural-graph")) return g_fail!=0;
    RUN(test_failed_resize_preserves_ready_component_plan);
    RUN(test_unpublished_water_keeps_dry_world_asleep);
    RUN(test_component_plan_respects_used_geometry_and_aggregate_demand);
    RUN(test_mixed_concavities_keep_exact_role_object_connectivity);
    RUN(test_diagonal_pixels_remain_independent);
    RUN(test_invalid_role_material_and_fabricated_structure_are_diagnosed);
    RUN(test_staging_resize_releases_allocation_and_bad_envelope_keeps_prior);
    RUN(test_absent_normal_uses_renderer_default_not_raw_placeholder);
    RUN(test_parallel_plans_span_workgroups_and_sparse_tiles);
    RUN(test_component_plan_only_offers_available_live_slots);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
