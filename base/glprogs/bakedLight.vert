#version 120

attribute vec4 attr_TexCoord;
attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;
attribute vec3 attr_Normal;
attribute vec2 attr_LightCoord;
uniform vec4 u_vertexParm[32];

void main() {
	gl_TexCoord[0] = vec4( dot( attr_TexCoord, u_vertexParm[0] ), dot( attr_TexCoord, u_vertexParm[1] ), 0.0, 1.0 );
	gl_TexCoord[1] = vec4( dot( attr_TexCoord, u_vertexParm[2] ), dot( attr_TexCoord, u_vertexParm[3] ), 0.0, 1.0 );
	gl_TexCoord[2] = vec4( attr_LightCoord, 0.0, 1.0 );
	gl_TexCoord[3] = vec4( attr_LightCoord, 0.0, 1.0 );
	vec3 localView = u_vertexParm[6].xyz - gl_Vertex.xyz;
	gl_TexCoord[4] = vec4( dot( attr_Tangent, localView ), dot( attr_Bitangent, localView ), dot( attr_Normal, localView ), 1.0 );
	gl_FrontColor = gl_Color * u_vertexParm[4] + u_vertexParm[5];
	gl_Position = ftransform();
}
