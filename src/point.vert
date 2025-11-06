#include <def.glsl>
#include <const.h>
layout(location = 0) in float pos;
void main() {
  float id = gl_VertexIndex * cmn.scale - 1;
  float y = pos * -I2_3 - I_3;
  vec2 p = vec2(id, y);
  vec2 ofst = vec2(-cmn.trig, 1) * gl_InstanceIndex;
  gl_PointSize = 1;
  gl_Position = vec4(p + ofst, 0, 1);
}
