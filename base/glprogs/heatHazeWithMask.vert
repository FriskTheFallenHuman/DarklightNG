#version 120

uniform vec4 u_vertexLocalParm[16];

void main() {
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[1] = gl_MultiTexCoord0 + u_vertexLocalParm[0];
	vec4 projected = gl_ProjectionMatrix * vec4( 1.0, 0.0, ( gl_ModelViewMatrix * gl_Vertex ).z, 1.0 );
	float deformScale = min( projected.x / max( projected.w, 1.0 ), 0.02 );
	gl_TexCoord[2] = vec4( deformScale ) * u_vertexLocalParm[1];
	gl_Position = ftransform();
}
