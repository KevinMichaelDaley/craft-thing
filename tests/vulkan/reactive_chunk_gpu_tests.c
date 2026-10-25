#define main reactive_chunk_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main

#include "ferrum/fire/reactive_chunk.h"

static void test_gpu_compaction_exports_negative_chunk_cells_and_particle_rows(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    c->coord=(dc_chunk_coord_t){-5,-7};c->cells[0].material=DC_MATERIAL_WOOD;
    dc_chunk_particle_init(c->particles+11,c->coord,11,DC_MATERIAL_LEAF_DEBRIS);c->particle_count=1;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    fr_reactive_state_t a,b;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,51,&a,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_from_particle(c->particles+11,600,52,&b,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,0,&a,err,sizeof(err))&&fr_gpu_reactive_seed_particle(g,11,&b,err,sizeof(err)));
    fr_reactive_chunk_t channel={0};ASSERT_TRUE(fr_gpu_reactive_export_chunk(g,0,c,&channel,err,sizeof(err)));
    ASSERT_EQ(channel.count,2u);ASSERT_EQ(channel.next_id,53u);
    bool cell=false,particle=false;
    for(uint32_t i=0;i<channel.count;i++) {
        fr_reactive_attachment_t *entry=channel.attachments+i;
        if(entry->record.owner.kind==FR_REACTIVE_OWNER_CELL) {ASSERT_EQ(entry->local_index,0u);ASSERT_TRUE(memcmp(&entry->record.state,&a,sizeof(a))==0);cell=true;}
        if(entry->record.owner.kind==FR_REACTIVE_OWNER_PARTICLE) {ASSERT_EQ(entry->local_index,11u);ASSERT_TRUE(memcmp(&entry->record.state,&b,sizeof(b))==0);particle=true;}
    }
    ASSERT_TRUE(cell&&particle&&fr_reactive_chunk_valid(&channel,c,err,sizeof(err)));
    ASSERT_TRUE(present(g,err,sizeof(err)));fr_reactive_chunk_release(&channel);free(c);dc_gpu_destroy(g);PASS();
}
static void test_import_replaces_old_slot_state_and_retains_high_water_without_reusing_ids(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    c->cells[0].material=DC_MATERIAL_WOOD;c->cells[1].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    fr_reactive_state_t old;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,80,&old,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,0,&old,err,sizeof(err)));
    fr_reactive_attachment_t a={.local_index=1,.record={.owner=fr_reactive_cell_owner(c->coord,1)}};
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,600,40,&a.record.state,err,sizeof(err)));
    fr_reactive_chunk_t channel={.count=1,.next_id=90,.attachments=&a};
    ASSERT_TRUE(fr_gpu_reactive_import_chunk(g,0,c,&channel,err,sizeof(err)));
    fr_reactive_record_t record;bool found=true;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,0,&record,&found,err,sizeof(err))&&!found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,1,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&record,&a.record,sizeof(record))==0);
    fr_gpu_reactive_stats_t stats;ASSERT_TRUE(fr_gpu_reactive_read_stats(g,&stats,err,sizeof(err)));ASSERT_EQ(stats.live,1u);
    fr_reactive_chunk_t saved={0};ASSERT_TRUE(fr_gpu_reactive_export_chunk(g,0,c,&saved,err,sizeof(err)));ASSERT_EQ(saved.next_id,90u);
    ASSERT_TRUE(present(g,err,sizeof(err)));fr_reactive_chunk_release(&saved);free(c);dc_gpu_destroy(g);PASS();
}
static void test_import_rejects_duplicate_resident_identity_and_rolls_back_all_source_state(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));ASSERT_TRUE(a&&b);
    a->cells[0].material=b->cells[0].material=DC_MATERIAL_WOOD;b->coord.x=1;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,a,err,sizeof(err))&&dc_gpu_upload_chunk(g,1,b,err,sizeof(err)));
    fr_reactive_state_t first,second;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,80,&first,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,600,81,&second,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,0,&first,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,1,0,&second,err,sizeof(err)));
    fr_reactive_attachment_t entry={.local_index=0,.record={.owner=fr_reactive_cell_owner(b->coord,0),.state=first}};
    fr_reactive_chunk_t channel={.count=1,.next_id=82,.attachments=&entry};
    ASSERT_TRUE(!fr_gpu_reactive_import_chunk(g,1,b,&channel,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,0,&record,&found,err,sizeof(err))&&found);ASSERT_TRUE(memcmp(&record.state,&first,sizeof(first))==0);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,1,0,&record,&found,err,sizeof(err))&&found);ASSERT_TRUE(memcmp(&record.state,&second,sizeof(second))==0);
    fr_gpu_reactive_stats_t stats;ASSERT_TRUE(fr_gpu_reactive_read_stats(g,&stats,err,sizeof(err)));ASSERT_EQ(stats.live,2u);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(a);free(b);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_gpu_compaction_exports_negative_chunk_cells_and_particle_rows);
    RUN(test_import_replaces_old_slot_state_and_retains_high_water_without_reusing_ids);
    RUN(test_import_rejects_duplicate_resident_identity_and_rolls_back_all_source_state);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
