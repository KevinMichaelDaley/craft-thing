#include <stdio.h>
#include <string.h>

#include "../../src/app/smoke_profile.h"

extern char **environ;
int main(void) {
    if(!dc_smoke_profile_defaults()) return 1;
    for(char **p=environ;*p;p++) if(!strncmp(*p,"DC_GAS_",7)) puts(*p);
    return 0;
}
