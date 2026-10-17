#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "../../src/vulkan/structural_store_internal.h"
#include "ferrum/renderer/gpu.h"
#include "terrain_support.h"

static VkBuffer captured_buffer;
static VkDeviceMemory captured_memory;
static void *captured;
static uint32_t capture_count;
static bool capturing;
static const VkDeviceSize contact_bytes=DC_GPU_CONTACT_HEADER_WORDS*4u+DC_GPU_CONTACT_CAPACITY*sizeof(dc_gpu_contact_t);
void __real_dc_gpu_record_contacts(dc_gpu_t *g);
void __wrap_dc_gpu_record_contacts(dc_gpu_t *g) {
    __real_dc_gpu_record_contacts(g);
    if(!capturing||capture_count>=9) return;
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT,.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT};
    VkDependencyInfo dep={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(g->command,&dep);
    VkBufferCopy copy={.srcOffset=(uint8_t*)dc_gpu_contact_data(g)-(uint8_t*)g->body_mapped,
        .dstOffset=contact_bytes*capture_count++,.size=contact_bytes};
    vkCmdCopyBuffer(g->command,g->body_buffer,captured_buffer,1,&copy);
    memory.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    memory.srcAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT;
    memory.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT;
    memory.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(g->command,&dep);
}

static void report(dc_gpu_t *g,unsigned tick) {
    dc_gpu_rigid_solver_stats_t stats;dc_gpu_contact_stats_t contacts;char err[256];
    dc_gpu_read_rigid_solver_stats(g,&stats);
    dc_gpu_read_contacts(g,&contacts,NULL,0,err,sizeof(err));
    const uint32_t *solver=dc_gpu_solver_data(g);
    printf("tick%u solver_flags=%u contacts=%u attempted=%u contact_flags=%u\n",
        tick,solver[3],contacts.count,((uint32_t*)dc_gpu_contact_data(g))[1],contacts.overflow);
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        const dc_gpu_body_record_t *b=(dc_gpu_body_record_t*)g->body_mapped+slot;
        if(!b->body.active) continue;
        unsigned wood=0;
        if(b->piece_count&DC_GPU_PIXEL_BODY_FLAG) {
            const uint32_t *shape=(const uint32_t*)g->body_mapped+b->compound_reserved;
            for(unsigned tile=0;tile<shape[1];tile++) for(unsigned cell=0;cell<256;cell++) {
                unsigned m=(shape[8+shape[2]*shape[3]+tile*33+1+cell/8]>>((cell%8)*4))&15u;
                wood+=m==2||m==4;
            }
        }
        printf("  slot%u id%08x pos %.4f %.4f v %.4f %.4f angle %.4f flags%u rest%u visible%u invmass%g pixels%u awake%u support%u\n",
            slot,b->body.id,b->body.x_fp/65536.,b->body.y_fp/65536.,
            b->body.vx_fp/65536.,b->body.vy_fp/65536.,b->angle,b->motion_flags,
            b->rest_ticks,b->visible,b->inverse_mass,b->piece_count&~DC_GPU_PIXEL_BODY_FLAG,
            solver[16+slot*32+13],solver[16+slot*32+19]);
        printf("    shape %ux%u center%g,%g wood%u mass%g\n",b->body.width,b->body.height,b->center[0],b->center[1],wood,b->mass);
    }
    const dc_gpu_contact_t *list=(const dc_gpu_contact_t*)(dc_gpu_contact_data(g)+DC_GPU_CONTACT_HEADER_WORDS);
    for(unsigned i=0;i<contacts.count;i++) if(list[i].body_a==0x80000001u||list[i].body_b==0x80000001u)
        printf("    tree contact %08x %08x kind%u normal%g,%g depth%g\n",list[i].body_a,list[i].body_b,list[i].kind,list[i].normal_x,list[i].normal_y,list[i].depth);
    for(unsigned phase=0;phase<capture_count;phase++) {
        const uint32_t *data=(const uint32_t*)((const uint8_t*)captured+phase*contact_bytes);
        list=(const dc_gpu_contact_t*)(data+DC_GPU_CONTACT_HEADER_WORDS);
        for(unsigned i=0;i<data[0];i++) if(list[i].body_a==0x80000001u||list[i].body_b==0x80000001u)
            printf("    phase%u tree %08x %08x kind%u material%u normal%g,%g depth%g terrain %lld,%lld cell%u point %lld,%lld:%d,%d\n",phase,
                list[i].body_a,list[i].body_b,list[i].kind,list[i].material_b,list[i].normal_x,list[i].normal_y,list[i].depth,
                (long long)list[i].feature_chunk.x,(long long)list[i].feature_chunk.y,list[i].feature_b,
                (long long)list[i].anchor_b.chunk.x,(long long)list[i].anchor_b.chunk.y,
                list[i].anchor_b.x_fp,list[i].anchor_b.y_fp);
    }
}
int main(int argc,char **argv) {
    if(argc<2||argc%2!=0) {fprintf(stderr,"Usage: %s rigid_bodies.bin [erase_world_x erase_world_y]... (in-memory replay only)\n",argv[0]);return 2;}
    FILE *file=fopen(argv[1],"rb");dc_structural_store_header_t header;
    if(!file) return 2;
    bool okay=fread(&header,sizeof(header),1,file)==1;fclose(file);if(!okay) return 2;
    char err[256]={0};dc_gpu_t *g=NULL;
    okay=dc_gpu_create(&g,512,448,"build/shaders/pattern.comp.spv",err,sizeof(err)) &&
        dc_gpu_set_body_origin(g,header.origin,err,sizeof(err)) &&
        fr_gpu_load_structure(g,argv[1],err,sizeof(err));
    unsigned slot=0;
    for(unsigned i=0;okay&&i<g->checkpoint_chunk_count;) {
        dc_chunk_coord_t coord=g->checkpoint_chunks[i].coord;
        int64_t x=coord.x-header.origin.x,y=coord.y-header.origin.y;
        if(x<0||y<0||x>=8||y>=7) {i++;continue;}
        dc_chunk_t *chunk=calloc(1,sizeof(*chunk));if(!chunk) {okay=false;break;}
        chunk->coord=coord;
        okay=dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)) &&
            dc_gpu_set_page(g,(uint32_t)x,(uint32_t)y,slot,err,sizeof(err));
        free(chunk);slot++;
    }
    okay=okay&&dc_gpu_enable_automatic_components(g)&&dc_gpu_set_body_origin(g,header.origin,err,sizeof(err));
    if(okay) {report(g,0);okay=report_terrain_support(g,err,sizeof(err));}
    for(int arg=2;okay&&arg<argc;arg+=2) {
        long double x=strtoll(argv[arg],NULL,10)-(long double)header.origin.x*64;
        long double y=strtoll(argv[arg+1],NULL,10)-(long double)header.origin.y*64;
        okay=x>=0&&x<512&&y>=0&&y<448&&
            dc_gpu_paint_material(g,(uint32_t)x,(uint32_t)y,0,DC_MATERIAL_AIR,err,sizeof(err));
    }
    okay=okay&&dc_gpu_make_mapped_buffer(g,contact_bytes*9,&captured_buffer,&captured_memory,&captured,err,sizeof(err));
    for(unsigned tick=1;okay&&tick<=8;tick++) {
        capturing=true;capture_count=0;
        okay=dc_gpu_rigid_step(g,err,sizeof(err));if(okay) report(g,tick);
    }
    if(!okay) fprintf(stderr,"%s\n",err);
    if(okay) okay=report_terrain_support(g,err,sizeof(err));
    if(okay&&fr_renderer_enable(g,err,sizeof(err))&&fr_renderer_set_mode(g,FR_RENDERER_ALBEDO)&&fr_renderer_render(g,err,sizeof(err))) {
        uint32_t *pixels=calloc(512u*448u,4);
        if(pixels&&dc_gpu_readback(g,pixels,512u*448u,err,sizeof(err))) {
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,512,448,32,512*4,SDL_PIXELFORMAT_ABGR8888);
            if(surface) {
                SDL_SaveBMP(surface,"build/support_scene.bmp");
                const uint32_t *pages=g->page_mapped;const dc_cell_t *atlas=g->chunk_mapped;
                for(unsigned y=0;y<448;y++) for(unsigned x=0;x<512;x++) {
                    unsigned page=pages[(y/64)*g->page_width+x/64];
                    if(!page) continue;
                    unsigned material=atlas[(page-1)*4096+(y%64)*64+x%64].material;
                    if(material==7||material==8) pixels[y*512+x]=0xffff00ffu;
                }
                SDL_SaveBMP(surface,"build/support_fixed_wood.bmp");
                SDL_Surface *detail=SDL_CreateRGBSurfaceWithFormat(0,400,400,32,SDL_PIXELFORMAT_ABGR8888);
                SDL_Rect crop={280,160,100,100};
                if(detail) {SDL_BlitScaled(surface,&crop,detail,NULL);SDL_SaveBMP(detail,"build/support_tree_detail.bmp");SDL_FreeSurface(detail);}
                SDL_FreeSurface(surface);
            }
        }
        free(pixels);
    }
    if(captured) vkUnmapMemory(g->device,captured_memory);
    if(captured_buffer) vkDestroyBuffer(g->device,captured_buffer,NULL);
    if(captured_memory) vkFreeMemory(g->device,captured_memory,NULL);
    dc_gpu_destroy(g);return okay?0:1;
}
