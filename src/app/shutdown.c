#define _POSIX_C_SOURCE 200809L
#include "shutdown.h"

#include <signal.h>
#include <string.h>

static volatile sig_atomic_t shutdown_requested;

static void dc_app_record_shutdown(int signal_number) {
    (void)signal_number;
    shutdown_requested = 1;
}

void dc_app_install_shutdown_handlers(void) {
    struct sigaction request;
    memset(&request, 0, sizeof(request));
    request.sa_handler = dc_app_record_shutdown;
    sigemptyset(&request.sa_mask);
    sigaction(SIGTERM, &request, NULL);
    sigaction(SIGINT, &request, NULL);
}

bool dc_app_shutdown_requested(void) {
    return shutdown_requested != 0;
}

bool dc_app_consume_shutdown_request(void) {
    int pending = shutdown_requested;
    shutdown_requested = 0;
    return pending != 0;
}
