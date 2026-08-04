#version 120

attribute vec4 attr_TexCoord;
attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;
attribute vec3 attr_Normal;
uniform vec4 u_vertexParm[32];

vec3 localToWorld( vec3 direction ) {
	return vec3( dot( direction, u_vertexParm[7].xyz ),
		dot( direction, u_vertexParm[8].xyz ),
		dot( direction, u_vertexParm[9].xyz ) );
}

void main() {
	vec4 position = gl_Vertex;
	gl_TexCoord[0] = vec4( dot( attr_TexCoord, u_vertexParm[0] ), dot( attr_TexCoord, u_vertexParm[1] ), 0.0, 1.0 );
	gl_TexCoord[1] = vec4( dot( attr_TexCoord, u_vertexParm[2] ), dot( attr_TexCoord, u_vertexParm[3] ), 0.0, 1.0 );
	gl_TexCoord[2] = vec4( localToWorld( attr_Tangent ), 0.0 );
	gl_TexCoord[3] = vec4( localToWorld( attr_Bitangent ), 0.0 );
	gl_TexCoord[4] = vec4( localToWorld( attr_Normal ), 0.0 );
	gl_TexCoord[5] = vec4( localToWorld( u_vertexParm[6].xyz - position.xyz ), 0.0 );
	gl_FrontColor = gl_Color * u_vertexParm[4] + u_vertexParm[5];
	gl_Position = ftransform();
}
