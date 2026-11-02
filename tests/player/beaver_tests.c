#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/material_profile.h"
#include "ferrum/player/beaver.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n",#fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);g_fail++;return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static uint8_t g_bytes[2000000];
static uint32_t g_mapping[FR_BEAVER_PIXELS*16];
static fr_beaver_asset_t g_asset;

static bool build(fr_player_action_t action) {
    return fr_beaver_asset_build(action,g_bytes,sizeof(g_bytes),g_mapping,
        FR_BEAVER_PIXELS*16,&g_asset,NULL,0);
}

static void test_standard_prop_pipeline_and_exact_clip_timing(void) {
    const uint32_t counts[]={1,6,8,8},durations[]={100,100,100,100};
    for (uint32_t a=0;a<FR_PLAYER_ACTION_COUNT;a++) {
        size_t bytes=0;uint32_t entries=0;
        ASSERT_TRUE(fr_beaver_asset_size((fr_player_action_t)a,&bytes,&entries));
        ASSERT_EQ(entries,counts[a]*FR_BEAVER_PIXELS*(a==FR_PLAYER_CAST?2:1)); ASSERT_TRUE(bytes<=sizeof(g_bytes));
        ASSERT_TRUE(build((fr_player_action_t)a));
        fr_prop_asset_t reopened;
        ASSERT_TRUE(fr_prop_asset_open(&reopened,g_bytes,bytes,NULL,0));
        ASSERT_EQ(reopened.width,(a==FR_PLAYER_CAST?80u:FR_BEAVER_WIDTH)); ASSERT_EQ(reopened.height,FR_BEAVER_HEIGHT);
        ASSERT_EQ(reopened.frame_count,counts[a]); ASSERT_EQ(reopened.flags,1u);
        for (uint32_t f=0;f<counts[a];f++) {
            ASSERT_EQ(fr_prop_animation_frame(&reopened,(uint64_t)f*durations[a]),f);
            ASSERT_EQ(fr_prop_animation_frame(&reopened,(uint64_t)(f+1)*durations[a]-1),f);
        }
        ASSERT_EQ(fr_prop_animation_frame(&reopened,(uint64_t)counts[a]*durations[a]),0u);
    }
    PASS();
}

static void test_every_visible_pixel_has_supported_physics_and_appearance(void) {
    ASSERT_TRUE(build(FR_PLAYER_WALK)); uint32_t regions[FR_BEAVER_REGION_COUNT]={0},holes=0;
    for (uint32_t p=0;p<FR_BEAVER_PIXELS;p++) {
        fr_prop_pixel_t pixel; uint32_t source=999;
        ASSERT_TRUE(fr_beaver_asset_pixel(&g_asset,0,p%FR_BEAVER_WIDTH,p/FR_BEAVER_WIDTH,false,&pixel,&source));
        fr_prop_appearance_t app; ASSERT_TRUE(fr_prop_appearance(&g_asset.prop,0,p,&app));
        if (!pixel.material) {ASSERT_EQ(source,0u);ASSERT_EQ(app.alpha,0.f);holes++;continue;}
        ASSERT_TRUE(source>0 && source<=FR_BEAVER_PIXELS); ASSERT_EQ(pixel.role,3u);
        fr_beaver_source_t rest; ASSERT_TRUE(fr_beaver_source(source,&rest));
        ASSERT_EQ(pixel.material,rest.prop_material); regions[rest.region]++;
        uint32_t material=fr_prop_terrain_material(pixel.material);
        const fr_reactive_profile_t *profile=fr_reactive_profile(material);
        ASSERT_TRUE(profile && fr_reactive_profile_for_body(fr_prop_body_material(pixel.material))==profile);
        if (rest.region!=FR_BEAVER_TEETH) {
            ASSERT_TRUE(profile->fuel_fraction>0 && profile->moisture_capacity>0);
            ASSERT_TRUE((profile->flags&FR_REACTIVE_WATER_DOUSEABLE)!=0);
        }
        ASSERT_EQ(app.alpha,1.f); ASSERT_TRUE(app.normal[2]>0);
    }
    for (uint32_t r=FR_BEAVER_FUR;r<=FR_BEAVER_TAIL;r++) ASSERT_TRUE(regions[r]>0);
    ASSERT_TRUE(holes>FR_BEAVER_PIXELS/2); PASS();
}

static void test_each_requested_frame_has_a_distinct_pose(void) {
    for (uint32_t a=FR_PLAYER_WALK;a<FR_PLAYER_ACTION_COUNT;a++) {
        ASSERT_TRUE(build((fr_player_action_t)a));
        for (uint32_t f=0;f<g_asset.prop.frame_count;f++) {
            for (uint32_t previous=0;previous<f;previous++)
                ASSERT_TRUE(memcmp(g_mapping+previous*g_asset.prop.width*FR_BEAVER_HEIGHT,
                    g_mapping+f*g_asset.prop.width*FR_BEAVER_HEIGHT,g_asset.prop.width*FR_BEAVER_HEIGHT*sizeof(*g_mapping))!=0);
        }
    }
    PASS();
}

static void test_flip_preserves_material_and_canonical_pixel_identity(void) {
    ASSERT_TRUE(build(FR_PLAYER_CHEW));
    for (uint32_t f=0;f<8;f++) for (uint32_t p=0;p<FR_BEAVER_PIXELS;p++) {
        fr_prop_pixel_t a,b;uint32_t first,second;
        uint32_t x=p%FR_BEAVER_WIDTH,y=p/FR_BEAVER_WIDTH;
        ASSERT_TRUE(fr_beaver_asset_pixel(&g_asset,f,x,y,false,&a,&first));
        ASSERT_TRUE(fr_beaver_asset_pixel(&g_asset,f,FR_BEAVER_WIDTH-1-x,y,true,&b,&second));
        ASSERT_EQ(first,second); ASSERT_EQ(a.material,b.material);ASSERT_EQ(a.object,b.object);
    }
    PASS();
}

static void test_source_owner_coordinates_survive_frames_and_player_motion(void) {
    fr_appearance_owner_t owners[FR_BEAVER_PIXELS]={0};
    fr_reactive_owner_t reactive[FR_BEAVER_PIXELS]={0};
    bool seen[FR_BEAVER_PIXELS]={false};uint32_t matches=0;
    for (uint32_t a=FR_PLAYER_WALK;a<FR_PLAYER_ACTION_COUNT;a++) {
        ASSERT_TRUE(build((fr_player_action_t)a));
        for (uint32_t f=0;f<g_asset.prop.frame_count;f++) for (uint32_t p=0;p<g_asset.prop.width*FR_BEAVER_HEIGHT;p++) {
            uint32_t source=g_mapping[f*g_asset.prop.width*FR_BEAVER_HEIGHT+p];if (!source) continue;
            fr_appearance_owner_t app;fr_reactive_owner_t rx;
            ASSERT_TRUE(fr_beaver_pixel_owners(source,41,101,7,&app,&rx));
            ASSERT_TRUE(fr_reactive_owner_valid(&rx,NULL,0));
            ASSERT_EQ(app.domain,FR_APPEARANCE_BODY); ASSERT_EQ(app.pixel_index,source-1);
            if (seen[source-1]) {
                ASSERT_EQ(app.identity,owners[source-1].identity);
                ASSERT_EQ(app.x,owners[source-1].x); ASSERT_EQ(app.y,owners[source-1].y);
                ASSERT_TRUE(memcmp(&rx,&reactive[source-1],sizeof(rx))==0);matches++;
            } else {owners[source-1]=app;reactive[source-1]=rx;seen[source-1]=true;}
            fr_appearance_owner_t other;fr_reactive_owner_t other_rx;
            ASSERT_TRUE(fr_beaver_pixel_owners(source,42,102,3,&other,&other_rx));
            ASSERT_TRUE(!fr_appearance_matches(&app,other.identity,other.generation));
            ASSERT_TRUE(other_rx.key[0]!=rx.key[0]); ASSERT_EQ(other_rx.key[2],rx.key[2]);
        }
    }
    ASSERT_TRUE(matches>1000); PASS();
}

static void test_invalid_inputs_preserve_buffers_and_outputs(void) {
    size_t bytes=99;uint32_t entries=99;char err[100]={0};
    ASSERT_TRUE(!fr_beaver_asset_size(FR_PLAYER_ACTION_COUNT,&bytes,&entries));
    ASSERT_EQ(bytes,99u);ASSERT_EQ(entries,99u);
    memset(g_bytes,0x5a,sizeof(g_bytes));g_mapping[0]=99;
    ASSERT_TRUE(!fr_beaver_asset_build(FR_PLAYER_WALK,g_bytes,1,g_mapping,
        FR_BEAVER_PIXELS*16,&g_asset,err,sizeof(err)));
    ASSERT_EQ(g_bytes[0],0x5au); ASSERT_EQ(g_mapping[0],99u);ASSERT_TRUE(err[0]);
    ASSERT_TRUE(!fr_beaver_asset_build(FR_PLAYER_WALK,g_bytes,sizeof(g_bytes),g_mapping,1,&g_asset,NULL,0));
    ASSERT_EQ(g_bytes[0],0x5au);ASSERT_EQ(g_mapping[0],99u);
    ASSERT_TRUE(build(FR_PLAYER_CAST));
    fr_prop_pixel_t pixel={.material=99};uint32_t source=99;
    ASSERT_TRUE(!fr_beaver_asset_pixel(&g_asset,8,0,0,false,&pixel,&source));
    ASSERT_TRUE(!fr_beaver_asset_pixel(&g_asset,0,80,0,false,&pixel,&source));
    ASSERT_EQ(pixel.material,99);ASSERT_EQ(source,99u);
    fr_beaver_source_t rest;ASSERT_TRUE(!fr_beaver_source(0,&rest));
    ASSERT_TRUE(!fr_beaver_source(UINT32_MAX,&rest));
    fr_appearance_owner_t app={.identity=99};fr_reactive_owner_t rx={.kind=99};
    ASSERT_TRUE(!fr_beaver_pixel_owners(0,41,101,7,&app,&rx));
    ASSERT_EQ(app.identity,99u);ASSERT_EQ(rx.kind,99u);PASS();
}

static void test_asset_build_is_deterministic(void) {
    ASSERT_TRUE(build(FR_PLAYER_CAST));size_t size;uint32_t entries;
    ASSERT_TRUE(fr_beaver_asset_size(FR_PLAYER_CAST,&size,&entries));
    uint8_t *copy=malloc(size);uint32_t *mapping=malloc(entries*sizeof(*mapping));
    ASSERT_TRUE(copy && mapping);memcpy(copy,g_bytes,size);memcpy(mapping,g_mapping,entries*sizeof(*mapping));
    ASSERT_TRUE(build(FR_PLAYER_CAST));ASSERT_TRUE(memcmp(copy,g_bytes,size)==0);
    ASSERT_TRUE(memcmp(mapping,g_mapping,entries*sizeof(*mapping))==0);
    free(copy);free(mapping);PASS();
}

static void test_authoritative_rgba_pixels_and_unique_material_correspondence(void) {
    static const uint32_t hashes[]={0x158a6745,0x906dd356,0x1d8c9342,0xd921ad04,
        0xd63103dc,0x143252e9,0x5f0f05cc,0x49af873e};
    for (uint32_t a=0;a<2;a++) {
        ASSERT_TRUE(build(a?FR_PLAYER_CHEW:FR_PLAYER_IDLE));
        for (uint32_t f=0;f<g_asset.prop.frame_count;f++) {
            uint32_t hash=2166136261u; bool used[FR_BEAVER_PIXELS]={false};
            for (uint32_t p=0;p<FR_BEAVER_PIXELS;p++) {
                fr_prop_appearance_t app; ASSERT_TRUE(fr_prop_appearance(&g_asset.prop,f,p,&app));
                for (uint32_t c=0;c<4;c++) {
                    float linear=c==3?app.alpha:app.color[c];
                    float value=c==3?linear:(linear<=.0031308f?12.92f*linear:1.055f*powf(linear,1.f/2.4f)-.055f);
                    hash=(hash^(uint8_t)lroundf(value*255.f))*16777619u;
                }
                fr_prop_pixel_t pixel;uint32_t source;
                ASSERT_TRUE(fr_beaver_asset_pixel(&g_asset,f,p%40,p/40,false,&pixel,&source));
                if (source) {ASSERT_TRUE(!used[source-1]);used[source-1]=true;}
                ASSERT_EQ(source!=0,app.alpha>0);
            }
            ASSERT_EQ(hash,hashes[f]);
        }
    }
    PASS();
}

static void test_authoritative_cast_wide_rgba_and_stable_owners(void) {
    static const uint32_t hashes[]={0x34fde37c,0x5a3c3633,0xd3e40594,0xb8a01573,
        0x56839d2d,0x11bc53b2,0x0cca2fda,0x3247195d};
    ASSERT_TRUE(build(FR_PLAYER_CAST));
    ASSERT_EQ(g_asset.prop.width,80u); ASSERT_EQ(g_asset.prop.height,40u);
    ASSERT_EQ(g_asset.prop.frame_count,8u);
    bool wide=false,partial=false;
    for (uint32_t f=0;f<8;f++) {
        uint32_t hash=2166136261u; bool used[FR_BEAVER_PIXELS]={false};
        for (uint32_t p=0;p<3200;p++) {
            fr_prop_appearance_t app; ASSERT_TRUE(fr_prop_appearance(&g_asset.prop,f,p,&app));
            for (uint32_t c=0;c<4;c++) {
                float v=c==3?app.alpha:app.color[c];
                if (c!=3) v=v<=.0031308f?12.92f*v:1.055f*powf(v,1.f/2.4f)-.055f;
                hash=(hash^(uint8_t)lroundf(v*255.f))*16777619u;
            }
            fr_prop_pixel_t pixel;uint32_t source;
            ASSERT_TRUE(fr_beaver_asset_pixel(&g_asset,f,p%80,p/80,false,&pixel,&source));
            if (source) {
                ASSERT_TRUE(source<=FR_BEAVER_PIXELS && !used[source-1]); used[source-1]=true;
                fr_beaver_source_t metadata; ASSERT_TRUE(fr_beaver_source(source,&metadata));
                ASSERT_EQ(metadata.prop_material,pixel.material);
                fr_prop_pixel_t flipped;uint32_t mirror;
                ASSERT_TRUE(fr_beaver_asset_pixel(&g_asset,f,79-p%80,p/80,true,&flipped,&mirror));
                ASSERT_EQ(mirror,source);
                wide|=p%80>=40; partial|=app.alpha<1.f;
            }
            ASSERT_EQ(source!=0,app.alpha>0);
        }
        ASSERT_EQ(hash,hashes[f]);
    }
    ASSERT_TRUE(wide && partial); PASS();
}

static void test_runtime_crop_and_stable_foot_contact(void) {
    static const uint32_t cast_feet[8]={32,33,33,33,33,32,32,32};
    for(uint32_t a=0;a<FR_PLAYER_ACTION_COUNT;a++) {
        ASSERT_TRUE(build((fr_player_action_t)a));
        for(uint32_t f=0;f<g_asset.prop.frame_count;f++) {
            fr_beaver_frame_layout_t layout;
            ASSERT_TRUE(fr_beaver_frame_layout(&g_asset,f,&layout));
            ASSERT_EQ(layout.canvas_width,a==FR_PLAYER_CAST?80u:40u);
            ASSERT_EQ(layout.canvas_height,40u);
            ASSERT_EQ(layout.foot_baseline,a==FR_PLAYER_CAST?cast_feet[f]:32u);
            ASSERT_TRUE(layout.y+layout.height<40u);
            ASSERT_TRUE(layout.width>0&&layout.height>0);
            bool bottom_occupied=false;
            for(uint32_t y=0;y<40;y++) for(uint32_t x=0;x<g_asset.prop.width;x++) {
                fr_prop_appearance_t app;
                ASSERT_TRUE(fr_prop_appearance(&g_asset.prop,f,y*g_asset.prop.width+x,&app));
                if(app.alpha==0) continue;
                ASSERT_TRUE(x>=layout.x&&x<layout.x+layout.width&&y>=layout.y&&y<layout.y+layout.height);
                if(y==layout.y+layout.height-1) bottom_occupied=true;
                uint32_t source=g_mapping[f*g_asset.prop.width*40+y*g_asset.prop.width+x];
                ASSERT_TRUE(source);
                for(uint32_t flip=0;flip<2;flip++) {
                    int crop_x=(int)x-(int)layout.x;
                    int mirrored=flip?(int)layout.width-1-crop_x:crop_x;
                    int pad=flip?(int)layout.canvas_width-(int)layout.x-(int)layout.width:(int)layout.x;
                    int origin=(int)(layout.canvas_width-40)/2-pad;
                    int world_x=mirrored-origin;
                    int expected=flip?(int)layout.canvas_width-1-(int)x-(int)(layout.canvas_width-40)/2:
                        (int)x-(int)(layout.canvas_width-40)/2;
                    ASSERT_INT_EQ(world_x,expected);
                }
            }
            ASSERT_TRUE(bottom_occupied);
            int origin_y=(int)layout.foot_baseline-(int)layout.y-40;
            ASSERT_INT_EQ((int)layout.foot_baseline-(int)layout.y-origin_y,40);
        }
    }
    fr_beaver_frame_layout_t invalid={.width=999};
    ASSERT_TRUE(!fr_beaver_frame_layout(NULL,0,&invalid));ASSERT_EQ(invalid.width,999u);
    PASS();
}

int main(void) {
    RUN(test_runtime_crop_and_stable_foot_contact);
    RUN(test_authoritative_cast_wide_rgba_and_stable_owners);
    RUN(test_authoritative_rgba_pixels_and_unique_material_correspondence);
    RUN(test_standard_prop_pipeline_and_exact_clip_timing);
    RUN(test_every_visible_pixel_has_supported_physics_and_appearance);
    RUN(test_each_requested_frame_has_a_distinct_pose);
    RUN(test_flip_preserves_material_and_canonical_pixel_identity);
    RUN(test_source_owner_coordinates_survive_frames_and_player_motion);
    RUN(test_invalid_inputs_preserve_buffers_and_outputs);
    RUN(test_asset_build_is_deterministic);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
