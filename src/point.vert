layout(location = 0) in float pos;
void main(){
	float id = gl_VertexIndex / 524288. / 3. - 1;
	gl_PointSize = 1.5;
  gl_Position = vec4(id,(1 + pos * 2) / -3.,0,1);
}
