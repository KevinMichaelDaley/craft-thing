#ifndef FERRUM_STRUCTURAL_MECHANICS_H
#define FERRUM_STRUCTURAL_MECHANICS_H
#include <stdbool.h>
#include <stdint.h>

#define FR_STRUCTURAL_MAX_NODES 32u
#define FR_STRUCTURAL_MAX_ELEMENTS 64u
typedef enum { FR_STRUCTURAL_STONE, FR_STRUCTURAL_WOOD, FR_STRUCTURAL_MORTAR, FR_STRUCTURAL_MATERIAL_COUNT } fr_structural_material_t;
typedef enum { FR_FAILURE_FRACTURE, FR_FAILURE_SPLIT, FR_FAILURE_CRUMBLE } fr_structural_failure_t;
typedef enum { FR_JOINT_BEARING, FR_JOINT_PINNED, FR_JOINT_BONDED, FR_JOINT_MOMENT } fr_structural_joint_kind_t;
typedef struct {
    fr_structural_joint_kind_t kind;
    double normal_stiffness, shear_stiffness, rotational_stiffness;
    double bearing_capacity, pullout_capacity, shear_capacity, moment_capacity, friction;
} fr_structural_joint_t;
typedef struct {
    double density, young, shear, tension, compression, shear_strength;
    double fracture_energy, grace_seconds, overload_seconds, grain_ratio;
    fr_structural_failure_t failure;
} fr_structural_profile_t;
typedef struct { double area, inertia, extreme, shear_correction; } fr_structural_section_t;
typedef struct { double x, y, force[3]; uint32_t fixed; } fr_structural_node_t;
typedef struct { uint32_t a, b; double young, shear; fr_structural_section_t section; bool truss; } fr_structural_element_t;
typedef struct {
    uint32_t node_count, element_count;
    fr_structural_node_t nodes[FR_STRUCTURAL_MAX_NODES];
    fr_structural_element_t elements[FR_STRUCTURAL_MAX_ELEMENTS];
} fr_structural_frame_t;
typedef struct { double displacement[FR_STRUCTURAL_MAX_NODES][3], reaction[FR_STRUCTURAL_MAX_NODES][3]; double residual; } fr_structural_solution_t;
/** Synthetic game profiles: cells, material mass units and simulated seconds. */
const fr_structural_profile_t *fr_structural_profile(fr_structural_material_t material);
/** Map distinct terrain or rigid namespaces without numeric reinterpretation. */
bool fr_structural_material_map(bool terrain, uint32_t id, fr_structural_material_t *out);
/** Unit-thickness rectangular section derived from authored beam depth. */
bool fr_structural_rectangle(double depth, fr_structural_section_t *out);
/** Mass preserves geometry volume through numerical subdivision. */
double fr_structural_mass(const fr_structural_profile_t *profile, fr_structural_section_t section, double length);
/** Finite joint tangent stiffness; bearing opens in tension and pins release rotation. */
bool fr_structural_joint_stiffness(fr_structural_joint_t joint, double normal_displacement, double out[3]);
/** Test-only bounded dense oracle; returns false for invalid input or a mechanism. */
bool fr_structural_reference_solve(const fr_structural_frame_t *frame, fr_structural_solution_t *out);
/** Combined axial/bending, shear and Euler instability utilization. */
double fr_structural_utilization(const fr_structural_profile_t *profile, fr_structural_section_t section, double axial, double shear, double moment, double unbraced_length);
/** Finite interface capacity: compression bearing, shear/friction, tensile pull-out and moment. */
double fr_structural_attachment_utilization(double normal, double shear, double moment, double bearing, double pullout, double shear_capacity, double moment_capacity, double friction);
#endif
