#version 120

void main() {
	vec4 position = gl_Vertex;
	vec4 projection = vec4(
		dot( position, gl_ObjectPlaneS[0] ),
		dot( position, gl_ObjectPlaneT[0] ),
		0.0,
		dot( position, gl_ObjectPlaneQ[0] ) );
	vec4 falloff = vec4(
		dot( position, gl_ObjectPlaneS[1] ),
		0.5,
		0.0,
		1.0 );

	gl_TexCoord[0] = gl_TextureMatrix[0] * projection;
	gl_TexCoord[1] = gl_TextureMatrix[1] * falloff;
	gl_FrontColor = gl_Color;
	gl_Position = ftransform();
}
