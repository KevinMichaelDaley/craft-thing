#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/player/beaver.h"
#include "ferrum/renderer/sprite_gpu.h"
#include "../vulkan/runtime_body_gpu.h"
#include "level_internal.h"
#include "player_session.h"
#include "view_config.h"

#define PLAYER_TRANSACTION UINT64_C(0x4245415600000000)
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
    uint32_t count,fire,ice,frame_offsets[FR_PLAYER_ACTION_COUNT][8];
    fr_beaver_frame_layout_t layouts[FR_PLAYER_ACTION_COUNT][8];
    uint64_t sequence;
    double milliseconds;
    bool editor;
    char failure[256];
    fr_player_action_event_t cast_events[FR_INPUT_PLAYERS];
};
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
        dc_cell_address(64+(int64_t)n*64,40,&spawn,&x,&y);
        spawn.x+=v->origin.x;spawn.y+=v->origin.y;
        if(!fr_beaver_body_admit(v->gpu,n+1,transaction,slot,spawn,(int32_t)x*65536,(int32_t)y*65536,&s->bodies[n],err,cap)||
           !dc_chunk_table_mark_dirty(&v->table,slot)) return false;
    }
    if(!fr_player_init(&s->players[n],n+1,&s->registry,s->values[n],s->abilities[n],2)||
       !fr_player_ability_set(&s->players[n],s->fire,true)||!fr_player_ability_set(&s->players[n],s->ice,true)||
       !fr_input_add_player(&s->input,n+1)||!fr_input_connect(&s->input,device,n+1)||
       !fr_input_sdl_defaults(&s->input,device,device!=FR_INPUT_KEYBOARD_DEVICE)) return players_error(err,cap,"Cannot bind player input owner");
    s->device[n]=device;s->count++;
    fr_player_body_binding_t bindings[FR_INPUT_PLAYERS];
    for(uint32_t p=0;p<s->count;p++) bindings[p]=s->bodies[p].binding;
    fr_player_drive_gpu_t *drive=NULL;
    if(!fr_player_drive_gpu_create(v->gpu,bindings,s->count,1,3,&drive,err,cap)) return false;
    fr_player_drive_gpu_destroy(s->drive);s->drive=drive;
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
    fr_player_drive_gpu_destroy(s->drive);
    for(unsigned n=0;n<FR_INPUT_DEVICES;n++) if(s->controllers[n]) SDL_GameControllerClose(s->controllers[n]);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);free(s);v->players=NULL;
}
bool dc_players_enable(dc_level_view_t *v,char *err,uint32_t cap) {
    if(!v) return false;
    if(v->players) return true;
    struct dc_player_session *s=calloc(1,sizeof(*s));
    if(!s) return players_error(err,cap,"Cannot allocate player session");
    v->players=s;fr_input_init(&s->input);
    bool okay=SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0&&
        fr_player_registry_init(&s->registry,s->definitions,2)&&
        fr_player_register(&s->registry,"test:fire_bolt",FR_PLAYER_ABILITY,0,&s->fire,err,cap)&&
        fr_player_register(&s->registry,"test:ice_storm",FR_PLAYER_ABILITY,0,&s->ice,err,cap)&&
        fr_player_registry_seal(&s->registry)&&players_atlas(v,err,cap)&&players_add(v,FR_INPUT_KEYBOARD_DEVICE,err,cap)&&
        fr_thermal_gpu_enable(v->gpu,err,cap)&&fr_thermal_gpu_overlay(v->gpu,0)&&
        dc_level_view_set_spring_enabled(v,false)&&dc_level_view_renderer_mode(v,FR_RENDERER_ALBEDO);
    if(!okay) {dc_players_destroy(v);return false;}
    for(int n=0;n<SDL_NumJoysticks();n++) players_open(s,n);
    printf("Play: A/D move, Space jump, C chew, Q fire, E ice; mouse aim. Tab editor. Controller Start joins; left stick/A/B/X/Y/right stick.\n");
    return true;
}
bool dc_players_gameplay(const dc_level_view_t *v) {
    return v&&v->players&&!v->players->editor&&v->players->input.players[0].context==FR_INPUT_GAMEPLAY;
}
void dc_players_ui(dc_level_view_t *v,bool active) {
    if(!v||!v->players) return;
    struct dc_player_session *s=v->players;
    for(uint32_t n=0;n<s->count;n++) fr_input_context(&s->input,n+1,active?FR_INPUT_UI:s->editor?FR_INPUT_EDITOR:FR_INPUT_GAMEPLAY);
}
bool dc_players_event(dc_level_view_t *v,const SDL_Event *e) {
    if(!v||!v->players||!e) return false;
    struct dc_player_session *s=v->players;
    if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode==SDL_SCANCODE_TAB) {
        if(!e->key.repeat) {s->editor=!s->editor;dc_players_ui(v,false);}
        return true;
    }
    if(e->type==SDL_KEYDOWN&&(e->key.keysym.scancode==SDL_SCANCODE_R||e->key.keysym.scancode==SDL_SCANCODE_F2)) return true;
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
    return fr_input_sdl_event(&s->input,e);
}
bool dc_players_step(dc_level_view_t *v,float seconds,char *err,uint32_t cap) {
    if(!v||!v->players||!isfinite(seconds)||seconds<=0||seconds>.25f) return false;
    struct dc_player_session *s=v->players;
    if(s->failure[0]) return players_error(err,cap,s->failure);
    s->milliseconds+=(double)seconds*1000.;uint64_t elapsed=(uint64_t)s->milliseconds;s->milliseconds-=elapsed;
    fr_player_drive_input_t drive[FR_INPUT_PLAYERS];
    for(uint32_t n=0;n<s->count;n++) {
        fr_input_state_t input;fr_input_sample(&s->input,n+1,&input);
        int mx=0,my=0;uint32_t x=0,y=0;bool valid=false;
        if(n==0) {
            SDL_GetMouseState(&mx,&my);
            valid=mx>=0&&my>=0&&dc_level_view_screen_cell(v,(uint32_t)mx,(uint32_t)my,&x,&y);
        } else {
            x=(uint32_t)((float)VIEW_WIDTH*.5f+input.values[FR_INPUT_AIM_X]*80.f);
            y=(uint32_t)((float)VIEW_HEIGHT*.5f+input.values[FR_INPUT_AIM_Y]*80.f);valid=true;
        }
        valid=valid&&s->input.focused&&s->input.players[n].context==FR_INPUT_GAMEPLAY;
        dc_chunk_coord_t chunk;uint32_t lx,ly;
        dc_cell_address((int64_t)x+v->camera_offset_x,(int64_t)y+v->camera_offset_y,&chunk,&lx,&ly);
        chunk.x+=v->origin.x;chunk.y+=v->origin.y;
        input.has_target=valid;input.target=(fr_player_point_t){chunk,(float)lx,(float)ly};
        if(!fr_player_control_tick(&s->controls[n],&s->players[n],&input,s->fire,s->ice,false,elapsed,&s->cast_events[n])) return false;
        drive[n]=(fr_player_drive_input_t){n+1,s->sequence+1,s->players[n].intent.move_x,s->players[n].intent.jump_pressed};
    }
    if(s->sequence==UINT64_MAX) return players_error(err,cap,"Player command sequence exhausted");
    s->sequence++;
    return fr_player_drive_gpu_submit(s->drive,drive,s->count,err,cap)&&players_publish(v,err,cap);
}
bool dc_players_status(dc_level_view_t *v,uint32_t n,fr_player_t *p,fr_beaver_body_t *b) {
    if(!v||!v->players||n>=v->players->count||!p||!b) return false;
    *p=v->players->players[n];*b=v->players->bodies[n];return true;
}
