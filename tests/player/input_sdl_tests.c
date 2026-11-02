#include <stdio.h>

#include "ferrum/player/input_sdl.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_keyboard_context_and_repeat(void) {
    fr_input_t i;fr_input_state_t s;fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,1));
    ASSERT_TRUE(fr_input_connect(&i,FR_INPUT_KEYBOARD_DEVICE,1));
    ASSERT_TRUE(fr_input_sdl_defaults(&i,FR_INPUT_KEYBOARD_DEVICE,false));
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_Q;
    ASSERT_TRUE(fr_input_sdl_event(&i,&e));ASSERT_TRUE(fr_input_sample(&i,1,&s));
    ASSERT_EQ(s.pressed,1u<<FR_INPUT_FIRE);
    e.key.repeat=1;ASSERT_TRUE(fr_input_sdl_event(&i,&e));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,0);
    e.type=SDL_KEYUP;e.key.repeat=0;ASSERT_TRUE(fr_input_sdl_event(&i,&e));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.released,1u<<FR_INPUT_FIRE);
    ASSERT_TRUE(fr_input_context(&i,1,FR_INPUT_EDITOR));e.type=SDL_KEYDOWN;
    ASSERT_TRUE(!fr_input_sdl_event(&i,&e));ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,0);
    PASS();
}
static void test_controller_axis_button_instance_ownership(void) {
    fr_input_t i;fr_input_state_t s;fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,1));ASSERT_TRUE(fr_input_add_player(&i,2));
    ASSERT_TRUE(fr_input_connect(&i,900,2));ASSERT_TRUE(fr_input_sdl_defaults(&i,900,true));
    SDL_Event e={.type=SDL_CONTROLLERAXISMOTION};e.caxis.which=900;
    e.caxis.axis=SDL_CONTROLLER_AXIS_LEFTX;e.caxis.value=-32768;
    ASSERT_TRUE(fr_input_sdl_event(&i,&e));ASSERT_TRUE(fr_input_sample(&i,2,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],-1);
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],0);
    e.caxis.value=3000;ASSERT_TRUE(fr_input_sdl_event(&i,&e));
    ASSERT_TRUE(fr_input_sample(&i,2,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],0);
    e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=900;e.cbutton.button=SDL_CONTROLLER_BUTTON_A;
    ASSERT_TRUE(fr_input_sdl_event(&i,&e));ASSERT_TRUE(fr_input_sample(&i,2,&s));ASSERT_EQ(s.pressed,1u<<FR_INPUT_JUMP);
    e.type=SDL_CONTROLLERDEVICEREMOVED;e.cdevice.which=900;
    fr_input_sdl_event(&i,&e);ASSERT_TRUE(fr_input_sample(&i,2,&s));ASSERT_EQ(s.values[FR_INPUT_JUMP],0);
    e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=900;e.cbutton.button=SDL_CONTROLLER_BUTTON_A;
    ASSERT_TRUE(!fr_input_sdl_event(&i,&e));PASS();
}
static void test_focus_and_configured_binding(void) {
    fr_input_t i;fr_input_state_t s;fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,1));ASSERT_TRUE(fr_input_connect(&i,-1,1));
    ASSERT_TRUE(fr_input_bind(&i,-1,SDL_SCANCODE_Z,FR_INPUT_CHEW,1));
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_Z;
    ASSERT_TRUE(fr_input_sdl_event(&i,&e));
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;fr_input_sdl_event(&i,&e);
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.values[FR_INPUT_CHEW],0);ASSERT_EQ(s.pressed,0);
    PASS();
}
int main(void) {
    RUN(test_keyboard_context_and_repeat);RUN(test_controller_axis_button_instance_ownership);
    RUN(test_focus_and_configured_binding);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
