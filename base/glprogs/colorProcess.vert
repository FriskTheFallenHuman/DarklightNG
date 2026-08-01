#version 120

uniform vec4 u_vertexLocalParm[16];

void main() {
	gl_TexCoord[0] = vec4( 1.0 ) - u_vertexLocalParm[0];
	gl_TexCoord[1] = u_vertexLocalParm[1] * u_vertexLocalParm[0];
	gl_Position = ftransform();
}
