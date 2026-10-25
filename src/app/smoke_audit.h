#ifndef DC_APP_SMOKE_AUDIT_H
#define DC_APP_SMOKE_AUDIT_H
#include "level.h"
/** Capture a cold, GPU-reduced benchmark diagnostic outside measured frames. */
bool dc_smoke_audit_capture(dc_level_view_t *view,const char *phase,fr_gas_gpu_audit_t *out,char *err,uint32_t cap);
/** Check meaningful stock and practical finite-state budgets after a closed run. */
bool dc_smoke_audit_compare(const fr_gas_gpu_audit_t *before,const fr_gas_gpu_audit_t *after,char *err,uint32_t cap);
#endif
