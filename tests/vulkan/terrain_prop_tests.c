#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/assets/prop_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/structural/gpu.h"

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

static void test_imported_soil_and_gravel_start_at_full_cell_density(void) {
    for(unsigned gravel=0;gravel<2;gravel++) {
        uint8_t *bytes=malloc(g_asset.size);ASSERT_TRUE(bytes);
        memcpy(bytes,g_bytes,g_asset.size);
        if(gravel) {
            for(unsigned i=0;i<g_asset.width*g_asset.height;i++) {
                uint8_t *record=bytes+g_asset.data_offset+i*8;
                if(record[0]==7 && record[1]==0) record[0]=8;
            }
            uint32_t crc=UINT32_MAX;
            for(size_t i=0;i<g_asset.size;i++) {
                if(i>=44&&i<48) continue;
                crc^=bytes[i];
                for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
            }
            crc=~crc;for(unsigned i=0;i<4;i++) bytes[44+i]=(uint8_t)(crc>>(i*8));
        }
        fr_prop_asset_t asset;char err[256];
        ASSERT_TRUE(fr_prop_asset_open(&asset,bytes,g_asset.size,err,sizeof(err)));
        dc_gpu_t *g=context();ASSERT_TRUE(g);
        ASSERT_TRUE(fr_gpu_place_terrain_prop(g,&asset,0,32,32,err,sizeof(err)));
        dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
        unsigned checked=0;uint32_t expected=(uint32_t)lround((gravel?2.7:1.2)*65536.);
        for(unsigned slot=0;slot<9;slot++) {
            ASSERT_TRUE(dc_gpu_download_chunk(g,slot,chunk,err,sizeof(err)));
            for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) {
                dc_mpm_particle_t *p=chunk->particles+i;
                if(!p->mass_fp||p->material!=(gravel?DC_MATERIAL_GRAVEL:DC_MATERIAL_DIRT)) continue;
                ASSERT_INT_EQ(p->mass_fp,expected);
                ASSERT_INT_EQ(p->grain_fp,DC_FLUID_FULL/2);
                ASSERT_TRUE(fabs(p->deformation[0]*p->deformation[3]-1.)<1e-6);
                checked++;
            }
        }
        ASSERT_EQ(checked,1294u);
        free(chunk);dc_gpu_destroy(g);free(bytes);
    }
    PASS();
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

static void check_detached_color(bool roundtrip) {
    dc_gpu_t *g=context();ASSERT_TRUE(g);char err[256]={0};
    uint8_t *bytes=malloc(g_asset.size);ASSERT_TRUE(bytes);
    memcpy(bytes,g_bytes,g_asset.size);
    uint8_t *physical=bytes+g_asset.data_offset;
    memset(physical,0,128*128*8);
    for(unsigned y=20;y<24;y++) for(unsigned x=20;x<49;x++) {
        bool wood=y<23&&x<29,stone=x>=40;
        if(!wood&&!stone) continue;
        unsigned i=y*128+x;physical[i*8]=wood?2:1;physical[i*8+2]=1;
        float rgb[3]={.1f+.02f*(x-20),.23f+.03f*(y-20),.71f};
        memcpy(physical+128*128*8+i*64,rgb,sizeof(rgb));
        float alpha=1;memcpy(physical+128*128*8+i*64+48,&alpha,4);
    }
    unsigned water=40*128+40;physical[water*8]=6;physical[water*8+2]=1;
    float water_rgb[3]={.12f,.8f,.3f},water_alpha=1;
    memcpy(physical+128*128*8+water*64,water_rgb,sizeof(water_rgb));
    memcpy(physical+128*128*8+water*64+48,&water_alpha,4);
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<g_asset.size;i++) {
        if(i>=44&&i<48) continue;
        crc^=bytes[i];
        for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    crc=~crc;memcpy(bytes+44,&crc,4);
    fr_prop_asset_t asset;
    ASSERT_TRUE(fr_prop_asset_open(&asset,bytes,g_asset.size,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_terrain_prop(g,&asset,0,32,32,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_paint_material(g,56,53,1,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    for(unsigned i=0;i<DC_GPU_BODY_CAPACITY;i++) {
        dc_gpu_body_record_t *body=(dc_gpu_body_record_t*)g->body_mapped+i;
        if(!body->body.active) continue;
        body->local_x_fp&=~65535;body->local_y_fp&=~65535;
        body->angle=body->angular_velocity=0;body->body.vx_fp=body->body.vy_fp=0;
        body->motion_flags=DC_GPU_BODY_KINEMATIC;
    }
    g->body_refresh_pending=true;
    ASSERT_TRUE(dc_gpu_set_body_origin(g,g->body_origin,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    uint32_t expected=0xff000000u|byte_from_linear(.1f)|
        (byte_from_linear(.23f)<<8)|(byte_from_linear(.71f)<<16);
    bool retained=((uint32_t*)g->mapped)[52*192+52]==expected;
    for(unsigned y=20;y<23;y++) for(unsigned x=20;x<29;x++) {
        int dx=(int)x-24,dy=(int)y-21;if(dx*dx+dy*dy<=1) continue;
        uint32_t color=0xff000000u|byte_from_linear(.1f+.02f*(x-20))|
            (byte_from_linear(.23f+.03f*(y-20))<<8)|(byte_from_linear(.71f)<<16);
        retained=retained && ((uint32_t*)g->mapped)[(32+y)*192+32+x]==color;
    }
    bool restored=true;
    if(roundtrip) {
        ASSERT_TRUE(dc_gpu_paint_material(g,100,30,1,DC_MATERIAL_STONE,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
        ASSERT_EQ(((uint32_t*)g->mapped)[52*192+52],expected);
        size_t image_bytes=192*192*4,water_bytes=9*4096*16;
        void *image=malloc(image_bytes),*colors=malloc(water_bytes);
        ASSERT_TRUE(image&&colors);
        memcpy(image,g->mapped,image_bytes);memcpy(colors,g->water_color_mapped,water_bytes);
        ASSERT_TRUE(fr_gpu_save_structure(g,"build/authored_color_roundtrip.dcs",err,sizeof(err)));
        dc_gpu_destroy(g);g=context();ASSERT_TRUE(g);
        ASSERT_TRUE(fr_gpu_load_structure(g,"build/authored_color_roundtrip.dcs",err,sizeof(err)));
        ASSERT_TRUE(fr_renderer_enable(g,err,sizeof(err)));
        ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
        ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
        restored=memcmp(image,g->mapped,image_bytes)==0 &&
            memcmp(colors,g->water_color_mapped,water_bytes)==0;
        FILE *bad=fopen("build/authored_color_roundtrip.dcs","r+b");ASSERT_TRUE(bad);
        ASSERT_EQ(fseek(bad,-4,SEEK_END),0);uint32_t nan=0x7fc00000u;
        ASSERT_EQ(fwrite(&nan,4,1,bad),1u);fclose(bad);
        ASSERT_TRUE(!fr_gpu_load_structure(g,"build/authored_color_roundtrip.dcs",err,sizeof(err)));
        ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
        restored=restored && memcmp(image,g->mapped,image_bytes)==0;
        free(image);free(colors);
    }
    dc_gpu_destroy(g);free(bytes);
    if(roundtrip) ASSERT_TRUE(restored);else ASSERT_TRUE(retained);
    PASS();
}
static void test_detached_body_keeps_authored_color_map(void) {check_detached_color(false);}
static void test_terrain_body_and_water_colors_survive_save_load(void) {check_detached_color(true);}
static void test_legacy_prop_bonds_release_leftover_tree_pixels_without_restamping(void) {
    dc_gpu_t *g=context();ASSERT_TRUE(g);char err[256];
    ASSERT_TRUE(dc_gpu_paint_material(g,32,32,0,DC_MATERIAL_AIR,err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));
    ASSERT_TRUE(chunk&&colors);
    const unsigned slot=1,at=60*64+25;
    chunk->coord=(dc_chunk_coord_t){1,0};
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN|DC_CELL_BONDS_READY};
    colors->terrain[at][0]=.73f;colors->terrain[at][1]=.21f;colors->terrain[at][2]=.11f;colors->terrain[at][3]=1;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,slot,colors,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_restore_terrain_prop_bonds(g,&g_asset,0,32,32,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,slot,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[at].material,DC_MATERIAL_WOOD);
    ASSERT_TRUE(chunk->cells[at].flags&DC_CELL_DIAGONAL_BONDS_READY);
    ASSERT_TRUE(chunk->cells[at].flags&8u);
    ASSERT_EQ(chunk->cells[61*64+26].material,DC_MATERIAL_AIR);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixel[1];unsigned count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixel,1,&count,err,sizeof(err)));
    ASSERT_EQ(count,1u);
    dc_gpu_body_record_t *b=g->body_mapped;const uint32_t *shape=(uint32_t*)g->body_mapped+b->compound_reserved;
    const float *rgb=dc_gpu_shape_colors(g,b->compound_reserved+shape[8]+1);
    ASSERT_TRUE(!memcmp(rgb,colors->terrain[at],16));
    ASSERT_TRUE(dc_gpu_download_chunk(g,slot,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[at].material,DC_MATERIAL_AIR);
    dc_gpu_destroy(g);free(chunk);free(colors);PASS();
}
static void test_old_save_recovers_missing_granular_support_history(void) {
    dc_gpu_t *g=context();ASSERT_TRUE(g);char err[256];
    ASSERT_TRUE(dc_gpu_paint_material(g,32,32,0,DC_MATERIAL_AIR,err,sizeof(err)));
    unsigned source=UINT32_MAX;
    for(unsigned i=0;i+g_asset.width<g_asset.width*g_asset.height;i++) {
        fr_prop_pixel_t wood,soil;
        ASSERT_TRUE(fr_prop_pixel(&g_asset,0,i,&wood));
        ASSERT_TRUE(fr_prop_pixel(&g_asset,0,i+g_asset.width,&soil));
        if(wood.material==2&&soil.material==7&&(32+i/g_asset.width)%64<63) {source=i;break;}
    }
    ASSERT_TRUE(source!=UINT32_MAX);
    unsigned x=32+source%g_asset.width,y=32+source/g_asset.width,slot=y/64*3+x/64,at=y%64*64+x%64;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->coord=(dc_chunk_coord_t){x/64,y/64};
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,
        .flags=DC_CELL_LOADED_TERRAIN|DC_CELL_BONDS_READY|DC_CELL_DIAGONAL_BONDS_READY};
    chunk->cells[at+64].material=DC_MATERIAL_DIRT;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_restore_terrain_prop_bonds(g,&g_asset,0,32,32,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,slot,chunk,err,sizeof(err)));
    ASSERT_TRUE(chunk->cells[at].flags&DC_CELL_SUPPORT_BONDS_READY);
    ASSERT_EQ(chunk->cells[at+64].flags,0u);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixel;unsigned count;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,&pixel,1,&count,err,sizeof(err)));
    ASSERT_EQ(count,1u);dc_gpu_destroy(g);free(chunk);PASS();
}

int main(void) {
    FILE *file=fopen("assets/props/level_piece01.prop/asset.cprop","rb");
    if (!file) return 2;
    fseek(file,0,SEEK_END); long size=ftell(file); rewind(file);
    g_bytes=malloc((size_t)size);
    if (!g_bytes || fread(g_bytes,1,(size_t)size,file)!=(size_t)size) return 2;
    fclose(file);
    if (!fr_prop_asset_open(&g_asset,g_bytes,(size_t)size,NULL,0)) return 2;
    RUN(test_imported_soil_and_gravel_start_at_full_cell_density);
    RUN(test_old_save_recovers_missing_granular_support_history);
    RUN(test_actual_scene_places_exact_materials_colors_water_and_holes);
    RUN(test_unknown_occupied_destination_rejects_entire_scene);
    RUN(test_detached_body_keeps_authored_color_map);
    RUN(test_terrain_body_and_water_colors_survive_save_load);
    RUN(test_legacy_prop_bonds_release_leftover_tree_pixels_without_restamping);
    free(g_bytes); printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail!=0;
}
