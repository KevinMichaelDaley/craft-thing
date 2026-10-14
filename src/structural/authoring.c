#include <stdio.h>

#include "ferrum/structural/authoring.h"

static bool invalid(char *err, uint32_t cap, const char *reason) {
    if (err && cap)
        snprintf(err, cap, "%s", reason);
    return false;
}

static int64_t chunk_carry(int64_t local) {
    int64_t carry = local / DC_CHUNK_SIDE;
    return carry - (local < 0 && local % DC_CHUNK_SIDE != 0);
}

static bool extent_valid(int64_t chunk, uint32_t local, int32_t pivot,
                          uint32_t extent) {
    int64_t first = chunk_carry((int64_t)local - pivot);
    int64_t last = chunk_carry((int64_t)local - pivot + extent - 1);
    return !(first < 0 && chunk < INT64_MIN - first) &&
           !(last > 0 && chunk > INT64_MAX - last);
}

bool fr_authored_request_valid(const fr_authored_request_t *request,
                               char *err_buf, uint32_t err_cap) {
    const fr_authored_request_t *r = request;
    if (!r || !r->width || !r->height ||
        (uint64_t)r->width * r->height > FR_AUTHORED_PIXEL_LIMIT)
        return invalid(err_buf, err_cap, "Invalid authored canvas dimensions");
    uint64_t count = (uint64_t)r->width * r->height;
    if (!r->physical || !r->appearance ||
        r->physical_bytes != count * FR_AUTHORED_PHYSICAL_STRIDE ||
        r->appearance_bytes != count * FR_AUTHORED_APPEARANCE_STRIDE)
        return invalid(err_buf, err_cap, "Invalid authored channel byte lengths");
    uint32_t identity = 0;
    for (unsigned i = 0; i < sizeof(r->asset_uuid); i++)
        identity |= r->asset_uuid[i];
    if (!identity || !r->transaction_id || !r->state_variant_count ||
        r->state_variant >= r->state_variant_count)
        return invalid(err_buf, err_cap, "Invalid authored identity or state variant");
    if (r->origin_x >= DC_CHUNK_SIDE || r->origin_y >= DC_CHUNK_SIDE ||
        !extent_valid(r->origin.x, r->origin_x, r->pivot_x, r->width) ||
        !extent_valid(r->origin.y, r->origin_y, r->pivot_y, r->height))
        return invalid(err_buf, err_cap, "Authored canvas exceeds world coordinate range");
    if ((r->member_ownership || r->member_ownership_bytes || r->structure) &&
        (!r->member_ownership || !r->structure ||
         r->member_ownership_bytes != count * sizeof(uint32_t)))
        return invalid(err_buf, err_cap, "Structural graph and ownership plane required together");
    return true;
}
