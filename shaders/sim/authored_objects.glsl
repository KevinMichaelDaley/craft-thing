layout(constant_id=1) const bool AUTHORED_RIGID_OBJECTS=false;
uint authored_source_object(uint index) {
#ifdef DC_RUNTIME_BODY_STORAGE
    if(AUTHORED_RIGID_OBJECTS)
        return BodyStorageWords(uvec2(DC_RUNTIME_WORK(104u),DC_RUNTIME_WORK(105u))).words[index+4u];
#endif
    return 0u;
}
void authored_validate_texel(bool valid) {
#ifdef DC_RUNTIME_BODY_STORAGE
    if(AUTHORED_RIGID_OBJECTS) {
        BodyStorageWords channel=BodyStorageWords(uvec2(DC_RUNTIME_WORK(104u),DC_RUNTIME_WORK(105u)));
        if(valid) atomicAdd(channel.words[1],1u);else atomicOr(channel.words[2],1u);
    }
#endif
}
void authored_publish_texel(uint index) {
#ifdef DC_RUNTIME_BODY_STORAGE
    if(AUTHORED_RIGID_OBJECTS) {
        BodyStorageWords channel=BodyStorageWords(uvec2(DC_RUNTIME_WORK(104u),DC_RUNTIME_WORK(105u)));
        channel.words[index+4u]=0u;atomicAdd(channel.words[1],0xffffffffu);
    }
#endif
}
bool authored_objects_compatible(uint a,uint b) {
    return !AUTHORED_RIGID_OBJECTS||authored_source_object(a)==authored_source_object(b);
}
