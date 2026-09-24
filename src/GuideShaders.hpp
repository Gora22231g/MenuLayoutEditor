#pragma once
namespace layout::shaders {
inline constexpr char const *vertex = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;
varying vec2 v_texCoord;
varying vec2 v_position;
void main() {
    gl_Position=CC_MVPMatrix*a_position;
    v_texCoord=a_texCoord;
    v_position=a_position.xy;
}
)";
inline constexpr char const *fragment = R"(
uniform sampler2D CC_Texture0;
uniform vec2 u_pixel;
uniform vec4 u_hole;
varying vec2 v_texCoord;
varying vec2 v_position;
void main() {
    if(v_position.x>=u_hole.x&&v_position.y>=u_hole.y&&
       v_position.x<=u_hole.z&&v_position.y<=u_hole.w)discard;
    vec4 sum=vec4(0.0);
    for(int x=-2;x<=2;x++) {
        for(int y=-2;y<=2;y++) {
            float wx=x==0?6.0:(x==1||x==-1?4.0:1.0);
            float wy=y==0?6.0:(y==1||y==-1?4.0:1.0);
            sum+=texture2D(CC_Texture0,v_texCoord+vec2(float(x),float(y))*u_pixel)*wx*wy;
        }
    }
    gl_FragColor=vec4(sum.rgb*(0.48/256.0),1.0);
}
)";
}
