#version 120

void main() {
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
	gl_TexCoord[1] = gl_TextureMatrix[1] * gl_MultiTexCoord1;
	// Preserve the fixed-function stage color.  The z prepass intentionally
	// submits black; forcing white here left every GPU-skinned silhouette white
	// before its textured interaction stages were accumulated.
	gl_FrontColor = gl_Color;
	gl_Position = ftransform();
}
