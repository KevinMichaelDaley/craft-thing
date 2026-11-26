#include <string.h>

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

typedef struct {
    dc_level_view_t *view;
    fr_player_t player;
    double values[2];
    uint8_t abilities[2];
    fr_player_hotbar_t hotbar;
    fr_player_drive_gpu_t *drive;
    fr_renderer_sprite_t poses[FR_INPUT_PLAYERS];
} player_join_t;

static bool players_prepare_gpu(void *user,const fr_beaver_body_t *body,char *err,uint32_t cap) {
    player_join_t *join=user;
    dc_level_view_t *v=join->view;
    const struct dc_player_session *s=v->players;
    uint32_t n=s->count;
    fr_player_body_binding_t bindings[FR_INPUT_PLAYERS];
    for(uint32_t i=0;i<n;i++) {
        bindings[i]=s->bodies[i].binding;
        if(!dc_player_session_make_pose(s,&s->players[i],&s->bodies[i],&join->poses[i],err,cap))
            return false;
    }
    bindings[n]=body->binding;
    return dc_player_session_make_pose(s,&join->player,body,&join->poses[n],err,cap)&&
        fr_player_drive_gpu_create(v->gpu,bindings,n+1u,1.f,3.f,&join->drive,err,cap)&&
        (!s->ray_enabled||fr_fire_ray_gpu_prepare_emitter(s->ray,&body->binding,err,cap))&&
        fr_renderer_sprite_prepare_poses(v->gpu,join->poses,n+1u,err,cap);
}

static bool players_prepare_body(player_join_t *join,fr_beaver_body_t *body,uint32_t *staging_slot,
    bool *world_consistent,char *err,uint32_t cap) {
    dc_level_view_t *v=join->view;
    uint32_t seat=v->players->count;
    uint64_t transaction=DC_PLAYER_TRANSACTION_BASE+seat+1u;
    uint32_t found=0;
    *staging_slot=UINT32_MAX;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(v->gpu);slot++) {
        const dc_body_storage_owner_t *owner=dc_gpu_body_owner(v->gpu,slot);
        if(owner->state==DC_BODY_OWNER_LIVE&&owner->source_transaction.low==(uint32_t)transaction&&
           owner->source_transaction.high==(uint32_t)(transaction>>32)) {
            *body=(fr_beaver_body_t){.binding={seat+1u,owner->runtime_id,owner->generation},
                .width=FR_BEAVER_WIDTH,.height=FR_BEAVER_HEIGHT};
            found++;
        }
    }
    if(found>1) return players_error(err,cap,"Saved player has ambiguous body ownership");
    if(found) return players_prepare_gpu(join,body,err,cap);
    dc_chunk_coord_t stage={v->origin.x-1,v->origin.y-1};
    if(!dc_chunk_table_find(&v->table,stage,staging_slot))
        return players_error(err,cap,"Player staging chunk is not resident");
    const dc_chunk_slot_t *slot=&v->table.slots[*staging_slot];
    if(slot->state!=DC_SLOT_ACTIVE&&slot->state!=DC_SLOT_SLEEPING)
        return players_error(err,cap,"Player staging chunk is not ready");
    dc_chunk_coord_t spawn;uint32_t x,y;
    dc_cell_address(v->players->spawn_x+(int64_t)seat*DC_CHUNK_SIDE,v->players->spawn_y,&spawn,&x,&y);
    spawn.x+=v->origin.x;spawn.y+=v->origin.y;
    return fr_beaver_body_admit_prepared(v->gpu,seat+1u,transaction,*staging_slot,spawn,
        (int32_t)x*DC_FLUID_FULL,(int32_t)y*DC_FLUID_FULL,players_prepare_gpu,join,body,
        world_consistent,err,cap);
}

static bool players_add(dc_level_view_t *v,int64_t device,const fr_input_t *prepared,
                        bool *world_consistent,char *err,uint32_t cap) {
    struct dc_player_session *s=v->players;uint32_t n=s->count;
    *world_consistent=true;
    if(n>=FR_INPUT_PLAYERS) return players_error(err,cap,"Local player capacity reached");
    player_join_t join={.view=v};
    if(!fr_player_init(&join.player,n+1u,&s->registry,join.values,join.abilities,2u)||
       !fr_player_ability_set(&join.player,s->fire,true)||!fr_player_ability_set(&join.player,s->ice,true)||
       !fr_player_hotbar_put_spell(&join.hotbar,1u,s->fire)||!fr_player_hotbar_put_spell(&join.hotbar,2u,s->ice)||
       !fr_player_hotbar_put_material(&join.hotbar,3u,DC_MATERIAL_STONE)||
       !fr_player_hotbar_put_material(&join.hotbar,4u,DC_MATERIAL_WOOD))
        return players_error(err,cap,"Cannot initialize player abilities and hotbar");
    fr_beaver_body_t body;
    uint32_t staging_slot;
    if(!players_prepare_body(&join,&body,&staging_slot,world_consistent,err,cap)) {
        fr_player_drive_gpu_destroy(join.drive);
        return false;
    }

    /* Body admission is complete. Publish the prepared owners before the next frame. */
    if(staging_slot!=UINT32_MAX) dc_chunk_table_mark_dirty(&v->table,staging_slot);
    s->input=*prepared;
    memcpy(s->values[n],join.values,sizeof(join.values));
    memcpy(s->abilities[n],join.abilities,sizeof(join.abilities));
    s->players[n]=join.player;
    s->players[n].values=s->values[n];
    s->players[n].abilities=s->abilities[n];
    s->bodies[n]=body;
    s->hotbar[n]=join.hotbar;
    s->manipulation[n]=(fr_player_manipulation_t){.carry_weight=60.,.throw_force=4.,.reach=24.};
    s->device[n]=device;
    fr_player_drive_gpu_t *previous=s->drive;
    s->drive=join.drive;
    if(s->ray_enabled) fr_fire_ray_gpu_commit_emitter(s->ray,&body.binding);
    fr_renderer_sprite_commit_poses(v->gpu,join.poses,n+1u);
    s->count=n+1u;
    fr_player_drive_gpu_destroy(previous);
    return true;
}

bool dc_player_session_add_keyboard(dc_level_view_t *v,char *err,uint32_t cap) {
    fr_input_t prepared;
    bool world_consistent;
    return players_prepare_input(v->players,0u,FR_INPUT_KEYBOARD_DEVICE,&prepared,err,cap)&&
        players_add(v,FR_INPUT_KEYBOARD_DEVICE,&prepared,&world_consistent,err,cap);
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
    bool world_consistent;
    bool joined=players_add(v,device,&prepared,&world_consistent,err,cap);
    *backend_failed=!world_consistent;
    return joined;
}

bool dc_players_join(dc_level_view_t *v,int64_t device,char *err,uint32_t cap) {
    bool backend_failed;
    return dc_player_session_join_controller(v,device,&backend_failed,err,cap);
}
