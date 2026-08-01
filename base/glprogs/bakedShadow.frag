#version 120

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform vec4 u_fragmentParm[8];

void main() {
	vec4 shadowCoord = gl_TexCoord[0];
	vec2 faceCoord = shadowCoord.xy / shadowCoord.w;
	if ( faceCoord.x < 0.0 || faceCoord.y < 0.0 || faceCoord.x > 1.0 || faceCoord.y > 1.0 || shadowCoord.w < 0.0 ) {
		discard;
	}
	float shadowDepth = texture2DProj( u_texture0, shadowCoord ).r;
	float receiverDepth = shadowCoord.z / shadowCoord.w;
	float lit = shadowDepth >= receiverDepth ? 1.0 : 0.0;
	vec4 projected = texture2DProj( u_texture1, gl_TexCoord[1] );
	projected *= texture2D( u_texture2, gl_TexCoord[2].xy );
	projected *= u_fragmentParm[0] * ( 1.0 - lit );
	gl_FragColor = clamp( vec4( 1.0 ) - projected, 0.0, 1.0 );
}
