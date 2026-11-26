#include "../vulkan/runtime_body_gpu.h"
#include "level_internal.h"
#include "player_session_internal.h"

static bool players_prepare_input(const struct dc_player_session *s,uint32_t seat,int64_t device,
                                  fr_input_t *prepared,char *err,uint32_t cap) {
    *prepared=s->input;
    uint64_t identity=seat+1u;
    if(seat==s->count&&(!fr_input_add_player(prepared,identity)||
       !fr_input_context(prepared,identity,s->input.players[0].context)))
        return players_error(err,cap,"Cannot register player input owner");
    if(!fr_input_connect(prepared,device,identity)||
       !fr_input_sdl_defaults(prepared,device,device!=FR_INPUT_KEYBOARD_DEVICE))
        return players_error(err,cap,"Cannot bind player input owner");
    return true;
}

static bool players_add(dc_level_view_t *v,int64_t device,const fr_input_t *prepared,char *err,uint32_t cap) {
    struct dc_player_session *s=v->players;uint32_t n=s->count;
    if(n>=FR_INPUT_PLAYERS) return players_error(err,cap,"Local player capacity reached");
    uint64_t transaction=DC_PLAYER_TRANSACTION_BASE+n+1u;uint32_t found=0;
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
       !fr_player_hotbar_put_material(&s->hotbar[n],4u,DC_MATERIAL_WOOD))
        return players_error(err,cap,"Cannot initialize player abilities and hotbar");
    s->input=*prepared;
    s->manipulation[n] = (fr_player_manipulation_t){.carry_weight=60., .throw_force=4., .reach=24.};
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
    return dc_player_session_publish(v,err,cap);
}

bool dc_player_session_add_keyboard(dc_level_view_t *v,char *err,uint32_t cap) {
    fr_input_t prepared;
    return players_prepare_input(v->players,0u,FR_INPUT_KEYBOARD_DEVICE,&prepared,err,cap)&&
        players_add(v,FR_INPUT_KEYBOARD_DEVICE,&prepared,err,cap);
}

bool dc_player_session_join_controller(dc_level_view_t *v,int64_t device,bool *backend_failed,
                                     char *err,uint32_t cap) {
    *backend_failed=false;
    if(!v||!v->players) return players_error(err,cap,"Player session is absent");
    if(device==FR_INPUT_KEYBOARD_DEVICE||device==INT64_MIN)
        return players_error(err,cap,"Reserved controller device ID");
    struct dc_player_session *s=v->players;
    if(!s->count) return players_error(err,cap,"The keyboard player must enable first");
    uint32_t seat=s->count;
    for(uint32_t n=1u;n<s->count;n++) {
        if(s->device[n]==device) return true;
        if(s->device[n]==INT64_MIN&&seat==s->count) seat=n;
    }
    if(seat>=FR_INPUT_PLAYERS) return players_error(err,cap,"Local player capacity reached");
    fr_input_t prepared;
    if(!players_prepare_input(s,seat,device,&prepared,err,cap)) return false;
    if(seat<s->count) {
        s->input=prepared;
        s->device[seat]=device;
        return true;
    }
    bool joined=players_add(v,device,&prepared,err,cap);
    *backend_failed=!joined;
    return joined;
}

bool dc_players_join(dc_level_view_t *v,int64_t device,char *err,uint32_t cap) {
    bool backend_failed;
    return dc_player_session_join_controller(v,device,&backend_failed,err,cap);
}
