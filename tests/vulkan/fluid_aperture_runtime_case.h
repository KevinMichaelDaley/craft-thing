static void test_long_transport_uses_exact_runtime_apertures(void) {
    char err[256];
    dc_chunk_t left={0},right={0},saved;
    for(unsigned y=0;y<64;y++) for(unsigned x=0;x<128;x++) {
        dc_chunk_t *chunk=x<64?&left:&right;
        unsigned i=y*64+x%64;
        chunk->cells[i].material=(x==0||x==127||y==63)?DC_MATERIAL_STONE:DC_MATERIAL_AIR;
        if(y>8&&y<55&&x>3&&x<124)
            chunk->cells[i].fluid_mass=DC_FLUID_FULL-(x*73+y*97)%8192;
        chunk->face_velocity[i].x=((int)(x%11)-5)*1.5f;
        chunk->face_velocity[i].y=((int)(y%7)-3)*.8f;
    }
    dc_gpu_t *g=make_grid(&left,&right,err,sizeof(err));ASSERT_TRUE(g);
    dc_gpu_body_t body={.id=99,.x_fp=48<<16,.y_fp=20<<16,.width=14,.height=12,.active=1};
    ASSERT_TRUE(dc_gpu_spawn_body(g,body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,99,.37f,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    dispatch_gpu=g;memset(dispatch_jobs,0,sizeof(dispatch_jobs));
    memset(g->mpm_label_a_mapped,0xa5,2*128*64*sizeof(uint32_t));
    for(unsigned tick=0;tick<2;tick++) ASSERT_TRUE(dc_gpu_fluid_step(g,err,sizeof(err)));
    uint64_t hash=UINT64_C(14695981039346656037),mass=0;
    for(unsigned slot=0;slot<2;slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g,slot,&saved,err,sizeof(err)));
        const unsigned char *bytes=(void *)saved.cells;
        for(size_t i=0;i<sizeof(saved.cells);i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
        bytes=(void *)saved.face_velocity;
        for(size_t i=0;i<sizeof(saved.face_velocity);i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
        for(unsigned i=0;i<4096;i++) mass+=saved.cells[i].fluid_mass;
    }
    const uint32_t *prefix=g->mpm_label_a_mapped,*coverage=g->occupancy_mapped;
    unsigned mismatches=0, restricted=0, open=0, changed_labels=0;
    const uint32_t *labels=g->mpm_label_a_mapped;
    for(unsigned y=0;y<64;y++) for(unsigned x=0;x<128;x++) {
        unsigned lo=x/16*16,hi=lo+16;
        uint32_t a=DC_FLUID_FULL,b=DC_FLUID_FULL;
        for(unsigned k=lo;k<=x;k++) {
            uint32_t covered=coverage[3*128*64+y*128+k];
            uint32_t value=DC_FLUID_FULL-(covered<DC_FLUID_FULL?covered:DC_FLUID_FULL);
            if(value<a) a=value;
        }
        for(unsigned k=x;k<hi;k++) {
            uint32_t covered=coverage[3*128*64+y*128+k];
            uint32_t value=DC_FLUID_FULL-(covered<DC_FLUID_FULL?covered:DC_FLUID_FULL);
            if(value<b) b=value;
        }
        unsigned word=2*(y*128+x);
        restricted+=a<DC_FLUID_FULL || b<DC_FLUID_FULL;
        open+=a==DC_FLUID_FULL && b==DC_FLUID_FULL;
        changed_labels+=labels[word]!=UINT32_C(0xa5a5a5a5) || labels[word+1]!=UINT32_C(0xa5a5a5a5);
        mismatches+=prefix[2*(y*128+x)]!=a||prefix[2*(y*128+x)+1]!=b;
    }
    printf("long transport fingerprint %016llx mass%llu runtime aperture mismatches%u\n",
        (unsigned long long)hash,(unsigned long long)mass,mismatches);
    printf("Aperture pairs: %u restricted, %u fully open, %u changed particle labels\n", restricted,open,changed_labels);
    bool cooperative=true;
    printf("transport workgroups");
    for(unsigned mode=0;mode<4;mode++) {
        printf(" %llu",(unsigned long long)dispatch_jobs[mode]);
        cooperative=cooperative&&dispatch_jobs[mode]==2u*(mode<2?g->width:g->height);
    }
    printf("; displacement %llu %llu\n",(unsigned long long)dispatch_jobs[11],(unsigned long long)dispatch_jobs[12]);
    bool displaced=dispatch_jobs[11]==2u*g->width&&dispatch_jobs[12]==2u*g->height;
    dispatch_gpu=NULL;
    dc_gpu_destroy(g);
    if(require_cooperative) ASSERT_TRUE(cooperative);
    if(require_displacement) ASSERT_TRUE(displaced);
    ASSERT_TRUE(restricted && open);
    ASSERT_EQ(changed_labels,0u);
    ASSERT_EQ(mismatches,0u);PASS();
}
