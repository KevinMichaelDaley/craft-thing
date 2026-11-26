const uint RIGID_COATING_HEAD = 0u;
const uint RIGID_COATING_NEXT = 1u;
const uint RIGID_COATING_TARGET = 2u;
const uint RIGID_COATING_ACCEPTED = 3u;
const uint RIGID_COATING_STOCK = 4u;
const uint RIGID_COATING_HEAT = 7u;

uint rigid_coating_word(uint index, uint field) {
    return index * FR_ARENA_RIGID_COATING_WORDS + field;
}

Words rigid_coating_work() {
    return Words(uvec2(root[FR_ARENA_RIGID_COATING], root[FR_ARENA_RIGID_COATING + 1u]));
}

uint rigid_coating_receiver(ivec2 p, uint body_id) {
    for (uint face = 0u; face < 4u; face++) {
        ivec2 q = p + body_directions[face];
        if (!in_body_reaction_grid(q)) continue;
        HeatContacts contacts;
        fr_body_heat_contacts(body_reaction_index(q), false, contacts);
        for (uint n = 0u; n < contacts.count; n++) {
            uvec2 h = contacts.handles[n];
            if (h.x >= root[2]) continue;
            Words e = Words(reactive_entry_address(uvec2(root[4], root[5]), h.x));
            uint receiver_id = e.words[FR_ENTRY_OWNER + 1u];
            if (receiver_id == body_id || e.words[FR_ENTRY_GENERATION] != h.y ||
                e.words[FR_ENTRY_OWNER] != 2u) continue;
            uint material = e.words[FR_ENTRY_STATE + FR_STATE_MATERIAL];
            if (material != 7u && material != 8u && material != 9u && material != 12u) continue;
            uint slot = runtime_body_find(receiver_id);
            if (slot >= BODY_CAPACITY) continue;
            Body receiver = DC_BODY(slot);
            if ((receiver.enabled | receiver.reserved0) != 0u && pixel_body(receiver)) return h.x;
        }
    }
    return COATING_CONTACT_NONE;
}

void prepare_rigid_coating(uint index, Words donor, ivec2 p, bool exposed) {
    Words work = rigid_coating_work();
    uint base = rigid_coating_word(index, 0u);
    work.words[base + RIGID_COATING_HEAD] = COATING_CONTACT_NONE;
    work.words[base + RIGID_COATING_TARGET] = COATING_CONTACT_NONE;
    if (!exposed) return;

    uint s = FR_ENTRY_STATE;
    uint flags = donor.words[s + FR_STATE_FLAGS];
    bool active_film = (flags & 4u) != 0u &&
        uintBitsToFloat(donor.words[s + FR_STATE_COATING_INTEGRAL]) >= coating_ignition_threshold(donor);
    if ((flags & 2u) == 0u && !active_film) return;
    uvec3 share = coating_inventory(donor) / 4u;
    if ((share.x | share.y) == 0u) return;
    uint target = rigid_coating_receiver(p, donor.words[FR_ENTRY_OWNER + 1u]);
    if (target == COATING_CONTACT_NONE) return;
    float heat = coating_contact_heat(donor, share);
    if (heat <= 0.) return;

    work.words[base + RIGID_COATING_TARGET] = target;
    for (uint i = 0u; i < 3u; i++) work.words[base + RIGID_COATING_STOCK + i] = share[i];
    work.words[base + RIGID_COATING_HEAT] = floatBitsToUint(heat);
    atomicMax(hud[FR_COMBUSTION_RIGID_COATING_DISPATCH_WORD], gl_NumWorkGroups.x);
    atomicMax(hud[FR_COMBUSTION_RIGID_COATING_DISPATCH_WORD + 1u], gl_NumWorkGroups.y);
}

void link_rigid_coating(uint index, Words work) {
    uint base = rigid_coating_word(index, 0u);
    uint target = work.words[base + RIGID_COATING_TARGET];
    if (target != COATING_CONTACT_NONE)
        work.words[base + RIGID_COATING_NEXT] = atomicExchange(
            work.words[rigid_coating_word(target, RIGID_COATING_HEAD)], index);
}

uvec3 prepared_rigid_coating(uint donor, Words work) {
    uint stock = rigid_coating_word(donor, RIGID_COATING_STOCK);
    return uvec3(work.words[stock], work.words[stock + 1u], work.words[stock + 2u]);
}

void admit_rigid_coating(uint base, Words work, Words entry) {
    uvec3 room = uvec3(0xffffffffu) - coating_inventory(entry);
    uint donor = work.words[base + RIGID_COATING_HEAD];
    bool accepted = donor != COATING_CONTACT_NONE;
    for (; donor != COATING_CONTACT_NONE; donor = work.words[rigid_coating_word(donor, RIGID_COATING_NEXT)]) {
        uvec3 share = prepared_rigid_coating(donor, work);
        if (any(greaterThan(share, room))) { accepted = false; break; }
        room -= share;
    }
    work.words[base + RIGID_COATING_ACCEPTED] = uint(accepted);
}

uint next_rigid_coating_donor(uint first, uint previous, Words work) {
    /* Coating stocks are integers, but order enthalpy accumulation by owner
     * index so atomic inbox insertion order cannot affect the heat ledger. */
    uint next = COATING_CONTACT_NONE;
    for (uint donor = first; donor != COATING_CONTACT_NONE;
         donor = work.words[rigid_coating_word(donor, RIGID_COATING_NEXT)]) {
        if ((previous == COATING_CONTACT_NONE || donor > previous) && donor < next) next = donor;
    }
    return next;
}

void publish_rigid_coating(uint index, uint base, Words work, Words entry) {
    uint s = FR_ENTRY_STATE;
    uint target = work.words[base + RIGID_COATING_TARGET];
    if (target != COATING_CONTACT_NONE &&
        work.words[rigid_coating_word(target, RIGID_COATING_ACCEPTED)] != 0u) {
        uvec3 share = prepared_rigid_coating(index, work);
        for (uint i = 0u; i < 3u; i++) entry.words[s + COATING_FIELDS[i]] -= share[i];
        add_energy(entry, -uintBitsToFloat(work.words[base + RIGID_COATING_HEAT]));
    }
    if (work.words[base + RIGID_COATING_ACCEPTED] == 0u) return;
    uint first = work.words[base + RIGID_COATING_HEAD];
    bool burning_mixture = false;
    for (uint donor = next_rigid_coating_donor(first, COATING_CONTACT_NONE, work);
         donor != COATING_CONTACT_NONE; donor = next_rigid_coating_donor(first, donor, work)) {
        uvec3 share = prepared_rigid_coating(donor, work);
        for (uint i = 0u; i < 3u; i++) entry.words[s + COATING_FIELDS[i]] += share[i];
        add_energy(entry, uintBitsToFloat(work.words[rigid_coating_word(donor, RIGID_COATING_HEAT)]));
        burning_mixture = burning_mixture || all(greaterThan(share, uvec3(0u)));
    }
    if (burning_mixture) kindle_contact_film(entry);
}

void rigid_coating_phase(uint mode, uint index, Words entry) {
    Words work = rigid_coating_work();
    uint base = rigid_coating_word(index, 0u);
    if (mode == FR_COMBUSTION_RIGID_COATING_LINK) link_rigid_coating(index, work);
    else if (mode == FR_COMBUSTION_RIGID_COATING_ADMIT) admit_rigid_coating(base, work, entry);
    else publish_rigid_coating(index, base, work, entry);
}
