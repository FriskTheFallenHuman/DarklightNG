#version 120

uniform vec4 u_vertexParm[32];

void main() {
	vec4 localPosition = gl_Vertex;
	vec3 worldPosition;
	worldPosition.x = dot( localPosition, u_vertexParm[0] );
	worldPosition.y = dot( localPosition, u_vertexParm[1] );
	worldPosition.z = dot( localPosition, u_vertexParm[2] );
	gl_TexCoord[0] = vec4( worldPosition, 1.0 );
	gl_Position = ftransform();
}
