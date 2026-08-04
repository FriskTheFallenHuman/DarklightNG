#version 120

uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform vec4 u_vertexParm[32];
uniform vec4 u_vertexLocalParm[16];
uniform vec4 u_fragmentParm[32];

const vec2 ATLAS_SIZE = vec2( 1320.0, 528.0 );
const vec2 CELL_SIZE = vec2( 264.0, 264.0 );
const vec2 FRAME_SIZE = vec2( 256.0, 256.0 );
const vec2 GUTTER = vec2( 4.0, 4.0 );

vec2 sampleWaterNormal( float frame, vec2 texCoord ) {
	float frameIndex = mod( frame, 10.0 );
	vec2 tile = vec2( mod( frameIndex, 5.0 ), floor( frameIndex / 5.0 ) );
	vec2 pixel = tile * CELL_SIZE + GUTTER + fract( texCoord ) * ( FRAME_SIZE - 1.0 ) + 0.5;
	return texture2D( u_texture1, pixel / ATLAS_SIZE ).rg * 2.0 - 1.0;
}

void main() {
	vec2 animatedTexCoord = gl_TexCoord[0].xy + u_vertexLocalParm[0].xy;
	float frame = mod( u_vertexLocalParm[0].z, 10.0 );
	float firstFrame = floor( frame );
	float nextFrame = mod( firstFrame + 1.0, 10.0 );
	vec2 normalXY = mix(
		sampleWaterNormal( firstFrame, animatedTexCoord ),
		sampleWaterNormal( nextFrame, animatedTexCoord ), fract( frame ) );
	vec2 screenTexCoord = gl_FragCoord.xy * u_fragmentParm[1].xy;
	screenTexCoord += normalXY * gl_TexCoord[6].x * u_vertexLocalParm[1].x;
	screenTexCoord = clamp( screenTexCoord, 0.0, 1.0 ) * u_fragmentParm[0].xy;
	vec3 sceneColor = texture2D( u_texture2, screenTexCoord ).rgb;
	float waterDistance = length( gl_TexCoord[5].xyz - u_vertexParm[1].xyz );
	float fog = 1.0 - exp( -waterDistance * u_vertexLocalParm[2].w );
	gl_FragColor = vec4( mix( sceneColor, u_vertexLocalParm[2].rgb, clamp( fog, 0.15, 0.92 ) ), 1.0 );
}
