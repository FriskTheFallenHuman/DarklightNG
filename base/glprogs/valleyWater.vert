#version 120

attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;

uniform vec4 u_vertexParm[32];

void main() {
	vec4 localPosition = gl_Vertex;
	vec3 localEye = u_vertexParm[5].xyz - localPosition.xyz;

	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[1] = vec4(
		dot( localEye, u_vertexParm[6].xyz ),
		dot( localEye, u_vertexParm[7].xyz ),
		dot( localEye, u_vertexParm[8].xyz ), 1.0 );
	gl_TexCoord[2] = vec4( dot( attr_Tangent, u_vertexParm[6].xyz ), dot( attr_Bitangent, u_vertexParm[6].xyz ), dot( gl_Normal, u_vertexParm[6].xyz ), 0.0 );
	gl_TexCoord[3] = vec4( dot( attr_Tangent, u_vertexParm[7].xyz ), dot( attr_Bitangent, u_vertexParm[7].xyz ), dot( gl_Normal, u_vertexParm[7].xyz ), 0.0 );
	gl_TexCoord[4] = vec4( dot( attr_Tangent, u_vertexParm[8].xyz ), dot( attr_Bitangent, u_vertexParm[8].xyz ), dot( gl_Normal, u_vertexParm[8].xyz ), 0.0 );
	gl_TexCoord[5] = vec4(
		dot( localPosition, u_vertexParm[6] ),
		dot( localPosition, u_vertexParm[7] ),
		dot( localPosition, u_vertexParm[8] ), 1.0 );
	vec4 projected = gl_ProjectionMatrix * vec4( 1.0, 0.0, ( gl_ModelViewMatrix * localPosition ).z, 1.0 );
	gl_TexCoord[6] = vec4( min( abs( projected.x / max( abs( projected.w ), 1.0 ) ), 0.02 ) );
	gl_TexCoord[7] = gl_ModelViewMatrix * localPosition;
	gl_Position = ftransform();
}
