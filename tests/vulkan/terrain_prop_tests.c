#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/assets/prop_gpu.h"
#include "ferrum/renderer/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s\n", __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static void *g_bytes;
static fr_prop_asset_t g_asset;
static dc_gpu_t *context(void) {
    dc_gpu_t *g = NULL;
    char err[256];
    if (!dc_gpu_create(&g, 192, 192, "build/shaders/pattern.comp.spv", err, sizeof(err))) return NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) { dc_gpu_destroy(g); return NULL; }
    bool okay = true;
    for (uint32_t y=0; y<3 && okay; y++) for (uint32_t x=0; x<3 && okay; x++) {
        chunk->coord = (dc_chunk_coord_t){x,y};
        chunk->cells[32*64+32].material = x==0 && y==0 ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
        okay = dc_gpu_upload_chunk(g,y*3+x,chunk,err,sizeof(err)) &&
            dc_gpu_set_page(g,x,y,y*3+x,err,sizeof(err));
    }
    free(chunk);
    if (!okay) { dc_gpu_destroy(g); return NULL; }
    return g;
}

static uint32_t byte_from_linear(float value) {
    float srgb = value <= .0031308f ? value*12.92f : 1.055f*powf(value,1.f/2.4f)-.055f;
    return (uint32_t)lroundf(srgb*255.f);
}

static void test_actual_scene_places_exact_materials_colors_water_and_holes(void) {
    dc_gpu_t *g=context(); ASSERT_TRUE(g); char err[256];
    ASSERT_TRUE(fr_gpu_place_terrain_prop(g,&g_asset,0,32,32,err,sizeof(err)));
    dc_chunk_t *chunks=calloc(9,sizeof(*chunks)); ASSERT_TRUE(chunks);
    for (uint32_t i=0;i<9;i++) ASSERT_TRUE(dc_gpu_download_chunk(g,i,&chunks[i],err,sizeof(err)));
    uint32_t counts[10]={0}, sample=UINT32_MAX;
    for (uint32_t i=0;i<128*128;i++) {
        fr_prop_pixel_t p; ASSERT_TRUE(fr_prop_pixel(&g_asset,0,i,&p));
        uint32_t x=32+i%128,y=32+i/128;
        dc_cell_t cell=chunks[(y/64)*3+x/64].cells[(y%64)*64+x%64];
        counts[p.material]++;
        if (!p.material) continue;
        ASSERT_EQ(cell.material, p.material==6 ? DC_MATERIAL_AIR : fr_prop_terrain_material(p.material));
        ASSERT_EQ(cell.fluid_mass,p.material==6 ? DC_FLUID_FULL : 0u);
        if(p.material==6) {
            fr_prop_appearance_t appearance;
            ASSERT_TRUE(fr_prop_appearance(&g_asset,0,i,&appearance));
            const float default_color[3]={.035f,.193f,.666f};
            uint32_t atlas=((y/64)*3+x/64)*4096+(y%64)*64+x%64;
            const float *pigment=(const float*)g->water_color_mapped+atlas*4;
            ASSERT_EQ(pigment[3],1.f);
            for(unsigned channel=0;channel<3;channel++) {
                float expected=default_color[channel]*(1.f-appearance.alpha)+
                    appearance.color[channel]*appearance.alpha;
                ASSERT_TRUE(fabsf(pigment[channel]-expected)<1e-6f);
            }
        }
        if (p.material==1 && sample==UINT32_MAX) sample=i;
    }
    ASSERT_EQ(counts[0],7729u); ASSERT_EQ(counts[1],5096u);
    ASSERT_EQ(counts[2],761u); ASSERT_EQ(counts[7],1294u); ASSERT_EQ(counts[6],1504u);
    ASSERT_EQ(chunks[0].cells[32*64+32].material,DC_MATERIAL_STONE);
    ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    fr_prop_appearance_t a; ASSERT_TRUE(fr_prop_appearance(&g_asset,0,sample,&a));
    uint32_t expected=0xff000000u|byte_from_linear(a.color[0])|
        (byte_from_linear(a.color[1])<<8)|(byte_from_linear(a.color[2])<<16);
    uint32_t index=(32+sample/128)*192+32+sample%128;
    ASSERT_EQ(((uint32_t*)g->mapped)[index],expected);
    uint32_t before=((uint32_t*)g->mapped)[index];
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t*)g->mapped)[index],before);
    free(chunks); dc_gpu_destroy(g); PASS();
}

static void test_unknown_occupied_destination_rejects_entire_scene(void) {
    dc_gpu_t *g=context(); ASSERT_TRUE(g); char err[256];
    ASSERT_TRUE(dc_gpu_set_page(g,1,1,UINT32_MAX,err,sizeof(err)));
    ASSERT_TRUE(!fr_gpu_place_terrain_prop(g,&g_asset,0,32,32,err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk)); ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[32*64+32].material,DC_MATERIAL_STONE);
    uint32_t occupied=0;
    for (uint32_t i=0;i<4096;i++) occupied+=chunk->cells[i].material!=0 || chunk->cells[i].fluid_mass!=0;
    ASSERT_EQ(occupied,1u);
    free(chunk); dc_gpu_destroy(g); PASS();
}

int main(void) {
    FILE *file=fopen("assets/props/level_piece01.prop/asset.cprop","rb");
    if (!file) return 2;
    fseek(file,0,SEEK_END); long size=ftell(file); rewind(file);
    g_bytes=malloc((size_t)size);
    if (!g_bytes || fread(g_bytes,1,(size_t)size,file)!=(size_t)size) return 2;
    fclose(file);
    if (!fr_prop_asset_open(&g_asset,g_bytes,(size_t)size,NULL,0)) return 2;
    RUN(test_actual_scene_places_exact_materials_colors_water_and_holes);
    RUN(test_unknown_occupied_destination_rejects_entire_scene);
    free(g_bytes); printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail!=0;
}
