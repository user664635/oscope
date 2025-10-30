#include <def.glsl>
layout(location = 0) out vec4 col;
void main(){
	uint id = gl_VertexIndex;
	col = cir[id].col;
	gl_PointSize = 5;
  gl_Position = vec4(cir[id].pv.xy * cmn.scl * 2160 - 1,0,1);
}
