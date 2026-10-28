layout(constant_id=1) const bool AUTHORED_RIGID_OBJECTS=false;
uint authored_source_object(uint index) {
#ifdef DC_RUNTIME_BODY_STORAGE
    if(AUTHORED_RIGID_OBJECTS)
        return BodyStorageWords(uvec2(DC_RUNTIME_WORK(104u),DC_RUNTIME_WORK(105u))).words[index];
#endif
    return 0u;
}
bool authored_objects_compatible(uint a,uint b) {
    return !AUTHORED_RIGID_OBJECTS||authored_source_object(a)==authored_source_object(b);
}
