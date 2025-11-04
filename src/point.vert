layout(location = 0) in float pos;
void main(){
	float id = gl_VertexIndex / 32767. - 1;
	gl_PointSize = 1;
  gl_Position = vec4(id,pos,0,1);
}
