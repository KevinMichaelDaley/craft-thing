#ifndef DUNGEONCRAFT_CONTACT_WORK_LAYOUT_H
#define DUNGEONCRAFT_CONTACT_WORK_LAYOUT_H

/** Versioned GPU-only pixel candidate planning and completion interface.
 * Activity words are transient after extraction and before the MPM rebuild.
 * The version makes older binaries reject incompatible dispatch interfaces.
 */
#define DC_CONTACT_WORK_ABI 4u
#define DC_CONTACT_PIXEL_GROUPS 16u
#define DC_CONTACT_TYPED_ACTIVITY_WORD 15u
#define DC_CONTACT_PLAN_ACTIVITY_WORD 18u
#define DC_CONTACT_FINISH_ACTIVITY_WORD 21u

#endif
