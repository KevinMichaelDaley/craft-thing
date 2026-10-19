#ifndef FR_RENDERER_TEXTURE_GLSL
#define FR_RENDERER_TEXTURE_GLSL

struct Appearance {vec4 color_roughness;vec4 normal_relief;};
uint texture_hash(uint x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
float noise_corner(ivec2 p,uint seed) {
    uint hash=texture_hash(uint(p.x)*0x9e3779b9u^uint(p.y)*0x85ebca6bu^seed);
    return float(hash&0xffffu)/65535.;
}
float smooth_noise(vec2 p,uint seed) {
    ivec2 corner=ivec2(floor(p));vec2 f=fract(p);f=f*f*(3.-2.*f);
    return mix(mix(noise_corner(corner,seed),noise_corner(corner+ivec2(1,0),seed),f.x),
        mix(noise_corner(corner+ivec2(0,1),seed),noise_corner(corner+ivec2(1,1),seed),f.x),f.y);
}
float surface_height(vec2 p,uint seed,uint material,uint style) {
    float phase=float(seed&1023u)*.006135923;
    if(material==7u || material==8u) {
        float warped=p.x+.7*sin(p.y*.035+phase)+.3*sin(p.y*.08);
        float grain=style==1u?length(p+vec2(14,8))*.62:warped*.75;
        return .5+.5*sin(grain+phase);
    }
    if(style==2u) {
        float row=floor(p.y/8.),bx=p.x+(mod(row,2.)==0.?0.:8.);
        float joint_x=bx-floor(bx/16.)*16.,joint_y=p.y-row*8.;
        if(joint_x<1. || joint_y<1.) return .08;
        return .55+.3*smooth_noise(p*.13,seed);
    }
    return .7*smooth_noise(p*.12,seed)+.3*smooth_noise(p*.47,seed+31u);
}
Appearance texture_sample(ivec2 rest,uint identity,uint material,uint style) {
    Appearance a=Appearance(vec4(.2,.2,.2,.85),vec4(0,0,1,0));
    if(material==0u) return a;
    if(material==9u || material==10u) a.color_roughness.rgb=vec3(.08,.3,.025);
    else if(material==7u || material==8u) a.color_roughness.rgb=vec3(.35,.16,.045);
    else if(material==4u) a.color_roughness.rgb=vec3(.28,.14,.05);
    else if(material==6u) a.color_roughness.rgb=vec3(.42,.39,.34);
    uint seed=texture_hash(identity);vec2 p=vec2(rest);
    float h=surface_height(p,seed,material,style),variation=.65+.7*h;
    if(style==2u && material!=7u && material!=8u)
        a.color_roughness.rgb=h<.1?vec3(.33,.31,.28):vec3(.42,.12,.055);
    a.color_roughness.rgb*=variation;
    float strength=material==1u?.55:.2;
    vec2 n=-strength*vec2(surface_height(p+vec2(1,0),seed,material,style)-surface_height(p-vec2(1,0),seed,material,style),
        surface_height(p+vec2(0,1),seed,material,style)-surface_height(p-vec2(0,1),seed,material,style));
    a.normal_relief=vec4(normalize(vec3(n,1)),h*strength);
    a.color_roughness.a=.7+.25*(1.-h);return a;
}

#endif
