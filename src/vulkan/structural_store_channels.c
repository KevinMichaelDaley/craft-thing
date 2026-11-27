#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "reactive_storage_gpu.h"
#include "structural_store_channels.h"

_Static_assert(sizeof(double) == 8 && FLT_RADIX == 2 && DBL_MANT_DIG == 53 &&
               DBL_MAX_EXP == 1024, "Structure clocks use IEEE binary64 seconds");
enum { OWNER_RECORD_BYTES = 16u };

static void put64(uint8_t *p,uint64_t value) {for(uint32_t i=0;i<8;i++) p[i]=(uint8_t)(value>>(8*i));}
static uint64_t get64(const uint8_t *p) {uint64_t value=0;for(uint32_t i=0;i<8;i++) value|=(uint64_t)p[i]<<(8*i);return value;}
static uint64_t hash_bytes(uint64_t hash,const void *source,size_t count) {
    const uint8_t *bytes=source;
    for(size_t i=0;i<count;i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
    return hash;
}
void dc_checkpoint_channels_discard(dc_gpu_t *g,dc_checkpoint_channels_t *p) {
    if(!p) return;
    fr_thermal_checkpoint_discard(g,p->prepared_thermal);
    fr_gpu_reactive_storage_destroy(g,p->prepared_reactive);
    fr_gpu_revolute_discard(g,p->prepared_joints);
    if(p->prepared_handles) {
        for(uint32_t i=0;i<p->reactive.chunk_count;i++) free(p->prepared_handles[i]);
        free(p->prepared_handles);
    }
    fr_reactive_archive_release(&p->reactive);fr_thermal_checkpoint_release(&p->thermal);
    free(p->reactive_bytes);free(p->thermal_bytes);free(p->joint_bytes);
    free(p->fluid_bytes);dc_checkpoint_fluid_destroy(p->fluids, p->fluid_count);
    free(p->owner_bytes);free(p->owners);
    free(p->time_bytes);free(p->chunk_times);memset(p,0,sizeof(*p));
}
bool dc_checkpoint_channels_collect(dc_gpu_t *g,const dc_chunk_t *chunks,const uint32_t *slots,uint32_t count,
    dc_checkpoint_channels_t *out,char *err,uint32_t cap) {
    dc_checkpoint_channels_t value={.version=fr_gpu_revolute_span(g)?9u:8u};bool okay=true;
    if(g->reactive) okay=fr_gpu_reactive_checkpoint_collect(g,chunks,slots,count,&value.reactive,err,cap)&&
        fr_reactive_archive_encode(&value.reactive,chunks,count,&value.reactive_bytes,&value.reactive_length,err,cap);
    if(okay&&g->thermal) okay=fr_thermal_checkpoint_collect(g,slots,count,&value.thermal,err,cap)&&
        fr_thermal_checkpoint_encode(&value.thermal,chunks,count,&value.thermal_bytes,&value.thermal_length,err,cap);
    if(okay) okay=fr_gpu_revolute_collect(g,&value.joint_bytes,&value.joint_length,err,cap);
    if(!okay) {dc_checkpoint_channels_discard(g,&value);return false;}
    *out=value;return true;
}
bool dc_checkpoint_channels_set_times(dc_checkpoint_channels_t *p,
                                      const dc_chunk_time_t *times,uint32_t count) {
    if (!times) return true;
    if (count > DC_GPU_CHUNK_SLOTS * 2u) return false;
    bool known = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!times[i].known) continue;
        if (!isfinite(times[i].seconds) || times[i].seconds < 0.0) return false;
        known = true;
    }
    if (!known) return true;
    const size_t length = (size_t)count * sizeof(uint64_t);
    uint8_t *bytes = malloc(length);
    if (!bytes) return false;
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t bits = UINT64_MAX;
        if (times[i].known) memcpy(&bits, &times[i].seconds, sizeof(bits));
        put64(bytes + (size_t)i * sizeof(bits), bits);
    }
    p->time_bytes = bytes;
    p->time_length = length;
    p->version = DC_STRUCTURE_TIME_VERSION;
    return true;
}

bool dc_checkpoint_channels_decode_times(dc_checkpoint_channels_t *p,uint32_t count) {
    if (p->version < DC_STRUCTURE_TIME_VERSION ||
        (p->version >= DC_STRUCTURE_FLUID_VERSION && !p->time_length)) return true;
    if (!p->time_bytes || !count || count > DC_GPU_CHUNK_SLOTS * 2u ||
        p->time_length != (size_t)count * sizeof(uint64_t)) return false;
    dc_chunk_time_t *times = calloc(count, sizeof(*times));
    if (!times) return false;
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t bits = get64(p->time_bytes + (size_t)i * sizeof(bits));
        if (bits == UINT64_MAX) continue;
        double seconds;
        memcpy(&seconds, &bits, sizeof(seconds));
        if (!isfinite(seconds) || seconds < 0.0) { free(times); return false; }
        times[i] = (dc_chunk_time_t){.seconds = seconds, .known = true};
    }
    p->chunk_times = times;
    return true;
}

bool dc_checkpoint_channels_set_fluids(dc_gpu_t *g, dc_checkpoint_channels_t *p,
    const dc_chunk_t *chunks, const uint32_t *slots, uint32_t count, const bool *gas_admitted,
    char *err, uint32_t cap) {
    if (!dc_checkpoint_fluid_encode(g, chunks, slots, count, gas_admitted,
                                  &p->fluid_bytes, &p->fluid_length, err, cap)) return false;
    if (p->fluid_length) p->version = DC_STRUCTURE_FLUID_VERSION;
    return true;
}

static bool new_owner_epoch(uint64_t *epoch) {
    FILE *random = fopen("/dev/urandom", "rb");
    if (!random) return false;
    bool okay = fread(epoch, sizeof(*epoch), 1, random) == 1 && *epoch != 0;
    return fclose(random) == 0 && okay;
}

bool dc_checkpoint_channels_set_owners(const dc_gpu_t *g, dc_checkpoint_channels_t *p,
    const uint32_t *slots, uint32_t count, bool level_save) {
    if (!count || (!level_save && !g->checkpoint_owners)) return true;
    if (count > DC_GPU_CHUNK_SLOTS * 2u) return false;
    uint8_t *bytes = calloc(count, OWNER_RECORD_BYTES);
    if (!bytes) return false;
    uint64_t epoch = 0;
    bool any = false;
    for (uint32_t i = 0; i < count; ++i) {
        dc_chunk_owner_t owner = {0};
        if (slots[i] == UINT32_MAX) {
            if (g->checkpoint_owners) owner = g->checkpoint_owners[i];
        } else if (level_save) {
            if (!epoch && !new_owner_epoch(&epoch)) {
                free(bytes);
                return false;
            }
            owner = (dc_chunk_owner_t){.epoch = epoch, .serial = i + 1u};
        }
        if (!dc_chunk_owner_valid(owner)) continue;
        put64(bytes + (size_t)i * OWNER_RECORD_BYTES, owner.epoch);
        put64(bytes + (size_t)i * OWNER_RECORD_BYTES + sizeof(uint64_t), owner.serial);
        any = true;
    }
    if (!any) {
        free(bytes);
        return true;
    }
    if (!p->fluid_length) { free(bytes); return false; }
    p->owner_bytes = bytes;
    p->owner_length = (size_t)count * OWNER_RECORD_BYTES;
    p->version = DC_STRUCTURE_OWNER_VERSION;
    return true;
}

bool dc_checkpoint_channels_decode_owners(dc_checkpoint_channels_t *p, uint32_t count) {
    if (p->version < DC_STRUCTURE_OWNER_VERSION) return true;
    if (!p->owner_bytes || !count || count > DC_GPU_CHUNK_SLOTS * 2u ||
        p->owner_length != (size_t)count * OWNER_RECORD_BYTES) return false;
    dc_chunk_owner_t *owners = calloc(count, sizeof(*owners));
    if (!owners) return false;
    bool any = false;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *row = p->owner_bytes + (size_t)i * OWNER_RECORD_BYTES;
        owners[i] = (dc_chunk_owner_t){get64(row), get64(row + sizeof(uint64_t))};
        if (!owners[i].epoch && !owners[i].serial) continue;
        if (!dc_chunk_owner_valid(owners[i])) { free(owners); return false; }
        for (uint32_t j = 0; j < i; ++j) {
            if (dc_chunk_owner_equal(owners[i], owners[j])) { free(owners); return false; }
        }
        any = true;
    }
    if (!any) { free(owners); return false; }
    p->owners = owners;
    return true;
}

static size_t channel_framing(uint32_t version) {
    if (version >= DC_STRUCTURE_OWNER_VERSION) return 48u;
    if (version >= DC_STRUCTURE_FLUID_VERSION) return 40u;
    if (version >= DC_STRUCTURE_TIME_VERSION) return 32u;
    return version >= 9u ? 24u : 16u;
}

static void channel_lengths(uint8_t lengths[48], const dc_checkpoint_channels_t *p) {
    put64(lengths, p->reactive_length);
    put64(lengths + 8, p->thermal_length);
    put64(lengths + 16, p->joint_length);
    put64(lengths + 24, p->time_length);
    put64(lengths + 32, p->fluid_length);
    put64(lengths + 40, p->owner_length);
}

uint64_t dc_checkpoint_channels_hash(uint64_t hash,const dc_checkpoint_channels_t *p) {
    uint8_t lengths[48];channel_lengths(lengths,p);
    hash=hash_bytes(hash,lengths,channel_framing(p->version));
    hash=hash_bytes(hash,p->reactive_bytes,p->reactive_length);
    hash=hash_bytes(hash,p->thermal_bytes,p->thermal_length);
    hash=hash_bytes(hash,p->joint_bytes,p->joint_length);
    hash=hash_bytes(hash,p->time_bytes,p->time_length);
    hash=hash_bytes(hash,p->fluid_bytes,p->fluid_length);
    return hash_bytes(hash,p->owner_bytes,p->owner_length);
}
bool dc_checkpoint_channels_write(FILE *file,const dc_checkpoint_channels_t *p) {
    uint8_t lengths[48];channel_lengths(lengths,p);
    size_t framing=channel_framing(p->version);
    return fwrite(lengths,1,framing,file)==framing&&
        (!p->reactive_length||fwrite(p->reactive_bytes,1,p->reactive_length,file)==p->reactive_length)&&
        (!p->thermal_length||fwrite(p->thermal_bytes,1,p->thermal_length,file)==p->thermal_length)&&
        (!p->joint_length||fwrite(p->joint_bytes,1,p->joint_length,file)==p->joint_length)&&
        (!p->time_length||fwrite(p->time_bytes,1,p->time_length,file)==p->time_length)&&
        (!p->fluid_length||fwrite(p->fluid_bytes,1,p->fluid_length,file)==p->fluid_length)&&
        (!p->owner_length||fwrite(p->owner_bytes,1,p->owner_length,file)==p->owner_length);
}

uint64_t dc_gpu_structure_checksum(dc_structural_store_header_t header,
                         const dc_structural_store_payload_t *payload,
                         const uint32_t *pixels, const dc_chunk_t *chunks,
                         const float *body_colors,const dc_chunk_colors_t *colors,
                         const dc_checkpoint_channels_t *channels) {
    header.checksum = 0;
    uint64_t hash =
        hash_bytes(UINT64_C(14695981039346656037), &header, sizeof(header));
    hash = hash_bytes(hash, payload->control, sizeof(payload->control));
    hash = hash_bytes(hash, payload->bodies, (size_t)header.body_words*4u);
    hash = hash_bytes(hash, payload->graph, sizeof(payload->graph));
    if(header.version>=7) hash=hash_bytes(hash,payload->states,
        (size_t)payload->body_count*sizeof(*payload->states));
    hash =
        hash_bytes(hash, pixels, (size_t)header.reserved[1] * sizeof(*pixels));
    hash=hash_bytes(hash,chunks,(size_t)header.reserved[0]*sizeof(*chunks));
    if(header.version>=6) {
        hash=hash_bytes(hash,body_colors,(size_t)header.reserved[1]*128);
        hash=hash_bytes(hash,colors,(size_t)header.reserved[0]*sizeof(*colors));
    }
    if(header.version>=8) hash=dc_checkpoint_channels_hash(hash,channels);
    return hash;
}

bool dc_checkpoint_channels_read(FILE *file,uint32_t version,dc_checkpoint_channels_t *out) {
    uint8_t lengths[48]={0};size_t framing=channel_framing(version);
    if(fread(lengths,1,framing,file)!=framing) return false;
    uint64_t a=get64(lengths),b=get64(lengths+8),c=get64(lengths+16);
    uint64_t t=get64(lengths+24),f=get64(lengths+32),o=get64(lengths+40);
    if(a>SIZE_MAX||b>SIZE_MAX||c>SIZE_MAX||a>UINT64_MAX-b||a+b>UINT64_MAX-c||(b&&!a)) return false;
    if(t>(uint64_t)DC_GPU_CHUNK_SLOTS*2u*sizeof(uint64_t)||a+b+c>UINT64_MAX-t) return false;
    if (version >= DC_STRUCTURE_FLUID_VERSION &&
        (!f || f > dc_checkpoint_fluid_max_bytes(DC_GPU_CHUNK_SLOTS * 2u) ||
         a+b+c+t > UINT64_MAX-f)) return false;
    if (version >= DC_STRUCTURE_OWNER_VERSION &&
        (!o || o > (uint64_t)DC_GPU_CHUNK_SLOTS * 2u * OWNER_RECORD_BYTES ||
         a+b+c+t+f > UINT64_MAX-o)) return false;
    long start=ftell(file);if(start<0||fseek(file,0,SEEK_END)!=0) return false;
    long end=ftell(file);if(end<start||fseek(file,start,SEEK_SET)!=0||(uint64_t)(end-start)!=a+b+c+t+f+o) return false;
    dc_checkpoint_channels_t value={.version=version,.reactive_length=(size_t)a,.thermal_length=(size_t)b,.joint_length=(size_t)c,.time_length=(size_t)t,.fluid_length=(size_t)f,.owner_length=(size_t)o};
    value.reactive_bytes=a?malloc((size_t)a):NULL;value.thermal_bytes=b?malloc((size_t)b):NULL;
    value.joint_bytes=c?malloc((size_t)c):NULL;
    value.time_bytes=t?malloc((size_t)t):NULL;
    value.fluid_bytes=f?malloc((size_t)f):NULL;
    value.owner_bytes=o?malloc((size_t)o):NULL;
    bool okay=(!a||(value.reactive_bytes&&fread(value.reactive_bytes,1,(size_t)a,file)==a))&&
        (!b||(value.thermal_bytes&&fread(value.thermal_bytes,1,(size_t)b,file)==b))&&
        (!c||(value.joint_bytes&&fread(value.joint_bytes,1,(size_t)c,file)==c))&&
        (!t||(value.time_bytes&&fread(value.time_bytes,1,(size_t)t,file)==t))&&
        (!f||(value.fluid_bytes&&fread(value.fluid_bytes,1,(size_t)f,file)==f))&&
        (!o||(value.owner_bytes&&fread(value.owner_bytes,1,(size_t)o,file)==o));
    if(!okay) {
        free(value.reactive_bytes);free(value.thermal_bytes);free(value.joint_bytes);
        free(value.time_bytes);free(value.fluid_bytes);free(value.owner_bytes);return false;
    }
    *out=value;return true;
}
bool dc_checkpoint_channels_prepare(dc_gpu_t *g,dc_checkpoint_channels_t *p,
    const dc_chunk_t *chunks,uint32_t count,const dc_structural_store_payload_t *payload,
    const uint32_t *pixels,uint32_t pixel_words,char *err,uint32_t cap) {
    bool okay = !p->fluid_length || dc_checkpoint_fluid_decode(chunks, count,
        p->fluid_bytes, p->fluid_length, &p->fluids);
    for (uint32_t i = 0; okay && p->owners && i < count; ++i)
        okay = !dc_chunk_owner_valid(p->owners[i]) || (p->fluids && p->fluids[i].known);
    if (!okay && err && cap) snprintf(err, cap, "Invalid structure fluid channel");
    p->fluid_count = count;
    if(okay&&p->reactive_length) okay=fr_reactive_archive_decode(p->reactive_bytes,p->reactive_length,
        chunks,count,&p->reactive,err,cap);
    if(okay&&p->thermal_length) okay=p->reactive_length&&fr_thermal_checkpoint_decode(p->thermal_bytes,p->thermal_length,
        chunks,count,&p->thermal,err,cap);
    if(okay) okay=fr_gpu_reactive_checkpoint_prepare(g,p->reactive_length?&p->reactive:NULL,
        chunks,payload,pixels,pixel_words,&p->prepared_reactive,&p->prepared_handles,err,cap);
    if(okay) okay=fr_thermal_checkpoint_prepare(g,p->prepared_reactive,p->thermal_length?&p->thermal:NULL,
        chunks,&p->prepared_thermal,err,cap);
    if(okay) okay=fr_gpu_revolute_prepare(g,p->joint_bytes,p->joint_length,payload,&p->prepared_joints,err,cap);
    return okay;
}
void dc_checkpoint_channels_publish(dc_gpu_t *g,dc_checkpoint_channels_t *p,
    dc_chunk_t *chunks,dc_chunk_colors_t *colors,uint32_t count) {
    fr_gpu_reactive_checkpoint_clear_pending(g);
    dc_checkpoint_fluid_destroy(g->checkpoint_fluids, g->checkpoint_chunk_count);
    free(g->checkpoint_chunks);free(g->checkpoint_colors);free(g->checkpoint_times);
    free(g->checkpoint_owners);
    g->checkpoint_chunks=chunks;g->checkpoint_colors=colors;g->checkpoint_chunk_count=count;
    g->checkpoint_times=p->chunk_times;p->chunk_times=NULL;
    g->checkpoint_fluids=p->fluids;p->fluids=NULL;
    g->checkpoint_owners=p->owners;p->owners=NULL;
    fr_gpu_reactive_checkpoint_publish(g,p->prepared_reactive);p->prepared_reactive=NULL;
    fr_thermal_checkpoint_publish(g,p->prepared_thermal);p->prepared_thermal=NULL;
    fr_gpu_revolute_publish(g,p->prepared_joints);p->prepared_joints=NULL;
    g->checkpoint_reactive=p->reactive.chunks;p->reactive.chunks=NULL;p->reactive.chunk_count=0;
    g->checkpoint_reactive_handles=p->prepared_handles;p->prepared_handles=NULL;
}
