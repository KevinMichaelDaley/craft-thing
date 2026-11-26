const uint COATING_CONTACT_NONE = 0xffffffffu;
/* Word two holds conductivity during body conduction. Target selection
 * reuses it only after conduction and until the next thermal snapshot. */
const uint COATING_CONTACT_TARGET = 2u;

bool is_corrosion_sample(uint material) {
    return material == 18u || material == 19u;
}

bool accepts_coating_contact(Words sample_entry) {
    return is_corrosion_sample(sample_entry.words[FR_ENTRY_STATE + FR_STATE_MATERIAL]) &&
        (sample_entry.words[FR_ENTRY_STATE + FR_STATE_FLAGS] & (4u | 16u)) == 0u;
}

uvec3 coating_contact_share(Words donor) {
    uint state = FR_ENTRY_STATE;
    if ((donor.words[state + FR_STATE_FLAGS] & (2u | 4u)) == 0u) return uvec3(0u);
    return min(uvec3(donor.words[state + FR_STATE_COATING_OIL],
        donor.words[state + FR_STATE_COATING_JELLY],
        donor.words[state + FR_STATE_COATING_OXIDIZER]) / 8u, uvec3(4096u));
}

uint choose_coating_contact(ivec2 p, Words donor) {
    if (any(equal(coating_contact_share(donor), uvec3(0u)))) return COATING_CONTACT_NONE;
    for (uint d = 0u; d < 8u; d++) {
        uint at = atlas(p + body_directions[d]);
        if (at == COATING_CONTACT_NONE || !is_corrosion_sample(cells[at].x & 65535u)) continue;
        uvec2 address;
        if (!solid_address(at, address) || !accepts_coating_contact(Words(address))) continue;
        atomicOr(hud[FR_COMBUSTION_SAMPLE_CONTACT_ACTIVE_WORD], 1u);
        return at;
    }
    return COATING_CONTACT_NONE;
}

bool transfer_sample_coating(Words donor, Words receiver) {
    uint s = FR_ENTRY_STATE;
    const uvec3 fields = uvec3(FR_STATE_COATING_OIL, FR_STATE_COATING_JELLY,
                              FR_STATE_COATING_OXIDIZER);
    uvec3 share = coating_contact_share(donor);
    for (uint i = 0u; i < 3u; i++)
        share[i] = min(share[i], 0xffffffffu - receiver.words[s + fields[i]]);
    if (any(equal(share, uvec3(0u)))) return false;

    float capacity = solid_capacity(donor);
    if (capacity <= 0.) return false;
    float temperature = reactive_parcel_energy(donor, s) / capacity;
    float film_capacity = (float(share.x) * coefficient(FR_COATING_OIL_MATERIAL,
        FR_PROFILE_HEAT_CAPACITY_WORD) + float(share.y + share.z) *
        coefficient(FR_COATING_JELLY_MATERIAL, FR_PROFILE_HEAT_CAPACITY_WORD)) / 65536.;
    float energy = film_capacity * temperature;
    for (uint i = 0u; i < 3u; i++) {
        donor.words[s + fields[i]] -= share[i];
        receiver.words[s + fields[i]] += share[i];
    }
    add_energy(donor, -energy);
    add_energy(receiver, energy);
    float threshold = uintBitsToFloat(receiver.words[s + FR_STATE_COATING_THRESHOLD]);
    if (threshold == 0.)
        threshold = ignition_threshold(receiver.words[s + FR_STATE_ORIGIN_LOW] ^ 0x7f4a7c15u,
                                       receiver.words[s + FR_STATE_ORIGIN_LOW + 1u]);
    receiver.words[s + FR_STATE_COATING_THRESHOLD] = floatBitsToUint(threshold);
    receiver.words[s + FR_STATE_COATING_INTEGRAL] = floatBitsToUint(max(threshold,
        uintBitsToFloat(receiver.words[s + FR_STATE_COATING_INTEGRAL])));
    receiver.words[s + FR_STATE_FLAGS] |= 4u;
    return true;
}

void apply_sample_coating(ivec2 p, uint at) {
    if (at == COATING_CONTACT_NONE || !is_corrosion_sample(cells[at].x & 65535u)) return;
    uvec2 address;
    if (!solid_address(at, address)) return;
    Words receiver = Words(address);
    if (!accepts_coating_contact(receiver)) return;

    /* One terrain invocation per receiver. Target selection names at most
     * one receiver per donor. Body and terrain reactions have completed. */
    Words scratch = Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH], root[FR_ARENA_THERMAL_SCRATCH + 1u]));
    for (uint d = 0u; d < 8u; d++) {
        ivec2 q = p + body_directions[d];
        if (!in_body_reaction_grid(q)) continue;
        HeatContacts contacts;
        fr_body_heat_contacts(body_reaction_index(q), true, contacts);
        for (uint n = 0u; n < contacts.count; n++) {
            uvec2 h = contacts.handles[n];
            if (h.x >= root[2] || scratch.words[h.x * 12u + COATING_CONTACT_TARGET] != at) continue;
            Words donor = Words(reactive_entry_address(uvec2(root[4], root[5]), h.x));
            if (donor.words[FR_ENTRY_GENERATION] != h.y || donor.words[FR_ENTRY_OWNER] != 2u) continue;
            if (transfer_sample_coating(donor, receiver)) {
                atomicAdd(hud[31], 1u);
                return;
            }
        }
    }
}
