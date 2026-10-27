static void test_constructed_stone_owns_heat_without_changing_its_collider_or_support(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[13*64+10]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_structural_gpu_graph_t graph={.node_count=2,.member_count=1,.anchor_count=1};
    graph.nodes[0]=(fr_structural_gpu_node_t){.id=1,.generation=1,.x_fp=10*65536,.y_fp=12*65536};
    graph.nodes[1]=graph.nodes[0];graph.nodes[1].id=2;graph.nodes[1].x_fp=22*65536;
    graph.members[0]=(fr_structural_gpu_member_t){1,1,1,2,FR_STRUCTURAL_STONE,2*65536,0,0};
    graph.anchors[0]=(fr_structural_gpu_anchor_t){.id=1,.generation=1,.node=1,.kind=FR_JOINT_MOMENT,
        .x=10,.y=13,.width=1,.height=1,.terrain_material=DC_MATERIAL_STONE,
        .normal_stiffness=1e8,.shear_stiffness=1e8,.rotational_stiffness=1e8,
        .bearing_capacity=1e7,.pullout_capacity=1e7,.shear_capacity=1e7,.moment_capacity=1e7,.friction=.5};
    ASSERT_TRUE(fr_gpu_place_structure(g,&graph,err,sizeof(err)));
    dc_gpu_body_motion_t before,after;ASSERT_TRUE(dc_gpu_read_body_motion(g,FR_STRUCTURAL_BODY_ID_BASE+1,&before,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_reactive_record_t stone;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,FR_STRUCTURAL_BODY_ID_BASE+1,5,0,&stone,&found,err,sizeof(err))&&found);
    ASSERT_EQ(stone.state.material,DC_MATERIAL_STONE);
    ASSERT_TRUE(fr_thermal_gpu_probe(g,15,11,true)&&fr_thermal_gpu_step(g,.25f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,FR_STRUCTURAL_BODY_ID_BASE+1,5,0,&stone,&found,err,sizeof(err))&&found);
    float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&stone.state,&temperature,err,sizeof(err))&&temperature>FR_THERMAL_AMBIENT_K);
    ASSERT_TRUE(dc_gpu_read_body_motion(g,FR_STRUCTURAL_BODY_ID_BASE+1,&after,err,sizeof(err)));
    ASSERT_TRUE(after.mass==before.mass&&after.inertia==before.inertia&&after.angle==before.angle);
    uint32_t pixels=0;ASSERT_TRUE(!dc_gpu_read_pixel_shape(g,FR_STRUCTURAL_BODY_ID_BASE+1,NULL,0,&pixels,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/constructed_stone_heat.dcs",err,sizeof(err))&&present(g,err,sizeof(err)));
    free(chunk);dc_gpu_destroy(g);PASS();
}
