#define _POSIX_C_SOURCE 200809L
#include "shutdown.h"

#include <signal.h>
#include <string.h>

static void dc_app_ignore_shutdown(int signal_number) {
    (void)signal_number;
}

void dc_app_install_shutdown_handlers(void) {
    struct sigaction ignore;
    memset(&ignore, 0, sizeof(ignore));
    ignore.sa_handler = dc_app_ignore_shutdown;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGTERM, &ignore, NULL);
    sigaction(SIGINT, &ignore, NULL);
}

bool dc_app_shutdown_requested(void) {
    return false;
}

bool dc_app_consume_shutdown_request(void) {
    return false;
}
