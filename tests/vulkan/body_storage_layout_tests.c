#include <stddef.h>
#include <stdio.h>
#include "dungeoncraft/body_storage_layout.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_root_and_page_match_shared_std430_contract(void) {
    ASSERT_EQ(sizeof(dc_body_storage_root_t),64u);
    ASSERT_EQ(offsetof(dc_body_storage_root_t,live_count),8u);
    ASSERT_EQ(offsetof(dc_body_storage_root_t,abi_version),16u);
    ASSERT_EQ(offsetof(dc_body_storage_root_t,data),32u);
    ASSERT_EQ(sizeof(dc_body_storage_page_t),48u);
    ASSERT_EQ(offsetof(dc_body_storage_page_t,free_mask),32u);
    ASSERT_EQ(offsetof(dc_body_storage_page_t,generation),40u);
    ASSERT_EQ(sizeof(dc_body_storage_owner_t),32u);
    ASSERT_EQ(offsetof(dc_body_storage_owner_t,source_transaction),16u);
    ASSERT_EQ(sizeof(dc_body_allocation_batch_t),32u);
    ASSERT_EQ(offsetof(dc_body_allocation_batch_t,transaction),24u);PASS();
}
int main(void) {RUN(test_root_and_page_match_shared_std430_contract);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
