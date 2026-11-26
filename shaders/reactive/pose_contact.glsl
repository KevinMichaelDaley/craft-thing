#include "../../include/dungeoncraft/sprite_layout.h"

layout(buffer_reference,std430,buffer_reference_align=4) readonly buffer ContactSprites {uint words[];};

uint fr_body_contact_pose(uvec2 address,uint slot,Body body) {
    if((address.x|address.y)==0u) return 0u;
    ContactSprites sprites=ContactSprites(address);
    if(sprites.words[0]!=DC_SPRITE_MAGIC) return 0u;
    uint generation=DC_BODY_OWNER(runtime_body_root,slot).generation;
    for(uint n=0u;n<min(sprites.words[1],DC_SPRITE_MAX);n++) {
        uint at=DC_SPRITE_HEADER_WORDS+n*DC_SPRITE_POSE_WORDS;
        if(sprites.words[at]==body.id&&sprites.words[at+1u]==generation) return at;
    }
    return 0u;
}

/* Move only the contact quad. The canonical piece still owns the material. */
bool fr_body_project_contact(uvec2 address,uint pose,uint piece,inout Body body) {
    if(pose==0u) return true;
    ContactSprites sprites=ContactSprites(address);
    ivec2 rest=pixel_piece_cell(body,piece);
    ivec2 source=rest+ivec2(sprites.words[pose+6u],sprites.words[pose+7u]);
    if(any(lessThan(source,ivec2(0)))||
       any(greaterThanEqual(source,ivec2(DC_SPRITE_SOURCE_SIDE)))) return false;
    uint map_at=sprites.words[pose+DC_SPRITE_CONTACT_WORD];
    uint texel=sprites.words[map_at+uint(source.y)*DC_SPRITE_SOURCE_SIDE+uint(source.x)];
    if(texel==0u) return false;
    uint width=sprites.words[pose+3u];
    uvec2 cell=uvec2((texel-1u)%width,(texel-1u)/width);
    if(sprites.words[pose+5u]!=0u) cell.x=width-1u-cell.x;
    vec2 local=vec2(cell)-vec2(ivec2(sprites.words[pose+8u],sprites.words[pose+9u]));
    vec2 origin=vec2(body.x_fp,body.y_fp)+round(body_rotate(local-vec2(rest),body.angle)*65536.);
    if(any(greaterThanEqual(abs(origin),vec2(2147483520.)))) return false;
    body.x_fp=int(origin.x);
    body.y_fp=int(origin.y);
    return true;
}
