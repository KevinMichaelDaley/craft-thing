#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/player/player.h"

static bool player_registry_valid(const fr_player_registry_t *r) {
    return r && r->count<=r->capacity && (!r->capacity || r->definitions);
}

static bool player_kind_valid(fr_player_definition_kind_t kind) {
    return kind>=FR_PLAYER_ATTRIBUTE && kind<=FR_PLAYER_ABILITY;
}

static bool player_key_valid(const char *key) {
    if (!key) return false;
    bool colon=false;
    for (uint32_t i=0;i<FR_PLAYER_KEY_CAP;i++) {
        unsigned char ch=(unsigned char)key[i];
        if (!ch) return i>0 && colon && key[i-1]!=':';
        if (ch==':') {
            if (!i || colon) return false;
            colon=true;
        } else if (!((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') ||
                     (ch>='0' && ch<='9') || ch=='_' || ch=='-' || ch=='.')) return false;
    }
    return false;
}

static bool player_registry_error(char *err,uint32_t cap,const char *reason) {
    if (err && cap) snprintf(err,cap,"%s",reason);
    return false;
}

bool fr_player_registry_init(fr_player_registry_t *r,fr_player_definition_t *storage,uint32_t cap) {
    if (!r || (cap && !storage)) return false;
    *r=(fr_player_registry_t){.definitions=storage,.capacity=cap};
    return true;
}

bool fr_player_register(fr_player_registry_t *r,const char *key,
    fr_player_definition_kind_t kind,double initial,uint32_t *out,char *err,uint32_t cap) {
    if (!player_registry_valid(r) || r->sealed || !out || !player_kind_valid(kind) ||
        !player_key_valid(key) || !isfinite(initial) || (kind==FR_PLAYER_ABILITY && initial!=0))
        return player_registry_error(err,cap,"Invalid player definition or sealed registry");
    if (r->count==r->capacity)
        return player_registry_error(err,cap,"Player definition storage exhausted");
    for (uint32_t i=0;i<r->count;i++)
        if (!strcmp(r->definitions[i].key,key))
            return player_registry_error(err,cap,"Duplicate player definition key");
    fr_player_definition_t definition={.kind=kind,.initial_value=initial};
    memcpy(definition.key,key,strlen(key)+1);
    r->definitions[r->count]=definition;
    *out=++r->count;
    if (err && cap) err[0]=0;
    return true;
}

bool fr_player_registry_seal(fr_player_registry_t *r) {
    if (!player_registry_valid(r)) return false;
    r->sealed=true;
    return true;
}

uint32_t fr_player_find(const fr_player_registry_t *r,const char *key,
    fr_player_definition_kind_t kind) {
    if (!player_registry_valid(r) || !player_key_valid(key) || !player_kind_valid(kind)) return 0;
    for (uint32_t i=0;i<r->count;i++)
        if (r->definitions[i].kind==kind && !strcmp(r->definitions[i].key,key)) return i+1;
    return 0;
}
