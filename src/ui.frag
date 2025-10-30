#include <def.glsl>

layout(location = 0) in vec2 cord;
layout(location = 0) out vec4 col;
void main(){
  col = vec4(texelFetch(font, ivec2(cord), 0).r);
}

