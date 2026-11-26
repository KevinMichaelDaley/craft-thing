/* A rotated unit cell spans fewer than two local units per axis and can
 * therefore overlap at most a three-by-three set of texels. */
const uint FR_HEAT_CONTACTS = 9u;
struct HeatContacts {
    uint count;
    uvec2 handles[FR_HEAT_CONTACTS];
    float coverage[FR_HEAT_CONTACTS];
#ifdef FR_BODY_REACTIONS
    uvec2 geometry[FR_HEAT_CONTACTS];
#endif
};

shared uint heat_pose_slots[DC_SPRITE_MAX];
shared Body heat_pose_bodies[DC_SPRITE_MAX];
shared vec4 heat_pose_bounds[DC_SPRITE_MAX];

uint fr_heat_pose_count() {
    if((params.sprite_address.x|params.sprite_address.y)==0u) return 0u;
    ContactSprites sprites=ContactSprites(params.sprite_address);
    return sprites.words[0]==DC_SPRITE_MAGIC?min(sprites.words[1],DC_SPRITE_MAX):0u;
}

void fr_prepare_heat_poses() {
    uint count=fr_heat_pose_count();
    if(count==0u) return;
    uint n=gl_LocalInvocationID.x;
    if(n<count) {
        ContactSprites sprites=ContactSprites(params.sprite_address);
        uint pose=fr_sprite_pose_word(n);
        uint slot=runtime_body_find(sprites.words[pose]);
        heat_pose_slots[n]=BODY_CAPACITY;
        if(slot<BODY_CAPACITY&&DC_BODY_OWNER(runtime_body_root,slot).generation==sprites.words[pose+1u]) {
            Body body=DC_BODY(slot);
            if(body.enabled!=0u&&pixel_body(body)) {
                vec2 local_low=-fr_sprite_pose_crop(sprites,pose);
                vec2 local_high=local_low+vec2(sprites.words[pose+3u],sprites.words[pose+4u]);
                vec2 origin=vec2(body.x_fp,body.y_fp)/65536.+body.center;
                vec2 low=vec2(1e20),high=vec2(-1e20);
                for(uint corner=0u;corner<4u;corner++) {
                    vec2 local=vec2((corner&1u)==0u?local_low.x:local_high.x,
                                    (corner&2u)==0u?local_low.y:local_high.y);
                    vec2 world=origin+body_rotate(local-body.center,body.angle);
                    low=min(low,world);high=max(high,world);
                }
                heat_pose_slots[n]=slot;
                heat_pose_bodies[n]=body;
                heat_pose_bounds[n]=vec4(floor(low),ceil(high));
            }
        }
    }
    barrier();
}

bool fr_has_heat_pose(uint id) {
    if(id==0u) return false;
    for(uint n=0u;n<fr_heat_pose_count();n++) {
        if(heat_pose_slots[n]<BODY_CAPACITY&&heat_pose_bodies[n].id==id) return true;
    }
    return false;
}

bool fr_heat_pose_may_touch(ivec2 cell,uint ordinal) {
    if(heat_pose_slots[ordinal]>=BODY_CAPACITY) return false;
    vec4 bounds=heat_pose_bounds[ordinal];
    return all(greaterThanEqual(vec2(cell),bounds.xy))&&all(lessThan(vec2(cell),bounds.zw));
}

vec2 fr_pose_grid_center(Body body,ivec2 cell,vec2 crop) {
    return body_local_point(body,vec2(cell)+.5-vec2(body.x_fp,body.y_fp)/65536.)+crop;
}

uint fr_pose_piece_at(ContactSprites sprites,uint pose,ivec2 size,ivec2 texel,Body body,out ivec2 rest) {
    if(any(lessThan(texel,ivec2(0)))||any(greaterThanEqual(texel,size))) return 0xffffffffu;
    uvec2 rest_origin=uvec2(sprites.words[pose+6u],sprites.words[pose+7u]);
    if(any(greaterThanEqual(rest_origin,uvec2(DC_SPRITE_SOURCE_SIDE)))) return 0xffffffffu;
    if(sprites.words[pose+5u]!=0u) texel.x=size.x-1-texel.x;
    uint offset=uint(texel.y*size.x+texel.x);
    uint frame=DC_SPRITE_ATLAS_WORD+2u*sprites.words[pose+2u];
    uint source=sprites.words[frame+2u*offset+1u];
    if(source==0u||source>DC_SPRITE_SOURCE_COUNT) return 0xffffffffu;
    uint inverse=sprites.words[pose+DC_SPRITE_CONTACT_WORD];
    if(sprites.words[inverse+source-1u]!=offset+1u) return 0xffffffffu;
    rest=ivec2((source-1u)%DC_SPRITE_SOURCE_SIDE,(source-1u)/DC_SPRITE_SOURCE_SIDE)-ivec2(rest_origin);
    return pixel_piece_at(body,rest);
}

#ifdef FR_BODY_REACTIONS
/* The displayed cell is sampled at its center. Fractional heat overlap
 * still exchanges energy at a contact without closing its gas cell. */
bool fr_pose_blocks_air(ivec2 cell) {
    for(uint n=0u;n<fr_heat_pose_count();n++) {
        if(!fr_heat_pose_may_touch(cell,n)) continue;
        ContactSprites sprites=ContactSprites(params.sprite_address);
        uint pose=fr_sprite_pose_word(n);
        ivec2 size=ivec2(sprites.words[pose+3u],sprites.words[pose+4u]);
        Body body=heat_pose_bodies[n];
        ivec2 art=ivec2(floor(fr_pose_grid_center(body,cell,fr_sprite_pose_crop(sprites,pose))));
        ivec2 rest;
        if(pixel_piece_material(body,fr_pose_piece_at(sprites,pose,size,art,body,rest))!=0u) return true;
    }
    return false;
}
#endif

uint fr_body_heat_owner(uint index) {
    if(fr_heat_pose_count()==0u) return thermal_occupancy[index];
    vec4 metadata=samples[index+params.width*params.height];
    if(floatBitsToUint(metadata.x)!=4u) return 0u;
    uvec2 address=reactive_entry_address(uvec2(root[4],root[5]),floatBitsToUint(metadata.y));
    return Words(address).words[FR_ENTRY_OWNER+1u];
}

bool fr_body_heat_occluder(uint index) {
    uint id=thermal_occupancy[index];
    return id!=0u&&!fr_has_heat_pose(id);
}

#ifdef FR_BODY_REACTIONS
ivec2 fr_body_parcel_cell(Body body,uint piece) {
    BodyQuad quad;
    if(!body_pixel_quad(body,piece,quad)) return ivec2(-1);
    return ivec2(floor(vec2(body.x_fp,body.y_fp)/65536.+.25*(quad.a+quad.b+quad.c+quad.d)));
}
#endif

void fr_append_heat_contact(Body body,uint slot,uint piece,ivec2 cell,bool centered,inout HeatContacts contacts) {
    float coverage=1.;
#ifdef FR_BODY_REACTIONS
    if(centered) {
        if(any(notEqual(fr_body_parcel_cell(body,piece),cell))) return;
    } else
#endif
    {
        vec2 center;
        coverage=body_cell_volume(body,piece,cell,1.,center);
        if(coverage<=0.) return;
    }
    uvec2 handle=fr_body_reactive_handle(body,slot,piece);
    if(handle.y==0u) return;
    contacts.handles[contacts.count]=handle;
    contacts.coverage[contacts.count]=coverage;
#ifdef FR_BODY_REACTIONS
    uint word=body.compound_reserved+pixel_tile_offset(body,piece/256u)+1u+(piece%256u)/8u;
    contacts.geometry[contacts.count]=uvec2(word,piece%8u);
#endif
    contacts.count++;
}

void fr_posed_heat_contacts(ivec2 cell,uint n,bool centered,inout HeatContacts contacts) {
    if(!fr_heat_pose_may_touch(cell,n)) return;
    uint slot=heat_pose_slots[n];
    Body body=heat_pose_bodies[n];
    ContactSprites sprites=ContactSprites(params.sprite_address);
    uint pose=fr_sprite_pose_word(n);
    ivec2 size=ivec2(sprites.words[pose+3u],sprites.words[pose+4u]);
    vec2 crop=fr_sprite_pose_crop(sprites,pose);
    vec2 local=fr_pose_grid_center(body,cell,crop);
    float extent=.5*(abs(cos(body.angle))+abs(sin(body.angle)));
    ivec2 low=clamp(ivec2(floor(local-extent)),ivec2(0),size);
    ivec2 high=clamp(ivec2(ceil(local+extent)),ivec2(0),size);
    for(int y=low.y;y<high.y;y++) for(int x=low.x;x<high.x;x++) {
        ivec2 rest;
        uint piece=fr_pose_piece_at(sprites,pose,size,ivec2(x,y),body,rest);
        if(pixel_piece_material(body,piece)==0u) continue;
        Body contact=body;
        if(fr_body_project_texel(rest,vec2(x,y)-crop,contact))
            fr_append_heat_contact(contact,slot,piece,cell,centered,contacts);
    }
}

void fr_body_heat_contacts(uint index,bool centered,out HeatContacts contacts) {
    contacts.count=0u;
    ivec2 cell=ivec2(index%params.width,index/params.width);
    for(uint n=0u;n<fr_heat_pose_count();n++) {
        fr_posed_heat_contacts(cell,n,centered,contacts);
        if(contacts.count!=0u) return;
    }
    uint id=thermal_occupancy[index];
    if(id==0u||fr_has_heat_pose(id)) return;
    uint slot=runtime_body_find(id);
    if(slot>=BODY_CAPACITY) return;
    Body body=body_thermal_grid(DC_BODY(slot));
    if(!pixel_body(body)) return;
    vec2 low=vec2(cell)-vec2(body.x_fp,body.y_fp)/65536.;
    uvec4 query=body_piece_query(body,low,low+1.);
    for(uint i=0u;i<query.z*query.w;i++) {
        uint piece=query_piece(body,query,i);
        if(pixel_piece_material(body,piece)!=0u) fr_append_heat_contact(body,slot,piece,cell,centered,contacts);
    }
}

bool fr_body_sample(uint index,out vec4 node_value,out vec4 metadata) {
    HeatContacts contacts;
    fr_body_heat_contacts(index,false,contacts);
    node_value=vec4(0);metadata=vec4(0);
    float total=0.,dominant=0.;
    for(uint i=0u;i<contacts.count;i++) {
        uvec2 handle=contacts.handles[i];float coverage=contacts.coverage[i];
        Words e=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));uint s=FR_ENTRY_STATE,m=e.words[s];
        float energy=reactive_parcel_energy(e,s);
        float capacity=reactive_parcel_heat_capacity(e,s);
        float wet_capacity=float(e.words[s+8u])*coefficient(m,19u),wet=wet_capacity>0.?clamp(float(e.words[s+12u])/wet_capacity,0.,1.):0.;
        node_value+=vec4(energy,capacity,coefficient(m,6u),wet)*coverage;total+=coverage;
        if(coverage>dominant) {dominant=coverage;metadata=vec4(uintBitsToFloat(4u),uintBitsToFloat(handle.x),uintBitsToFloat(m),uintBitsToFloat(handle.y));}
    }
    if(total>0.) node_value.zw/=total;
    return node_value.y>0.;
}

void fr_body_atomic_heat(Words scratch,uint word,float heat) {
    if(heat==0.) return;
    uint old=atomicAdd(scratch.words[word],0u);
    for(;;) {uint next=floatBitsToUint(uintBitsToFloat(old)+heat);
        uint observed=atomicCompSwap(scratch.words[word],old,next);if(observed==old) break;old=observed;}
}

float fr_body_heat_weight(uvec2 handle,float coverage,float heat) {
    Words entry=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));uint state=FR_ENTRY_STATE;
    float energy=uintBitsToFloat(entry.words[state+17u])+uintBitsToFloat(entry.words[state+25u])+uintBitsToFloat(entry.words[state+26u]);
    Words scratch=Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    float capacity=uintBitsToFloat(scratch.words[handle.x*12u+1u]);
    return fr_thermal_heat_weight(coverage,capacity,energy,heat);
}

void fr_body_deposit_radiance(uint index,float heat,float outgoing,float incoming) {
    if(heat==0.&&outgoing==0.&&incoming==0.) return;
    HeatContacts contacts;
    fr_body_heat_contacts(index,false,contacts);
    if(contacts.count==0u) return;
    float total=0.,heat_total=0.,weights[FR_HEAT_CONTACTS];
    for(uint i=0u;i<contacts.count;i++) {
        total+=contacts.coverage[i];
        weights[i]=fr_body_heat_weight(contacts.handles[i],contacts.coverage[i],heat);
        heat_total+=weights[i];
    }
    Words scratch=Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    for(uint i=0u;i<contacts.count;i++) {
        uint base=contacts.handles[i].x*12u;
        float share=contacts.coverage[i]/total;
        if(heat_total>0.) fr_body_atomic_heat(scratch,base+4u,heat*weights[i]/heat_total);
        fr_body_atomic_heat(scratch,base+10u,outgoing*share);
        fr_body_atomic_heat(scratch,base+11u,incoming*share);
    }
}

void fr_body_deposit(uint index,float heat) {fr_body_deposit_radiance(index,heat,0.,0.);}
