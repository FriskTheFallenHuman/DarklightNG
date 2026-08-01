#version 120

uniform vec4 u_vertexParm[32];

void main() {
	gl_TexCoord[0] = vec4( dot( gl_Vertex, u_vertexParm[0] ), dot( gl_Vertex, u_vertexParm[1] ), dot( gl_Vertex, u_vertexParm[2] ), dot( gl_Vertex, u_vertexParm[3] ) );
	gl_TexCoord[1] = vec4( dot( gl_Vertex, u_vertexParm[4] ), dot( gl_Vertex, u_vertexParm[5] ), 0.0, dot( gl_Vertex, u_vertexParm[6] ) );
	gl_TexCoord[2] = vec4( dot( gl_Vertex, u_vertexParm[7] ), 0.5, 0.0, 1.0 );
	gl_Position = ftransform();
}
