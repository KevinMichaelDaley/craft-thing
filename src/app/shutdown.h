#ifndef DUNGEONCRAFT_APP_SHUTDOWN_H
#define DUNGEONCRAFT_APP_SHUTDOWN_H

#include <stdbool.h>

/** Route SIGTERM/SIGINT to a request flag for the main loop to consume. */
void dc_app_install_shutdown_handlers(void);

/** True when a termination signal requested exit since the last consume. */
bool dc_app_shutdown_requested(void);

/** Clear and report one pending termination request. */
bool dc_app_consume_shutdown_request(void);

#endif
