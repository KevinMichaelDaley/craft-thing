#ifndef DUNGEONCRAFT_TERRAIN_FLAGS_H
#define DUNGEONCRAFT_TERRAIN_FLAGS_H

/** Shared C/GLSL flags in the high half of a packed material/flags word. */
#define DC_CELL_LOADED_TERRAIN 0x100u
#define DC_CELL_BONDS_READY 0x200u
#define DC_CELL_BOND_MASK 0x3c00u
#define DC_CELL_RELEASED_TERRAIN 0x4000u
#define DC_CELL_FLAGS_SHIFT 16u
#define DC_CELL_BOND_SHIFT 26u

#endif
