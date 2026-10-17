#define main particle_full_suite_main
#include "../../tests/vulkan/particle_tests.c"
#undef main

int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc!=2||strcmp(argv[1],"connectivity")!=0) return 2;
    RUN(test_winding_dirt_component_has_one_gpu_label);
    RUN(test_large_wet_dirt_component_crosses_seam_as_mud);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
