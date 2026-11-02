/** Export the tested native CPRP placeholder and its canonical mapping for review.
 * This is an asset contact sheet, not a runtime Vulkan acceptance capture. */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL.h>

#include "ferrum/player/beaver.h"

enum { ATLAS_SCALE=4, ATLAS_COLUMNS=8,
       ATLAS_WIDTH=FR_BEAVER_CAST_WIDTH*ATLAS_COLUMNS*ATLAS_SCALE,
       ATLAS_HEIGHT=FR_BEAVER_HEIGHT*FR_PLAYER_ACTION_COUNT*ATLAS_SCALE };

static bool beaver_write_file(const char *path,const void *data,size_t size) {
    FILE *file=fopen(path,"wb");
    if (!file) return false;
    bool okay=fwrite(data,1,size,file)==size;
    return fclose(file)==0 && okay;
}

static uint32_t beaver_channel(float linear) {
    float value=linear<=.0031308f?linear*12.92f:1.055f*powf(linear,1.f/2.4f)-.055f;
    return (uint32_t)lroundf(fminf(1.f,fmaxf(0.f,value))*255.f);
}

int main(int argc,char **argv) {
    const char *directory=argc>1?argv[1]:"build/beaver_assets";
    if (mkdir(directory,0777)!=0 && errno!=EEXIST) {perror(directory);return 1;}
    uint32_t *atlas=malloc((size_t)ATLAS_WIDTH*ATLAS_HEIGHT*sizeof(*atlas));
    if (!atlas) return 1;
    for (uint32_t y=0;y<ATLAS_HEIGHT;y++) for (uint32_t x=0;x<ATLAS_WIDTH;x++)
        atlas[y*ATLAS_WIDTH+x]=((x/16+y/16)%2)?0xffd0d0d0u:0xffb8b8b8u;
    const char *names[]={"idle","walk","chew","cast"};
    bool okay=true;char path[1024],err[200]={0};
    for (uint32_t a=0;a<FR_PLAYER_ACTION_COUNT && okay;a++) {
        size_t size=0;uint32_t entries=0;
        okay=fr_beaver_asset_size((fr_player_action_t)a,&size,&entries);
        uint8_t *bytes=okay?malloc(size):NULL;
        uint32_t *mapping=okay?malloc((size_t)entries*sizeof(*mapping)):NULL;
        fr_beaver_asset_t asset;
        okay=bytes && mapping && fr_beaver_asset_build((fr_player_action_t)a,bytes,size,
            mapping,entries,&asset,err,sizeof(err));
        if (okay) {
            int n=snprintf(path,sizeof(path),"%s/%s.cprop",directory,names[a]);
            okay=n>0 && (size_t)n<sizeof(path) && beaver_write_file(path,bytes,size);
        }
        if (okay) {
            uint8_t *encoded=malloc((size_t)entries*4);
            if (!encoded) okay=false;
            else {
                for (uint32_t i=0;i<entries;i++) for (uint32_t b=0;b<4;b++)
                    encoded[i*4+b]=(uint8_t)(mapping[i]>>(b*8));
                int n=snprintf(path,sizeof(path),"%s/%s.source.u32",directory,names[a]);
                okay=n>0 && (size_t)n<sizeof(path) && beaver_write_file(path,encoded,(size_t)entries*4);
                free(encoded);
            }
        }
        for (uint32_t f=0;okay && f<asset.prop.frame_count;f++) for (uint32_t p=0;p<asset.prop.width*asset.prop.height;p++) {
            fr_prop_appearance_t app;
            okay=fr_prop_appearance(&asset.prop,f,p,&app);
            if (!okay || app.alpha==0) continue;
            uint32_t color=0xff000000u|beaver_channel(app.color[0])|(beaver_channel(app.color[1])<<8)|(beaver_channel(app.color[2])<<16);
            uint32_t ax=(f*FR_BEAVER_CAST_WIDTH+p%asset.prop.width)*ATLAS_SCALE;
            uint32_t ay=(a*FR_BEAVER_HEIGHT+p/asset.prop.width)*ATLAS_SCALE;
            for (uint32_t y=0;y<ATLAS_SCALE;y++) for (uint32_t x=0;x<ATLAS_SCALE;x++)
                atlas[(ay+y)*ATLAS_WIDTH+ax+x]=color;
        }
        if (okay) printf("%s: %u frames, %zu compiled bytes; canonical source mapping exported\n",names[a],asset.prop.frame_count,size);
        free(bytes);free(mapping);
    }
    if (okay) {
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(atlas,ATLAS_WIDTH,ATLAS_HEIGHT,
            32,ATLAS_WIDTH*4,SDL_PIXELFORMAT_ABGR8888);
        int n=snprintf(path,sizeof(path),"%s/contact_sheet.bmp",directory);
        okay=surface && n>0 && (size_t)n<sizeof(path) && SDL_SaveBMP(surface,path)==0;
        if (surface) SDL_FreeSurface(surface);
    }
    free(atlas);
    if (!okay) fprintf(stderr,"Beaver asset export failed: %s\n",err);
    return okay?0:1;
}
