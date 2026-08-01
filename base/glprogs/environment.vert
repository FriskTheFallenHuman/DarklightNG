#version 120

uniform vec4 u_vertexParm[32];

void main() {
	gl_TexCoord[0] = vec4( gl_Normal, 1.0 );
	gl_TexCoord[1] = vec4( u_vertexParm[5].xyz - gl_Vertex.xyz, 1.0 );
	gl_FrontColor = gl_Color;
	gl_Position = ftransform();
}
