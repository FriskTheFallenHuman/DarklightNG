#version 120

attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;
uniform vec4 u_vertexParm[32];

void main() {
	vec3 localEye = u_vertexParm[5].xyz - gl_Vertex.xyz;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[1] = vec4( dot( localEye, u_vertexParm[6].xyz ), dot( localEye, u_vertexParm[7].xyz ), dot( localEye, u_vertexParm[8].xyz ), 1.0 );
	gl_TexCoord[2] = vec4( dot( attr_Tangent, u_vertexParm[6].xyz ), dot( attr_Bitangent, u_vertexParm[6].xyz ), dot( gl_Normal, u_vertexParm[6].xyz ), 0.0 );
	gl_TexCoord[3] = vec4( dot( attr_Tangent, u_vertexParm[7].xyz ), dot( attr_Bitangent, u_vertexParm[7].xyz ), dot( gl_Normal, u_vertexParm[7].xyz ), 0.0 );
	gl_TexCoord[4] = vec4( dot( attr_Tangent, u_vertexParm[8].xyz ), dot( attr_Bitangent, u_vertexParm[8].xyz ), dot( gl_Normal, u_vertexParm[8].xyz ), 0.0 );
	gl_FrontColor = gl_Color;
	gl_Position = ftransform();
}
