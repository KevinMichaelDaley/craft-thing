#include <math.h>
#include <stddef.h>
#include "ferrum/structural/mechanics.h"

/* Synthetic gameplay coefficients in seconds: tick-normalized stresses scale by
 * 60^2. */
static const fr_structural_profile_t profiles[FR_STRUCTURAL_MATERIAL_COUNT] = {
    {2.7, 72000000, 28800000, 144000, 2880000, 360000, 43200, 1, 2, 1,
     FR_FAILURE_FRACTURE},
    {.6, 21600000, 3600000, 720000, 864000, 288000, 108000, 1.5, 3, .15,
     FR_FAILURE_SPLIT},
    {1.6, 3600000, 1440000, 28800, 144000, 36000, 7200, .75, 1, 1,
     FR_FAILURE_CRUMBLE}};

const fr_structural_profile_t *
fr_structural_profile(fr_structural_material_t material) {
    return (unsigned)material < FR_STRUCTURAL_MATERIAL_COUNT
               ? &profiles[material]
               : NULL;
}

bool fr_structural_material_map(bool terrain, uint32_t id,
                                fr_structural_material_t *out) {
    if (!out)
        return false;
    if (id == 1) {
        *out = FR_STRUCTURAL_STONE;
        return true;
    }
    if (!terrain && id == 2) {
        *out = FR_STRUCTURAL_WOOD;
        return true;
    }
    if (!terrain && id == 3) {
        *out = FR_STRUCTURAL_MORTAR;
        return true;
    }
    return false;
}

bool fr_structural_rectangle(double depth, fr_structural_section_t *out) {
    if (!out || !isfinite(depth) || depth <= 0)
        return false;
    *out = (fr_structural_section_t){depth, depth * depth * depth / 12,
                                     depth / 2, 5.0 / 6};
    return true;
}

double fr_structural_mass(const fr_structural_profile_t *p,
                          fr_structural_section_t s, double l) {
    return p && s.area > 0 && l > 0 ? p->density * s.area * l : 0;
}

bool fr_structural_joint_stiffness(fr_structural_joint_t j, double d,
                                   double out[3]) {
    if (!out || (unsigned)j.kind > FR_JOINT_MOMENT || !isfinite(d) ||
        !isfinite(j.normal_stiffness) || !isfinite(j.shear_stiffness) ||
        !isfinite(j.rotational_stiffness) || j.normal_stiffness < 0 ||
        j.shear_stiffness < 0 || j.rotational_stiffness < 0)
        return false;
    bool open = j.kind == FR_JOINT_BEARING && d > 0;
    out[0] = open ? 0 : j.normal_stiffness;
    out[1] = open ? 0 : j.shear_stiffness;
    out[2] = j.kind == FR_JOINT_BONDED || j.kind == FR_JOINT_MOMENT
                 ? j.rotational_stiffness
                 : 0;
    return true;
}

static double ratio(double force, double capacity) {
    return force == 0 ? 0 : capacity > 0 ? force / capacity : INFINITY;
}

double fr_structural_utilization(const fr_structural_profile_t *p,
                                 fr_structural_section_t s, double n, double v,
                                 double m, double length) {
    if (!p || s.area <= 0 || s.inertia <= 0 || length <= 0)
        return INFINITY;
    double bending = fabs(m) * s.extreme / s.inertia;
    double hi = n / s.area + bending, lo = n / s.area - bending;
    double u = fmax(ratio(fmax(hi, 0), p->tension),
                    ratio(fmax(-lo, 0), p->compression));
    u = fmax(u, ratio(1.5 * fabs(v) / s.area, p->shear_strength));
    const double pi = 3.14159265358979323846;
    return fmax(u, ratio(fmax(-n, 0),
                         pi * pi * p->young * s.inertia / (length * length)));
}

double fr_structural_attachment_utilization(double normal, double shear,
                                            double moment, double bearing,
                                            double pullout, double capacity,
                                            double moment_capacity,
                                            double friction) {
    double u = normal > 0 ? ratio(normal, pullout) : ratio(-normal, bearing);
    u = fmax(u, ratio(fabs(shear), capacity + friction * fmax(-normal, 0)));
    return fmax(u, ratio(fabs(moment), moment_capacity));
}
