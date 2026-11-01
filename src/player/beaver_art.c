#include <string.h>

#include "beaver_internal.h"

enum { BEAVER_TAIL, BEAVER_LEFT_FOOT, BEAVER_RIGHT_FOOT, BEAVER_ROBE,
       BEAVER_HEAD, BEAVER_LEFT_ARM, BEAVER_RIGHT_ARM, BEAVER_JAW, BEAVER_HAT };

static bool beaver_rectangle(int32_t x,int32_t y,int32_t left,int32_t top,int32_t right,int32_t bottom) {
    return x>=left && x<=right && y>=top && y<=bottom;
}

static bool beaver_ellipse(int32_t x,int32_t y,int32_t cx,int32_t cy,int32_t rx,int32_t ry) {
    int32_t dx=x-cx,dy=y-cy;
    return dx*dx*ry*ry+dy*dy*rx*rx<=rx*rx*ry*ry;
}

bool beaver_art_source(uint32_t source,beaver_art_pixel_t *out) {
    if (!source || source>FR_BEAVER_PIXELS || !out) return false;
    int32_t x=(int32_t)((source-1)%FR_BEAVER_WIDTH),y=(int32_t)((source-1)/FR_BEAVER_WIDTH);
    beaver_art_pixel_t pixel={0};
    if (beaver_ellipse(x,y,8,30,6,3)) pixel=(beaver_art_pixel_t){.region=FR_BEAVER_TAIL,.part=BEAVER_TAIL};
    if (beaver_rectangle(x,y,17,33,21,35)) pixel=(beaver_art_pixel_t){.region=FR_BEAVER_FUR,.part=BEAVER_LEFT_FOOT};
    if (beaver_rectangle(x,y,25,33,29,35)) pixel=(beaver_art_pixel_t){.region=FR_BEAVER_FUR,.part=BEAVER_RIGHT_FOOT};
    if (y>=22 && y<=32 && x>=17-(y-22)/4 && x<=27+(y-22)/4)
        pixel=(beaver_art_pixel_t){.region=FR_BEAVER_ROBE,.part=BEAVER_ROBE};
    if (beaver_ellipse(x,y,24,18,6,5) || beaver_rectangle(x,y,27,18,33,21) || beaver_ellipse(x,y,19,14,2,2))
        pixel=(beaver_art_pixel_t){.region=FR_BEAVER_FUR,.part=BEAVER_HEAD};
    if (beaver_rectangle(x,y,14,24,17,28)) pixel=(beaver_art_pixel_t){.region=FR_BEAVER_FUR,.part=BEAVER_LEFT_ARM};
    if (beaver_rectangle(x,y,28,24,30,28)) pixel=(beaver_art_pixel_t){.region=FR_BEAVER_FUR,.part=BEAVER_RIGHT_ARM};
    if (beaver_rectangle(x,y,30,22,32,24)) pixel=(beaver_art_pixel_t){.region=FR_BEAVER_TEETH,.part=BEAVER_JAW};
    if ((y>=4 && y<=12 && x>=23-(y-4)/2 && x<=23+(y-4)/2) || beaver_rectangle(x,y,15,13,30,14))
        pixel=(beaver_art_pixel_t){.region=FR_BEAVER_HAT,.part=BEAVER_HAT};
    if (!pixel.region) return false;
    static const uint8_t colors[FR_BEAVER_REGION_COUNT][3]={
        {0,0,0},{147,91,49},{96,64,158},{83,48,145},{91,56,34},{240,226,187}
    };
    memcpy(pixel.color,colors[pixel.region],sizeof(pixel.color));
    int32_t shade=(x*3+y*5)%9-4;
    for (uint32_t i=0;i<3;i++) pixel.color[i]=(uint8_t)((int32_t)pixel.color[i]+shade);
    if (pixel.region==FR_BEAVER_ROBE && (x==21 || x==26)) {
        pixel.color[0]=69;pixel.color[1]=44;pixel.color[2]=119;
    }
    if ((pixel.region==FR_BEAVER_HAT && (y==11 || y==12)) ||
        (pixel.region==FR_BEAVER_ROBE && y==25 && x>=19 && x<=26)) {
        pixel.color[0]=207;pixel.color[1]=165;pixel.color[2]=66;
    }
    if (pixel.region==FR_BEAVER_TAIL && (x+y)%4==0) {
        pixel.color[0]=58;pixel.color[1]=36;pixel.color[2]=25;
    }
    if (pixel.part==BEAVER_HEAD && ((x==27 && y==17) || (x>=32 && y==18))) {
        pixel.color[0]=24;pixel.color[1]=18;pixel.color[2]=16;
    }
    if (pixel.region==FR_BEAVER_TEETH && x==31) {
        pixel.color[0]=109;pixel.color[1]=86;pixel.color[2]=62;
    }
    *out=pixel;
    return true;
}

void beaver_art_pose(const beaver_art_pixel_t *pixel,fr_player_action_t action,
    uint32_t frame,int32_t *x,int32_t *y) {
    if (action==FR_PLAYER_WALK) {
        static const int32_t stride[6]={-2,0,2,2,0,-2};
        static const int32_t lift[6]={0,-1,-1,0,1,1};
        static const int32_t arm[6]={0,1,2,0,-1,-2};
        if (pixel->part==BEAVER_LEFT_FOOT) {*x+=stride[frame];*y+=lift[frame]<0?-1:0;}
        else if (pixel->part==BEAVER_RIGHT_FOOT) {*x-=stride[frame];*y+=lift[frame]>0?-1:0;}
        else if (pixel->part==BEAVER_LEFT_ARM || pixel->part==BEAVER_RIGHT_ARM) *y+=arm[frame];
        else if (pixel->part==BEAVER_HEAD || pixel->part==BEAVER_HAT || pixel->part==BEAVER_JAW) *y+=lift[frame];
        else if (pixel->part==BEAVER_TAIL) *y+=arm[frame]/2;
    } else if (action==FR_PLAYER_CHEW) {
        static const int32_t jaw[8]={0,1,2,3,2,1,0,-1};
        static const int32_t reach[8]={0,1,2,3,4,3,2,1};
        if (pixel->part==BEAVER_JAW) *y+=jaw[frame];
        if (pixel->part==BEAVER_LEFT_ARM || pixel->part==BEAVER_RIGHT_ARM) {
            *x+=reach[frame];*y-=2;
        }
    } else if (action==FR_PLAYER_CAST) {
        static const int32_t rise[4]={0,-3,-6,-8};
        if (pixel->part==BEAVER_LEFT_ARM || pixel->part==BEAVER_RIGHT_ARM) {
            *x+=(int32_t)frame;*y+=rise[frame];
        }
    }
}
