#include <def.glsl>

layout(location = 0) in vec4 attr;
layout(location = 0) out vec2 cord;
vec2 tex[] = {{0,0},{0,16},{8,0},{8,16}};
void main(){
  vec2 p0 = vec2(0,attr.z);
  cord = p0 + tex[gl_VertexIndex];
  gl_Position = vec4(tex[gl_VertexIndex] * cmn.scl + attr.xy,0,1);
}

