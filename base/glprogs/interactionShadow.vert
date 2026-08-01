#version 120

attribute vec4 attr_TexCoord;
attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;
attribute vec3 attr_Normal;
uniform vec4 u_vertexParm[32];

void main() {
	vec3 toLight = u_vertexParm[4].xyz - gl_Vertex.xyz;
	gl_TexCoord[0] = vec4( dot( attr_Tangent, toLight ), dot( attr_Bitangent, toLight ), dot( attr_Normal, toLight ), dot( gl_Vertex, u_vertexParm[21] ) );
	gl_TexCoord[1] = vec4( dot( attr_TexCoord, u_vertexParm[10] ), dot( attr_TexCoord, u_vertexParm[11] ), 0.0, 1.0 );
	gl_TexCoord[2] = vec4( dot( gl_Vertex, u_vertexParm[9] ), 0.5, dot( gl_Vertex, u_vertexParm[18] ), dot( gl_Vertex, u_vertexParm[19] ) );
	gl_TexCoord[3] = vec4( dot( gl_Vertex, u_vertexParm[6] ), dot( gl_Vertex, u_vertexParm[7] ), dot( gl_Vertex, u_vertexParm[20] ), dot( gl_Vertex, u_vertexParm[8] ) );
	gl_TexCoord[4] = vec4( dot( attr_TexCoord, u_vertexParm[12] ), dot( attr_TexCoord, u_vertexParm[13] ), 0.0, 1.0 );
	gl_TexCoord[5] = vec4( dot( attr_TexCoord, u_vertexParm[14] ), dot( attr_TexCoord, u_vertexParm[15] ), 0.0, 1.0 );
	vec3 halfAngle = normalize( toLight ) + normalize( u_vertexParm[5].xyz - gl_Vertex.xyz );
	gl_TexCoord[6] = vec4( dot( attr_Tangent, halfAngle ), dot( attr_Bitangent, halfAngle ), dot( attr_Normal, halfAngle ), 1.0 );
	gl_FrontColor = gl_Color * u_vertexParm[16] + u_vertexParm[17];
	gl_Position = ftransform();
}
