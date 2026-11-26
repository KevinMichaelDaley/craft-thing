const uint RIGID_COATING_HEAD = 0u;
const uint RIGID_COATING_NEXT = 1u;
const uint RIGID_COATING_TARGET = 2u;
const uint RIGID_COATING_ACCEPTED = 3u;
const uint RIGID_COATING_STOCK = 4u;
const uint RIGID_COATING_HEAT = 7u;
const uint RIGID_COATING_GENERATION = 8u;
const uint RIGID_COATING_CREDIT = 9u;
const uint RIGID_COATING_PENDING_CREDIT = 12u;

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

float rigid_adhesion_fraction(float seconds) {
    float x = float(FR_COMBUSTION_RIGID_ADHESION_RATE) * seconds;
    /* The series preserves small time steps when 1 - exp(-x) cancels. */
    if (x < .03125) return x * (1. + x * (-.5 + x * (1./6. - x/24.)));
    return 1. - exp(-x);
}

uint accrue_rigid_adhesion(uint stock, uint fraction, inout uint credit) {
    if (stock == 0u) { credit = 0u; return 0u; }
    /* Credit is elapsed adhesion demand below one mass quantum, stored as
     * Q0.32. Integrate against stock minus the demand already accrued. */
    uint whole, fractional, discount, unused;
    umulExtended(stock, fraction, whole, fractional);
    umulExtended(credit, fraction, discount, unused);
    uint accumulated = fractional + (credit - discount);
    whole += uint(accumulated < fractional);
    credit = whole == stock ? 0u : accumulated;
    return whole;
}

void prepare_rigid_coating(uint index, Words donor, ivec2 p, bool exposed) {
    Words work = rigid_coating_work();
    uint base = rigid_coating_word(index, 0u);
    work.words[base + RIGID_COATING_HEAD] = COATING_CONTACT_NONE;
    work.words[base + RIGID_COATING_TARGET] = COATING_CONTACT_NONE;
    uint generation = donor.words[FR_ENTRY_GENERATION];
    if (work.words[base + RIGID_COATING_GENERATION] != generation) {
        work.words[base + RIGID_COATING_GENERATION] = generation;
        for (uint i = 0u; i < 3u; i++) work.words[base + RIGID_COATING_CREDIT + i] = 0u;
    }
    if (!exposed) return;

    uint s = FR_ENTRY_STATE;
    uint flags = donor.words[s + FR_STATE_FLAGS];
    bool active_film = (flags & 4u) != 0u &&
        uintBitsToFloat(donor.words[s + FR_STATE_COATING_INTEGRAL]) >= coating_ignition_threshold(donor);
    if ((flags & 2u) == 0u && !active_film) return;
    uvec3 stock = coating_inventory(donor);
    if ((stock.x | stock.y) == 0u) return;
    uint target = rigid_coating_receiver(p, donor.words[FR_ENTRY_OWNER + 1u]);
    if (target == COATING_CONTACT_NONE) return;
    float fraction = rigid_adhesion_fraction(uintBitsToFloat(params.seconds));
    uvec3 share = stock;
    uvec3 credit = uvec3(0u);
    if (fraction < 1.) {
        uint fixed_fraction = uint(fraction * 4294967296.);
        for (uint i = 0u; i < 3u; i++) {
            credit[i] = work.words[base + RIGID_COATING_CREDIT + i];
            share[i] = accrue_rigid_adhesion(stock[i], fixed_fraction, credit[i]);
        }
    }
    if ((share.x | share.y | share.z) == 0u) {
        for (uint i = 0u; i < 3u; i++) work.words[base + RIGID_COATING_CREDIT + i] = credit[i];
        return;
    }
    float heat = coating_contact_heat(donor, share);
    if (heat <= 0.) return;

    work.words[base + RIGID_COATING_TARGET] = target;
    for (uint i = 0u; i < 3u; i++) {
        work.words[base + RIGID_COATING_STOCK + i] = share[i];
        work.words[base + RIGID_COATING_PENDING_CREDIT + i] = credit[i];
    }
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
        for (uint i = 0u; i < 3u; i++) {
            entry.words[s + COATING_FIELDS[i]] -= share[i];
            work.words[base + RIGID_COATING_CREDIT + i] = work.words[base + RIGID_COATING_PENDING_CREDIT + i];
        }
        add_energy(entry, -uintBitsToFloat(work.words[base + RIGID_COATING_HEAT]));
    }
    if (work.words[base + RIGID_COATING_ACCEPTED] == 0u) return;
    uint first = work.words[base + RIGID_COATING_HEAD];
    for (uint donor = next_rigid_coating_donor(first, COATING_CONTACT_NONE, work);
         donor != COATING_CONTACT_NONE; donor = next_rigid_coating_donor(first, donor, work)) {
        uvec3 share = prepared_rigid_coating(donor, work);
        for (uint i = 0u; i < 3u; i++) entry.words[s + COATING_FIELDS[i]] += share[i];
        add_energy(entry, uintBitsToFloat(work.words[rigid_coating_word(donor, RIGID_COATING_HEAT)]));
    }
    if (all(greaterThan(coating_inventory(entry), uvec3(0u)))) kindle_contact_film(entry);
}

void rigid_coating_phase(uint mode, uint index, Words entry) {
    Words work = rigid_coating_work();
    uint base = rigid_coating_word(index, 0u);
    if (mode == FR_COMBUSTION_RIGID_COATING_LINK) link_rigid_coating(index, work);
    else if (mode == FR_COMBUSTION_RIGID_COATING_ADMIT) admit_rigid_coating(base, work, entry);
    else publish_rigid_coating(index, base, work, entry);
}
