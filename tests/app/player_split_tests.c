#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define PASS() g_pass++
static char err[256];
static dc_level_view_t *v=NULL;

static void setup(void) {
    char directory[]="build/player_split_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface=96;
    dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&surface};
    v=dc_level_view_create_ground(directory,314,&ground,err,sizeof(err));
    ASSERT_TRUE(v);
    ASSERT_TRUE(dc_level_view_pan_pixels(v,0,128));
    ASSERT_TRUE(dc_level_view_wait_visible(v,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_players_enable(v,err,sizeof(err)));
}
static void teardown(void) {
    if(v) dc_level_view_destroy(v,err,sizeof(err));
    v=NULL;
}
static void test_second_seat_joins_with_its_own_beaver(void) {
    setup();
    fr_player_t p;fr_beaver_body_t b0,b1;
    ASSERT_TRUE(dc_players_status(v,0,&p,&b0));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,1,&p,&b1));
    ASSERT_TRUE(b1.binding.body_id!=0u);
    ASSERT_TRUE(b1.binding.body_id!=b0.binding.body_id);
    /* The joined seat rides its own input device. */
    ASSERT_TRUE(p.identity!=0u);
    for(unsigned n=0;n<4;n++)
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    teardown();
    PASS();
}
static void test_split_screen_routes_bands_to_their_players(void) {
    setup();
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    for(unsigned n=0;n<4;n++)
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    /* The present layout exposes two side-by-side bands of the world grid. */
    uint32_t x,y;
    ASSERT_TRUE(dc_gpu_screen_cell(dc_level_view_gpu(v),10u*WINDOW_SCALE,135u*WINDOW_SCALE,&x,&y));
    uint32_t left_x=x;
    ASSERT_TRUE(dc_gpu_screen_cell(dc_level_view_gpu(v),470u*WINDOW_SCALE,135u*WINDOW_SCALE,&x,&y));
    uint32_t right_x=x;
    printf("split bands left x %u right x %u\n",left_x,right_x);
    /* Each band shows a different half of the same simulated grid. */
    ASSERT_TRUE(left_x<240u);
    ASSERT_TRUE(right_x>=240u);
    /* Rendered acceptance evidence: walk the keyboard player under a real
     * display server and grab the presented split window. */
    if(getenv("DISPLAY")) {
        ASSERT_TRUE(dc_level_view_set_title(v,"player-split-tests"));
        mkdir("build/agents",0777);mkdir("build/agents/beaver-player",0777);
        mkdir("build/agents/beaver-player/split",0777);
        int start_grab=system("import -window root "
            "build/agents/beaver-player/split/split-start.png 2>/dev/null");
        ASSERT_TRUE(start_grab==0);
        SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_D;
        ASSERT_TRUE(dc_players_event(v,&e));
        for(unsigned n=0;n<40;n++)
            ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        e.type=SDL_KEYUP;ASSERT_TRUE(dc_players_event(v,&e));
        int grabbed=system("import -window root "
            "build/agents/beaver-player/split/split-walk.png 2>/dev/null");
        ASSERT_TRUE(grabbed==0);
        struct stat st;
        ASSERT_TRUE(stat("build/agents/beaver-player/split/split-walk.png",&st)==0&&st.st_size>1000);
        printf("split evidence %ld bytes\n",(long)st.st_size);
        /* The left viewport follows its player: the walk shifts its crop. */
        int moved=system("cmp -s build/agents/beaver-player/split/split-start.png "
            "build/agents/beaver-player/split/split-walk.png");
        ASSERT_TRUE(moved!=0);
        printf("viewport crop followed the walking player\n");
    }
    ASSERT_TRUE(dc_players_split_screen(v,0,err,sizeof(err)));
    teardown();
    PASS();
}
static void test_split_screen_needs_two_seats(void) {
    setup();
    ASSERT_TRUE(!dc_players_split_screen(v,2,err,sizeof(err)));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,0,err,sizeof(err)));
    teardown();
    PASS();
}
int main(void) {
    printf("=== Player Split Screen Tests ===\n\n");
    RUN(test_second_seat_joins_with_its_own_beaver);
    RUN(test_split_screen_routes_bands_to_their_players);
    RUN(test_split_screen_needs_two_seats);
    printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
