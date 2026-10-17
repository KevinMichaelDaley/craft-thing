#include <stdio.h>
#include "native_demo_controls.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_escape_and_close_end_demo(void) {
    bool running=true,paused=false;
    SDL_Event event={.type=SDL_KEYDOWN};event.key.keysym.sym=SDLK_ESCAPE;
    native_demo_event(&event,&running,&paused);ASSERT_TRUE(!running);
    running=true;event.type=SDL_QUIT;
    native_demo_event(&event,&running,&paused);ASSERT_TRUE(!running);PASS();
}
static void test_space_toggles_pause_without_key_repeat(void) {
    bool running=true,paused=false;
    SDL_Event event={.type=SDL_KEYDOWN};event.key.keysym.sym=SDLK_SPACE;
    native_demo_event(&event,&running,&paused);ASSERT_TRUE(paused&&running);
    event.key.repeat=1;native_demo_event(&event,&running,&paused);ASSERT_TRUE(paused);
    event.key.repeat=0;native_demo_event(&event,&running,&paused);ASSERT_TRUE(!paused);PASS();
}
int main(void) {RUN(test_escape_and_close_end_demo);RUN(test_space_toggles_pause_without_key_repeat);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
