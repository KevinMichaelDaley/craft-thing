#ifndef DUNGEONCRAFT_TERRAIN_FLAGS_H
#define DUNGEONCRAFT_TERRAIN_FLAGS_H

/** Shared C/GLSL flags in the high half of a packed material/flags word. */
#define DC_CELL_LOADED_TERRAIN 0x100u
#define DC_CELL_BONDS_READY 0x200u
#define DC_CELL_BOND_MASK 0x3c00u
#define DC_CELL_RELEASED_TERRAIN 0x4000u
#define DC_CELL_DIAGONAL_BOND_MASK 0x000fu
#define DC_CELL_DIAGONAL_BONDS_READY 0x0010u
#define DC_CELL_SUPPORT_BONDS_READY 0x0020u
#define DC_CELL_ALL_BOND_MASK (DC_CELL_BOND_MASK | DC_CELL_DIAGONAL_BOND_MASK)
#define DC_CELL_FLAGS_SHIFT 16u
#define DC_CELL_BOND_SHIFT 26u
#define DC_CELL_DIAGONAL_BOND_SHIFT 16u

/** Untagged rigid-capable paint can create bodies; loose grains cannot bond. */
#define DC_CELL_RIGID_CAPABLE(m) ((m)==1u||(m)==7u||(m)==8u||(m)==9u|| \
    (m)==11u||(m)==12u)
#define DC_CELL_COMPONENT_MEMBER(p) (((p)&65535u)!=0u&&((p)&65535u)!=3u&& \
    (((p)&(DC_CELL_LOADED_TERRAIN<<DC_CELL_FLAGS_SHIFT))!=0u||DC_CELL_RIGID_CAPABLE((p)&65535u)))

#endif
