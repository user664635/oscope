layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 col;
void main(){
	col = color;
	vec2 p = gl_VertexIndex == 0 ? pos.xy : pos.zw;
  gl_Position = vec4(p,0,1);
}
