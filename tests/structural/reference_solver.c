#include <math.h>
#include <string.h>
#include "ferrum/structural/mechanics.h"

#define DOFS (3u*FR_STRUCTURAL_MAX_NODES)

static bool element_matrix(const fr_structural_frame_t *f, const fr_structural_element_t *e,
                           double k[6][6]) {
    if (e->a >= f->node_count || e->b >= f->node_count || e->a == e->b ||
        !isfinite(e->young) || e->young <= 0 || e->section.area <= 0) return false;
    double dx = f->nodes[e->b].x-f->nodes[e->a].x;
    double dy = f->nodes[e->b].y-f->nodes[e->a].y;
    double l = hypot(dx,dy), local[6][6] = {{0}}, t[6][6] = {{0}};
    if (!isfinite(l) || l <= 0) return false;
    double axial = e->young*e->section.area/l;
    local[0][0] = local[3][3] = axial;
    local[0][3] = local[3][0] = -axial;
    if (!e->truss) {
        if (e->shear <= 0 || e->section.inertia <= 0 || e->section.shear_correction <= 0) return false;
        double ei = e->young*e->section.inertia;
        double phi = 12*ei/(e->section.shear_correction*e->shear*e->section.area*l*l);
        double a = 12*ei/((1+phi)*l*l*l), b = a*l/2;
        double c = (4+phi)*ei/((1+phi)*l), d = (2-phi)*ei/((1+phi)*l);
        unsigned indices[4] = {1,2,4,5};
        double bending[4][4] = {{a,b,-a,b},{b,c,-b,d},{-a,-b,a,-b},{b,d,-b,c}};
        for (unsigned i=0;i<4;i++) for(unsigned j=0;j<4;j++) local[indices[i]][indices[j]]=bending[i][j];
    }
    for (unsigned o=0;o<6;o+=3) {
        t[o][o]=dx/l; t[o][o+1]=dy/l;
        t[o+1][o]=-dy/l; t[o+1][o+1]=dx/l; t[o+2][o+2]=1;
    }
    for(unsigned i=0;i<6;i++) for(unsigned j=0;j<6;j++) {
        k[i][j]=0;
        for(unsigned a=0;a<6;a++) for(unsigned b=0;b<6;b++) k[i][j]+=t[a][i]*local[a][b]*t[b][j];
    }
    return true;
}

bool fr_structural_reference_solve(const fr_structural_frame_t *f, fr_structural_solution_t *out) {
    if (!f || !out || !f->node_count || f->node_count > FR_STRUCTURAL_MAX_NODES ||
        f->element_count > FR_STRUCTURAL_MAX_ELEMENTS) return false;
    double k[DOFS][DOFS]={{0}}, a[DOFS][DOFS]={{0}}, rhs[DOFS]={0}, q[DOFS]={0};
    unsigned count=3*f->node_count, free_dof[DOFS], nf=0;
    for(unsigned i=0;i<f->element_count;i++) {
        double ek[6][6]; const fr_structural_element_t *e=&f->elements[i];
        if (!element_matrix(f,e,ek)) return false;
        unsigned map[6]={3*e->a,3*e->a+1,3*e->a+2,3*e->b,3*e->b+1,3*e->b+2};
        for(unsigned r=0;r<6;r++) for(unsigned c=0;c<6;c++) k[map[r]][map[c]]+=ek[r][c];
    }
    for(unsigned i=0;i<count;i++) {
        if (!isfinite(f->nodes[i/3].force[i%3])) return false;
        /* Truss-only junctions have no rotational variable, rather than a fake rotational spring. */
        if (i%3 == 2 && k[i][i] == 0 && f->nodes[i/3].force[2] == 0) continue;
        if (!(f->nodes[i/3].fixed & (1u<<(i%3)))) free_dof[nf++]=i;
    }
    double scale=0;
    for(unsigned i=0;i<nf;i++) {
        rhs[i]=f->nodes[free_dof[i]/3].force[free_dof[i]%3];
        for(unsigned j=0;j<nf;j++) a[i][j]=k[free_dof[i]][free_dof[j]];
        scale=fmax(scale,fabs(a[i][i]));
    }
    for(unsigned col=0;col<nf;col++) {
        unsigned pivot=col;
        for(unsigned r=col+1;r<nf;r++) if(fabs(a[r][col])>fabs(a[pivot][col])) pivot=r;
        if(fabs(a[pivot][col])<=scale*1e-12) return false;
        if(pivot!=col) {
            for(unsigned j=0;j<nf;j++){double v=a[col][j];a[col][j]=a[pivot][j];a[pivot][j]=v;}
            double v=rhs[col];rhs[col]=rhs[pivot];rhs[pivot]=v;
        }
        for(unsigned r=col+1;r<nf;r++) {
            double factor=a[r][col]/a[col][col];
            for(unsigned j=col;j<nf;j++) a[r][j]-=factor*a[col][j];
            rhs[r]-=factor*rhs[col];
        }
    }
    for(unsigned i=nf;i-->0;) {
        double v=rhs[i];for(unsigned j=i+1;j<nf;j++) v-=a[i][j]*q[free_dof[j]];
        q[free_dof[i]]=v/a[i][i];
    }
    fr_structural_solution_t result={0};
    for(unsigned i=0;i<count;i++) {
        double reaction=-f->nodes[i/3].force[i%3];
        for(unsigned j=0;j<count;j++) reaction+=k[i][j]*q[j];
        result.displacement[i/3][i%3]=q[i];
        result.reaction[i/3][i%3]=reaction;
        if(!(f->nodes[i/3].fixed&(1u<<(i%3)))) result.residual=fmax(result.residual,fabs(reaction));
    }
    *out=result;return true;
}
