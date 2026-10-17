#ifndef DC_DIAGNOSTIC_TERRAIN_SUPPORT_H
#define DC_DIAGNOSTIC_TERRAIN_SUPPORT_H

static bool support_solid(unsigned material) {return material==1||material==7||material==8;}
static bool report_terrain_support(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!dc_gpu_copy_chunk_state(g,UINT32_MAX,false,false,err,cap)) return false;
    size_t area=(size_t)g->width*g->height;
    dc_cell_t *cells=calloc(area,sizeof(*cells));
    uint8_t *seen=calloc(area,1);uint32_t *queue=calloc(area,sizeof(*queue));
    if(!cells||!seen||!queue) {free(cells);free(seen);free(queue);return false;}
    const uint32_t *pages=g->page_mapped;
    const dc_cell_t *atlas=g->chunk_mapped;
    for(unsigned y=0;y<g->height;y++) for(unsigned x=0;x<g->width;x++) {
        uint32_t page=pages[(y/64)*g->page_width+x/64];
        if(page) cells[(size_t)y*g->width+x]=atlas[(page-1)*4096+(y%64)*64+x%64];
    }
    const int dx[4]={-1,1,0,0},dy[4]={0,0,-1,1};
    for(size_t start=0;start<area;start++) {
        if(seen[start]||!support_solid(cells[start].material)) continue;
        unsigned head=0,tail=1,wood=0,loaded=0,released=0,broken=0,cold=0;
        unsigned lx=g->width,ly=g->height,hx=0,hy=0;queue[0]=(uint32_t)start;seen[start]=1;
        while(head<tail) {
            unsigned at=queue[head++],x=at%g->width,y=at/g->width;
            if(x<lx) lx=x;
            if(y<ly) ly=y;
            if(x>hx) hx=x;
            if(y>hy) hy=y;
            dc_cell_t cell=cells[at];unsigned bonds=0;
            wood+=cell.material==7||cell.material==8;
            loaded+=(cell.flags&DC_CELL_LOADED_TERRAIN)!=0;
            released+=(cell.flags&DC_CELL_RELEASED_TERRAIN)!=0;
            for(unsigned axis=0;axis<4;axis++) {
                int nx=(int)x+dx[axis],ny=(int)y+dy[axis];
                if(nx<0||ny<0||nx>=(int)g->width||ny>=(int)g->height||!pages[(ny/64)*g->page_width+nx/64]) {cold++;bonds|=cell.flags&(1u<<(10+axis));continue;}
                unsigned next=(unsigned)ny*g->width+(unsigned)nx;
                if(!support_solid(cells[next].material)) continue;
                bonds|=1u<<(10+axis);
                if(!seen[next]) {seen[next]=1;queue[tail++]=next;}
            }
            broken+=(cell.flags&DC_CELL_BONDS_READY)!=0&&((cell.flags&DC_CELL_BOND_MASK)&~bonds)!=0;
        }
        if(wood) printf("terrain component cells%u wood%u bounds %.0Lf,%.0Lf..%.0Lf,%.0Lf loaded%u released%u broken%u unknown_faces%u\n",
            tail,wood,(long double)g->body_origin.x*64+lx,(long double)g->body_origin.y*64+ly,
            (long double)g->body_origin.x*64+hx,(long double)g->body_origin.y*64+hy,loaded,released,broken,cold);
    }
    free(cells);free(seen);free(queue);return true;
}
#endif
