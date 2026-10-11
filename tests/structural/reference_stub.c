#include "ferrum/structural/mechanics.h"
const fr_structural_profile_t *fr_structural_profile(fr_structural_material_t m) {(void)m; return 0;}
bool fr_structural_material_map(bool t,uint32_t i,fr_structural_material_t *o){(void)t;(void)i;(void)o;return false;}
bool fr_structural_rectangle(double d,fr_structural_section_t *o){(void)d;(void)o;return false;}
bool fr_structural_reference_solve(const fr_structural_frame_t *f,fr_structural_solution_t *o){(void)f;(void)o;return false;}
double fr_structural_utilization(const fr_structural_profile_t *p,fr_structural_section_t s,double n,double v,double m,double l){(void)p;(void)s;(void)n;(void)v;(void)m;(void)l;return 0;}
double fr_structural_attachment_utilization(double n,double s,double m,double b,double p,double c,double r,double f){(void)n;(void)s;(void)m;(void)b;(void)p;(void)c;(void)r;(void)f;return 0;}
