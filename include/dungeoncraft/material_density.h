#ifndef DUNGEONCRAFT_MATERIAL_DENSITY_H
#define DUNGEONCRAFT_MATERIAL_DENSITY_H

/* Shared by C material profiles and Vulkan shaders; water has density one. */
#define DC_DENSITY_STONE 2.7
#define DC_DENSITY_WOOD 0.6
#define DC_DENSITY_DENSE_WOOD 1.2
#define DC_DENSITY_MORTAR 1.6
#define DC_DENSITY_LEAVES 0.2
#define DC_DENSITY_ASH 0.25
#define DC_DENSITY_COAL 1.3
#define DC_DENSITY_CHAR 0.35
#define DC_DENSITY_DIRT 1.2
#define DC_DENSITY_SAND 2.65
#define DC_DENSITY_GOLD 19.3
#define DC_DENSITY_MAGIC_INGREDIENT 0.7
#define DC_DENSITY_CORROSION_RESIDUE 2.4

/* Pixel body material IDs are independent of terrain material IDs. */
#define DC_BODY_MATERIAL_MAX 15u
#define DC_BODY_WOOD_MATERIAL 2u
#define DC_BODY_DENSITY(m) ((m)==10u?DC_DENSITY_COAL:(m)==11u?DC_DENSITY_CHAR:(m)==12u?DC_DENSITY_GOLD:(m)==13u?DC_DENSITY_MAGIC_INGREDIENT:(m)==14u?DC_DENSITY_CORROSION_RESIDUE:(m)==15u?DC_DENSITY_ASH:(m)==2u?DC_DENSITY_WOOD:(m)==3u?DC_DENSITY_MORTAR:(m)==4u?DC_DENSITY_DENSE_WOOD:(m)==5u||(m)==9u?DC_DENSITY_LEAVES:(m)==6u?DC_DENSITY_SAND:(m)==7u?DC_DENSITY_DIRT:DC_DENSITY_STONE)
#define DC_TERRAIN_TO_BODY(m) ((m)==18u?12u:(m)==19u?13u:(m)==17u?14u:(m)==13u?15u:(m)==11u?10u:(m)==12u?11u:(m)==7u?2u:(m)==8u?4u:(m)==9u?5u:(m)==2u?6u:(m)==4u?7u:(m)==5u?8u:(m)==6u?3u:(m)==10u?9u:1u)
#define DC_BODY_TO_TERRAIN(m) ((m)==12u?18u:(m)==13u?19u:(m)==14u?17u:(m)==15u?13u:(m)==10u?11u:(m)==11u?12u:(m)==1u?1u:(m)==2u?7u:(m)==3u?6u:(m)==4u?8u:(m)==5u?9u:(m)==6u?2u:(m)==7u?4u:(m)==8u?5u:(m)==9u?10u:0u)

#endif
