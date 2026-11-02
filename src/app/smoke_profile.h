#ifndef DC_APP_SMOKE_PROFILE_H
#define DC_APP_SMOKE_PROFILE_H
#include <stdbool.h>
/** Apply the same process-local defaults as tools/smoke_profile. Explicit
 * caller overrides are retained; no files or global machine settings change. */
bool dc_smoke_profile_defaults(void);
#endif
