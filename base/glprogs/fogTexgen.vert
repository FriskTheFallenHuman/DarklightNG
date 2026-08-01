#version 120

void main() {
	vec4 position = gl_Vertex;
	vec4 fogDistance = vec4(
		dot( position, gl_ObjectPlaneS[0] ),
		dot( position, gl_ObjectPlaneT[0] ),
		0.0,
		1.0 );
	vec4 fogEnter = vec4(
		dot( position, gl_ObjectPlaneS[1] ),
		dot( position, gl_ObjectPlaneT[1] ),
		0.0,
		1.0 );

	gl_TexCoord[0] = gl_TextureMatrix[0] * fogDistance;
	gl_TexCoord[1] = gl_TextureMatrix[1] * fogEnter;
	gl_FrontColor = gl_Color;
	gl_Position = ftransform();
}
