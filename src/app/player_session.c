#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/player/beaver.h"
#include "ferrum/player/hotbar.h"
#include "ferrum/player/abilities.h"
#include "ferrum/player/chew.h"
#include "ferrum/player/manipulation.h"
#include "ferrum/player/placement.h"
#include "ferrum/renderer/sprite_gpu.h"
#include "../vulkan/runtime_body_gpu.h"
#include "../vulkan/gpu_internal.h"
#include "level_internal.h"
#include "player_session.h"
#include "view_config.h"

#define PLAYER_TRANSACTION UINT64_C(0x4245415600000000)
enum { PLAYER_CAMERA_DEAD_ZONE = 8, PLAYER_CAMERA_MAX_PAN = 1024 };
struct dc_player_session {
    fr_input_t input;
    fr_player_registry_t registry;
    fr_player_definition_t definitions[2];
    fr_player_t players[FR_INPUT_PLAYERS];
    fr_player_control_t controls[FR_INPUT_PLAYERS];
    fr_beaver_body_t bodies[FR_INPUT_PLAYERS];
    double values[FR_INPUT_PLAYERS][2];
    uint8_t abilities[FR_INPUT_PLAYERS][2];
    SDL_GameController *controllers[FR_INPUT_DEVICES];
    int64_t device[FR_INPUT_PLAYERS];
    fr_player_drive_gpu_t *drive;
    fr_fire_ray_gpu_t *ray;
    fr_fire_ray_config_t ray_config;
    bool ray_enabled,follow_camera;
    uint32_t spawn_x,spawn_y;
    uint32_t count,fire,ice,frame_offsets[FR_PLAYER_ACTION_COUNT][8];
    fr_beaver_frame_layout_t layouts[FR_PLAYER_ACTION_COUNT][8];
    uint64_t sequence;
    double milliseconds;
    double pending_action_ms;
    uint64_t cast_pressed_at[FR_INPUT_PLAYERS];
    bool presentation_clock;
    bool editor,trace;
    char failure[256];
    fr_player_action_event_t cast_events[FR_INPUT_PLAYERS];
    fr_player_hotbar_t hotbar[FR_INPUT_PLAYERS];
    fr_player_carry_t carry[FR_INPUT_PLAYERS];
    fr_player_manipulation_t manipulation[FR_INPUT_PLAYERS];
    bool split;
    uint32_t split_count;
    fr_player_inventory_t inventory[FR_INPUT_PLAYERS];
    fr_player_chew_t chew_work[FR_INPUT_PLAYERS];
    bool aim_override[FR_INPUT_PLAYERS];
    fr_player_point_t aim_point[FR_INPUT_PLAYERS];
};
static bool split_refresh(dc_level_view_t *v,char *err,uint32_t cap);
static bool players_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}
static uint32_t players_pack(const fr_prop_appearance_t *a) {
    uint32_t packed=0;
    for(unsigned c=0;c<4;c++) {
        float value=c==3?a->alpha:a->color[c];
        packed|=(uint32_t)lroundf(fminf(1,fmaxf(0,value))*255.f)<<(c*8);
    }
    return packed;
}
static bool players_atlas(dc_level_view_t *v,char *err,uint32_t cap) {
    struct dc_player_session *s=v->players;uint32_t count=0;
    for(unsigned a=0;a<FR_PLAYER_ACTION_COUNT;a++) {
        size_t bytes;uint32_t entries;
        if(!fr_beaver_asset_size((fr_player_action_t)a,&bytes,&entries)) return false;
        count+=entries;
    }
    fr_renderer_sprite_pixel_t *pixels=calloc(count,sizeof(*pixels));
    if(!pixels) return players_error(err,cap,"Cannot stage immutable player atlas");
    bool okay=true;uint32_t used=0;
    for(unsigned a=0;a<FR_PLAYER_ACTION_COUNT&&okay;a++) {
        size_t size;uint32_t entries;fr_beaver_asset_t asset;
        fr_beaver_asset_size((fr_player_action_t)a,&size,&entries);
        uint8_t *bytes=malloc(size);uint32_t *mapping=malloc((size_t)entries*4);
        okay=bytes&&mapping&&fr_beaver_asset_build((fr_player_action_t)a,bytes,size,mapping,entries,&asset,err,cap);
        if(okay) for(uint32_t frame=0;frame<asset.prop.frame_count&&okay;frame++) {
            fr_beaver_frame_layout_t *layout=&s->layouts[a][frame];
            if(!fr_beaver_frame_layout(&asset,frame,layout)) {okay=false;break;}
            s->frame_offsets[a][frame]=used;
            for(uint32_t y=layout->y;y<layout->y+layout->height;y++)
                for(uint32_t x=layout->x;x<layout->x+layout->width;x++) {
                    uint32_t p=y*asset.prop.width+x;fr_prop_appearance_t appearance;
                    if(!fr_prop_appearance(&asset.prop,frame,p,&appearance)) {okay=false;break;}
                    uint32_t source=mapping[frame*asset.prop.width*asset.prop.height+p];
                    pixels[used++]=(fr_renderer_sprite_pixel_t){players_pack(&appearance),source};
                }
        }
        free(bytes);free(mapping);
    }
    if(okay) okay=fr_renderer_sprite_atlas(v->gpu,pixels,used,err,cap);
    free(pixels);return okay;
}
static bool players_publish(dc_level_view_t *v,char *err,uint32_t cap) {
    struct dc_player_session *s=v->players;fr_renderer_sprite_t poses[FR_INPUT_PLAYERS];
    for(uint32_t n=0;n<s->count;n++) {
        uint32_t frame;fr_player_frame(&s->players[n],&frame);
        fr_player_action_t display=s->players[n].action==FR_PLAYER_WALK?FR_PLAYER_IDLE:s->players[n].action;
        if(display==FR_PLAYER_IDLE) frame=0;
        fr_beaver_frame_layout_t layout=s->layouts[display][frame];
        bool flip=s->players[n].facing<0;
        uint32_t horizontal_pad=flip?layout.canvas_width-layout.x-layout.width:layout.x;
        fr_beaver_body_t b=s->bodies[n];
        poses[n]=(fr_renderer_sprite_t){.body_id=b.binding.body_id,.generation=b.binding.generation,
            .offset=s->frame_offsets[display][frame],.width=layout.width,.height=layout.height,
            .flip=flip,.rest_origin_x=b.rest_origin_x,.rest_origin_y=b.rest_origin_y,
            .origin_x=(int32_t)b.rest_origin_x+(int32_t)(layout.canvas_width-40)/2-(int32_t)horizontal_pad,
            .origin_y=(int32_t)layout.foot_baseline-(int32_t)layout.y-(int32_t)b.height};
    }
    return fr_renderer_sprite_poses(v->gpu,poses,s->count,err,cap);
}
static bool players_add(dc_level_view_t *v,int64_t device,char *err,uint32_t cap) {
    struct dc_player_session *s=v->players;uint32_t n=s->count;
    if(n>=FR_INPUT_PLAYERS) return players_error(err,cap,"Local player capacity reached");
    uint64_t transaction=PLAYER_TRANSACTION+n+1;uint32_t found=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(v->gpu);slot++) {
        dc_body_storage_owner_t *o=dc_gpu_body_owner(v->gpu,slot);
        if(o->state==DC_BODY_OWNER_LIVE&&o->source_transaction.low==(uint32_t)transaction&&
           o->source_transaction.high==(uint32_t)(transaction>>32)) {
            s->bodies[n]=(fr_beaver_body_t){.binding={n+1,o->runtime_id,o->generation},.width=40,.height=40};found++;
        }
    }
    if(found>1) return players_error(err,cap,"Saved player has ambiguous body ownership");
    if(!found) {
        dc_chunk_coord_t stage={v->origin.x-1,v->origin.y-1};uint32_t slot;
        if(!dc_chunk_table_find(&v->table,stage,&slot)) return players_error(err,cap,"Player staging chunk is not resident");
        dc_chunk_coord_t spawn;uint32_t x,y;
        dc_cell_address(s->spawn_x+(int64_t)n*64,s->spawn_y,&spawn,&x,&y);
        spawn.x+=v->origin.x;spawn.y+=v->origin.y;
        if(!fr_beaver_body_admit(v->gpu,n+1,transaction,slot,spawn,(int32_t)x*65536,(int32_t)y*65536,&s->bodies[n],err,cap)||
           !dc_chunk_table_mark_dirty(&v->table,slot)) return false;
    }
    if(!fr_player_init(&s->players[n],n+1,&s->registry,s->values[n],s->abilities[n],2)||
       !fr_player_ability_set(&s->players[n],s->fire,true)||!fr_player_ability_set(&s->players[n],s->ice,true)||
       !fr_player_hotbar_put_spell(&s->hotbar[n],1u,s->fire)||!fr_player_hotbar_put_spell(&s->hotbar[n],2u,s->ice)||
       !fr_player_hotbar_put_material(&s->hotbar[n],3u,DC_MATERIAL_STONE)||
       !fr_player_hotbar_put_material(&s->hotbar[n],4u,DC_MATERIAL_WOOD)||
       !fr_input_add_player(&s->input,n+1)||!fr_input_connect(&s->input,device,n+1)||
       !fr_input_sdl_defaults(&s->input,device,device!=FR_INPUT_KEYBOARD_DEVICE)) return players_error(err,cap,"Cannot bind player input owner");
    s->device[n]=device;s->count++;
    fr_player_body_binding_t bindings[FR_INPUT_PLAYERS];
    for(uint32_t p=0;p<s->count;p++) bindings[p]=s->bodies[p].binding;
    fr_player_drive_gpu_t *drive=NULL;
    if(!fr_player_drive_gpu_create(v->gpu,bindings,s->count,1,3,&drive,err,cap)) return false;
    fr_player_drive_gpu_destroy(s->drive);s->drive=drive;
    if(s->ray_enabled) {
        fr_fire_ray_gpu_destroy(s->ray);s->ray=NULL;
        if(!fr_fire_ray_gpu_create(v->gpu,bindings,s->count,s->ray_config,&s->ray,err,cap)) return false;
    }
    return players_publish(v,err,cap);
}
static void players_open(struct dc_player_session *s,int index) {
    if(!SDL_IsGameController(index)) return;
    for(unsigned n=0;n<FR_INPUT_DEVICES;n++) if(!s->controllers[n]) {
        s->controllers[n]=SDL_GameControllerOpen(index);return;
    }
}
void dc_players_destroy(dc_level_view_t *v) {
    if(!v||!v->players) return;
    struct dc_player_session *s=v->players;
    fr_fire_ray_gpu_destroy(s->ray);
    fr_player_drive_gpu_destroy(s->drive);
    for(unsigned n=0;n<FR_INPUT_DEVICES;n++) if(s->controllers[n]) SDL_GameControllerClose(s->controllers[n]);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);free(s);v->players=NULL;
}
bool dc_players_enable_at(dc_level_view_t *v,uint32_t x,uint32_t y,
                          bool follow_camera,char *err,uint32_t cap) {
    if(!v) return false;
    if(v->players) return true;
    struct dc_player_session *s=calloc(1,sizeof(*s));
    if(!s) return players_error(err,cap,"Cannot allocate player session");
    s->follow_camera=follow_camera;
    v->players=s;s->spawn_x=x;s->spawn_y=y;s->trace=getenv("DC_PLAYER_INPUT_TRACE")!=NULL;fr_input_init(&s->input);
    bool okay=SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0&&
        fr_player_registry_init(&s->registry,s->definitions,2)&&
        fr_player_register(&s->registry,"test:fire_bolt",FR_PLAYER_ABILITY,0,&s->fire,err,cap)&&
        fr_player_register(&s->registry,"test:ice_storm",FR_PLAYER_ABILITY,0,&s->ice,err,cap)&&
        fr_player_registry_seal(&s->registry)&&players_atlas(v,err,cap)&&players_add(v,FR_INPUT_KEYBOARD_DEVICE,err,cap)&&
        fr_thermal_gpu_enable(v->gpu,err,cap)&&fr_thermal_gpu_overlay(v->gpu,0)&&
        dc_level_view_set_spring_enabled(v,false)&&dc_level_view_renderer_mode(v,FR_RENDERER_ALBEDO);
    if(!okay) {dc_players_destroy(v);return false;}
    for(int n=0;n<SDL_NumJoysticks();n++) players_open(s,n);
    return true;
}
bool dc_players_enable(dc_level_view_t *v,char *err,uint32_t cap) {
    bool first = v && !v->players;
    if (!dc_players_enable_at(v,64,40,true,err,cap)) return false;
    if (first) {
        puts("A/D moves, Space jumps, C harvests, and Tab opens the editor.");
        puts("Select fire, ice, stone or wood with 1-4. Q casts the selected spell.");
        puts("R places one terrain cell and B places a rigid block at the aim using inventory. G grabs, E drops, and F throws.");
        puts("Controller Start joins. D-pad up/right/down/left selects slots 1-4, X casts the selected spell, and Y casts ice.");
        puts("The right shoulder places terrain and the left shoulder places a rigid block.");
    }
    return true;
}
bool dc_players_fire_ray(dc_level_view_t *v,fr_fire_ray_config_t config,char *err,uint32_t cap) {
    if(!v||!v->players) return false;
    struct dc_player_session *s=v->players;
    if(s->ray_enabled) return true;
    fr_player_body_binding_t bindings[FR_INPUT_PLAYERS];
    for(uint32_t n=0;n<s->count;n++) bindings[n]=s->bodies[n].binding;
    if(!fr_fire_ray_gpu_create(v->gpu,bindings,s->count,config,&s->ray,err,cap)) return false;
    s->ray_config=config;s->ray_enabled=true;return true;
}
bool dc_players_gameplay(const dc_level_view_t *v) {
    return v&&v->players&&!v->players->editor&&v->players->input.players[0].context==FR_INPUT_GAMEPLAY;
}
void dc_players_ui(dc_level_view_t *v,bool active) {
    if(!v||!v->players) return;
    struct dc_player_session *s=v->players;
    for(uint32_t n=0;n<s->count;n++) fr_input_context(&s->input,n+1,active?FR_INPUT_UI:s->editor?FR_INPUT_EDITOR:FR_INPUT_GAMEPLAY);
}
void dc_players_frame_time(dc_level_view_t *v,double seconds) {
    if(!v||!v->players||!isfinite(seconds)||seconds<0) return;
    v->players->presentation_clock=true;
    v->players->pending_action_ms+=seconds*1000.;
}
bool dc_players_event(dc_level_view_t *v,const SDL_Event *e) {
    if(!v||!v->players||!e) return false;
    struct dc_player_session *s=v->players;
    if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode==SDL_SCANCODE_TAB) {
        if(!e->key.repeat) {s->editor=!s->editor;dc_players_ui(v,false);}
        return true;
    }
    if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode==SDL_SCANCODE_F2) return true;
    if(e->type==SDL_CONTROLLERDEVICEADDED) players_open(s,e->cdevice.which);
    if(e->type==SDL_CONTROLLERDEVICEREMOVED) {
        for(uint32_t n=0;n<s->count;n++) if(s->device[n]==e->cdevice.which) s->device[n]=INT64_MIN;
        for(unsigned n=0;n<FR_INPUT_DEVICES;n++) if(s->controllers[n]&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s->controllers[n]))==e->cdevice.which) {
            SDL_GameControllerClose(s->controllers[n]);s->controllers[n]=NULL;
        }
    }
    if(e->type==SDL_CONTROLLERBUTTONDOWN&&e->cbutton.button==SDL_CONTROLLER_BUTTON_START) {
        for(uint32_t n=0;n<s->count;n++) if(s->device[n]==e->cbutton.which) return true;
        for(uint32_t n=1;n<s->count;n++) if(s->device[n]==INT64_MIN) {
            fr_input_connect(&s->input,e->cbutton.which,n+1);fr_input_sdl_defaults(&s->input,e->cbutton.which,true);
            s->device[n]=e->cbutton.which;return true;
        }
        if(s->count<FR_INPUT_PLAYERS) players_add(v,e->cbutton.which,s->failure,sizeof(s->failure));
        return true;
    }
    uint32_t before[FR_INPUT_PLAYERS];
    for(uint32_t n=0;n<s->count;n++) before[n]=s->input.players[n].state.pressed;
    bool handled=fr_input_sdl_event(&s->input,e);
    uint64_t now=SDL_GetTicks64();
    uint64_t stamp=(now&~UINT64_C(0xffffffff))|e->common.timestamp;
    if(stamp>now) stamp=now;
    if(!e->common.timestamp) stamp=now;
    for(uint32_t n=0;n<s->count;n++)
        if((s->input.players[n].state.pressed&~before[n])&((1u<<FR_INPUT_FIRE)|(1u<<FR_INPUT_ICE))) s->cast_pressed_at[n]=stamp;
    if(s->trace&&(e->type==SDL_KEYDOWN||e->type==SDL_KEYUP)&&e->key.keysym.scancode==SDL_SCANCODE_Q) {
        fprintf(stderr,"[player-input] Q %s repeat%u handled%d focused%d context%u pressed0x%x held%.0f\n",
            e->type==SDL_KEYDOWN?"down":"up",e->key.repeat,handled,s->input.focused,
            s->input.players[0].context,s->input.players[0].state.pressed,s->input.players[0].state.values[FR_INPUT_FIRE]);
        fflush(stderr);
    }
    return handled;
}

static double player_view_axis(int64_t chunk, int64_t origin, int32_t position,
                               uint32_t offset) {
    /* Subtract before converting so large origins retain cell precision. */
    double chunk_delta = chunk >= origin
        ? (double)((uint64_t)chunk - (uint64_t)origin)
        : -(double)((uint64_t)origin - (uint64_t)chunk);
    return chunk_delta * DC_CHUNK_SIDE + position / 65536.0 - offset;
}

static int32_t player_camera_pan(double position, uint32_t extent) {
    double delta = position - extent / 2u;
    double distance = fabs(delta) - PLAYER_CAMERA_DEAD_ZONE;
    if (distance <= 0.0) return 0;
    return (int32_t)copysign(fmin(distance, PLAYER_CAMERA_MAX_PAN), delta);
}

static void players_follow_camera(dc_level_view_t *v) {
    if (!v->players->follow_camera || !dc_players_gameplay(v)) return;
    uint32_t id = v->players->bodies[0].binding.body_id;
    dc_gpu_world_body_t body;
    if (!id || !dc_gpu_read_world_body(v->gpu, id, &body, NULL, 0)) return;
    double x = player_view_axis(body.chunk.x, v->origin.x, body.body.x_fp,
                                v->camera_offset_x);
    double y = player_view_axis(body.chunk.y, v->origin.y, body.body.y_fp,
                                v->camera_offset_y);
    int32_t dx = player_camera_pan(x, v->gpu->view_width);
    int32_t dy = player_camera_pan(y, v->gpu->view_height);
    if (dx || dy) dc_level_view_pan_pixels(v, dx, dy);
}

static fr_player_hotbar_item_t players_select_item(fr_player_hotbar_t *bar,
                                                  uint32_t pressed) {
    for (uint32_t slot = 0; slot < FR_PLAYER_HOTBAR_SLOTS; ++slot)
        if (pressed & (1u << (FR_INPUT_SLOT_1 + slot)))
            fr_player_hotbar_select(bar, slot + 1u);
    fr_player_hotbar_item_t item = {0};
    fr_player_hotbar_selected_item(bar, &item);
    return item;
}

static bool players_place_item(dc_level_view_t *v, fr_player_inventory_t *inventory,
                                const fr_input_state_t *input,
                                const fr_player_hotbar_item_t *item,
                                char *err, uint32_t cap) {
    bool terrain = (input->pressed & (1u << FR_INPUT_PLACE)) != 0u;
    bool rigid = (input->pressed & (1u << FR_INPUT_BUILD)) != 0u;
    if (!input->has_target || item->kind != FR_SLOT_MATERIAL || !(terrain || rigid))
        return true;
    bool placed;
    bool okay = fr_player_place_material(v->gpu, inventory, &input->target,
        (uint16_t)item->material, !terrain, &placed, err, cap);
    uint32_t slot;
    if (placed && terrain && dc_chunk_table_find(&v->table, input->target.chunk, &slot))
        dc_chunk_table_mark_dirty(&v->table, slot);
    return okay;
}

bool dc_players_step(dc_level_view_t *v,float seconds,char *err,uint32_t cap) {
    if(!v||!v->players||!isfinite(seconds)||seconds<=0||seconds>.25f) return false;
    struct dc_player_session *s=v->players;
    if(s->failure[0]) return players_error(err,cap,s->failure);
    s->milliseconds+=s->presentation_clock?s->pending_action_ms:(double)seconds*1000.;
    s->pending_action_ms=0;
    uint64_t elapsed=(uint64_t)s->milliseconds;s->milliseconds-=elapsed;
    fr_player_drive_input_t drive[FR_INPUT_PLAYERS];
    for(uint32_t n=0;n<s->count;n++) {
        fr_input_state_t input;fr_input_sample(&s->input,n+1,&input);
        const uint32_t edges = input.pressed;
        fr_player_hotbar_item_t item = players_select_item(&s->hotbar[n], edges);
        uint32_t selected_spell = s->ray ? s->fire :
            item.kind == FR_SLOT_SPELL ? item.ability_id : 0u;
        bool cast_pressed = (edges & (1u << FR_INPUT_FIRE)) != 0u
            ? selected_spell != 0u : (edges & (1u << FR_INPUT_ICE)) != 0u;
        int mx=0,my=0;uint32_t x=0,y=0;bool valid=false;
        if(n==0) {
            SDL_GetMouseState(&mx,&my);
            valid=mx>=0&&my>=0&&dc_level_view_screen_cell(v,(uint32_t)mx,(uint32_t)my,&x,&y);
        } else {
            x=(uint32_t)((float)VIEW_WIDTH*.5f+input.values[FR_INPUT_AIM_X]*80.f);
            y=(uint32_t)((float)VIEW_HEIGHT*.5f+input.values[FR_INPUT_AIM_Y]*80.f);valid=true;
        }
        bool can_aim=s->input.focused&&s->input.players[n].context==FR_INPUT_GAMEPLAY;
        valid=valid&&can_aim;
        dc_chunk_coord_t chunk;uint32_t lx,ly;
        dc_cell_address((int64_t)x+v->camera_offset_x,(int64_t)y+v->camera_offset_y,&chunk,&lx,&ly);
        chunk.x+=v->origin.x;chunk.y+=v->origin.y;
        input.has_target=valid;input.target=(fr_player_point_t){chunk,(float)lx,(float)ly};
        if(s->aim_override[n]&&can_aim) {input.has_target=true;input.target=s->aim_point[n];}
        fr_player_action_t old_action=s->players[n].action;
        uint64_t action_elapsed=elapsed;
        if(s->presentation_clock&&old_action!=FR_PLAYER_CAST&&cast_pressed) {
            uint64_t now=SDL_GetTicks64(),age=now>=s->cast_pressed_at[n]?now-s->cast_pressed_at[n]:0;
            if(age<action_elapsed) action_elapsed=age;
        }
        dc_gpu_world_body_t self;
        bool has_body=s->bodies[n].binding.body_id&&
            dc_gpu_read_world_body(v->gpu,s->bodies[n].binding.body_id,&self,err,cap);
        bool harvestable=false;
        bool chew_held=input.values[FR_INPUT_CHEW]>0.f;
        if(chew_held||s->chew_work[n].bank.work) {
            if(has_body&&input.has_target) {
                fr_player_point_t origin={self.chunk,self.body.x_fp/65536.f,self.body.y_fp/65536.f};
                bool casting=old_action==FR_PLAYER_CAST||cast_pressed;
                if(!fr_player_harvest_tick(v->gpu,&s->inventory[n],&s->chew_work[n],
                    &origin,&input.target,chew_held&&!casting,elapsed,&harvestable,err,cap))
                    return false;
            } else s->chew_work[n]=(fr_player_chew_t){0};
        }
        if(!fr_player_control_tick(&s->controls[n],&s->players[n],&input,selected_spell,s->ice,harvestable,action_elapsed,&s->cast_events[n])) return false;
        if(s->trace&&(old_action!=s->players[n].action||s->cast_events[n].ability)) {
            uint32_t frame;fr_player_frame(&s->players[n],&frame);
            fprintf(stderr,"[player-action] wall%llu player%u action%u frame%u elapsed%llu event%u facing%d ray_enabled%d\n",
                (unsigned long long)SDL_GetTicks64(),n+1,s->players[n].action,frame+1,
                (unsigned long long)s->players[n].elapsed_ms,s->cast_events[n].ability,s->players[n].facing,s->ray!=NULL);fflush(stderr);
        }
        if(!s->manipulation[n].carry_weight)
            s->manipulation[n]=(fr_player_manipulation_t){.carry_weight=60.0,.throw_force=4.0,.reach=24.0};
        if(has_body) {
            float px=(float)self.chunk.x*64.f+self.body.x_fp/65536.f;
            float py=(float)self.chunk.y*64.f+self.body.y_fp/65536.f;
            /* Sandbox ability dispatch: the fire ability launches a burning
             * bolt and the ice ability casts a bounded storm at the target.
             * Ray-equipped demo scenes keep their beam behaviour. */
            const fr_player_action_event_t *cast=&s->cast_events[n];
            if(!s->ray&&cast->ability&&cast->has_target&&can_aim) {
                fr_player_point_t muzzle={self.chunk,self.body.x_fp/65536.f,self.body.y_fp/65536.f};
                if(cast->ability==s->fire)
                    fr_player_cast_fire_bolt(v->gpu,s->players[n].identity,&muzzle,&cast->target,NULL,err,cap);
                else if(cast->ability==s->ice)
                    fr_player_cast_ice_storm(v->gpu,&cast->target,6u,err,cap);
            }
            bool acted=false;
            if(edges&(1u<<FR_INPUT_GRAB)) {fr_player_manipulation_grab(v->gpu,&s->carry[n],&s->manipulation[n],px,py,&input.target,err,cap);acted=true;}
            else if(edges&(1u<<FR_INPUT_DROP)) {fr_player_manipulation_drop(v->gpu,&s->carry[n],err,cap);acted=true;}
            else if(edges&(1u<<FR_INPUT_THROW)) {fr_player_manipulation_throw(v->gpu,&s->carry[n],&s->manipulation[n],px,py,&input.target,err,cap);acted=true;}
            if(!acted&&!fr_player_manipulation_tick(v->gpu,&s->carry[n],px,py,err,cap)) return false;
            if (!players_place_item(v, &s->inventory[n], &input, &item, err, cap))
                return false;
        }
        drive[n]=(fr_player_drive_input_t){n+1,s->sequence+1,s->players[n].intent.move_x,s->players[n].intent.jump_pressed};
    }
    if(s->sequence==UINT64_MAX) return players_error(err,cap,"Player command sequence exhausted");
    s->sequence++;
    if(!fr_player_drive_gpu_submit(s->drive,drive,s->count,err,cap)||!players_publish(v,err,cap)) return false;
    if(s->split&&!split_refresh(v,err,cap)) return false;
    players_follow_camera(v);
    if(s->ray) {
        fr_fire_ray_input_t rays[FR_INPUT_PLAYERS];
        for(uint32_t n=0;n<s->count;n++) rays[n]=(fr_fire_ray_input_t){n+1,s->sequence,s->players[n].facing,s->cast_events[n].ability==s->fire};
        if(!fr_fire_ray_gpu_submit(s->ray,rays,s->count,seconds,err,cap)) return false;
    }
    return true;
}
bool dc_players_status(dc_level_view_t *v,uint32_t n,fr_player_t *p,fr_beaver_body_t *b) {
    if(!v||!v->players||n>=v->players->count||!p||!b) return false;
    *p=v->players->players[n];*b=v->players->bodies[n];return true;
}
bool dc_players_aim_override(dc_level_view_t *v,uint32_t n,
                             const fr_player_point_t *target,char *err,uint32_t cap) {
    if(!v||!v->players||n>=FR_INPUT_PLAYERS||!target) return false;
    struct dc_player_session *s=v->players;
    if(n>=s->count) return players_error(err,cap,"Aim override needs a joined player");
    s->aim_override[n]=true;
    s->aim_point[n]=*target;
    return true;
}

bool dc_players_inventory(dc_level_view_t *v,uint32_t n,
                          fr_player_inventory_t *out) {
    if(!v||!v->players||!out||n>=v->players->count) return false;
    *out=v->players->inventory[n];
    return true;
}

bool dc_players_join(dc_level_view_t *v,int64_t device,char *err,uint32_t cap) {
    if(!v||!v->players) return players_error(err,cap,"Player session is absent");
    struct dc_player_session *s=v->players;
    for(uint32_t n=1;n<s->count;n++) if(s->device[n]==device) return true;
    if(!s->count) return players_error(err,cap,"The keyboard player must enable first");
    if(s->count>=FR_INPUT_PLAYERS) return players_error(err,cap,"Local player capacity reached");
    return players_add(v,device,err,cap);
}

/* Recompute each viewport crop around its own beaver; presentation only,
 * so the shared world still steps exactly once per tick. */
static bool split_refresh(dc_level_view_t *v,char *err,uint32_t cap) {
    struct dc_player_session *s=v->players;
    uint32_t count=s->split_count;
    if(!count) return true;
    dc_presentation_focus_t crops[FR_INPUT_PLAYERS];
    uint32_t grid_w=v->gpu->view_width,grid_h=v->gpu->view_height;
    uint32_t view_w=count>=3u?grid_w/2u:grid_w,
             view_h=count>=3u?grid_h/2u:grid_h;
    uint32_t active=0u;
    for(uint32_t n=0;n<s->count&&active<count;n++) {
        dc_gpu_world_body_t self;
        if(!dc_gpu_read_world_body(v->gpu,s->bodies[n].binding.body_id,&self,err,cap))
            continue;
        double px=player_view_axis(self.chunk.x,v->origin.x,self.body.x_fp,
                                   v->camera_offset_x);
        double py=player_view_axis(self.chunk.y,v->origin.y,self.body.y_fp,
                                   v->camera_offset_y);
        uint32_t cx=(uint32_t)fmin(fmax(px-view_w/2u,0.0),grid_w-view_w);
        uint32_t cy=(uint32_t)fmin(fmax(py-view_h/2u,0.0),grid_h-view_h);
        crops[active++]=(dc_presentation_focus_t){cx,cy,view_w,view_h};
    }
    if(active!=count) return players_error(err,cap,"Split screen needs every viewport player admitted");
    return dc_gpu_set_split_presentation(v->gpu,count,crops);
}

bool dc_players_split_screen(dc_level_view_t *v,uint32_t count,char *err,uint32_t cap) {
    if(!v||!v->players||count>FR_INPUT_PLAYERS||(count&&count<2u))
        return players_error(err,cap,"Split screen needs two to four joined players");
    struct dc_player_session *s=v->players;
    s->split=count!=0u;
    s->split_count=count;
    if(!count) return dc_gpu_set_split_presentation(v->gpu,0u,NULL);
    return split_refresh(v,err,cap);
}
