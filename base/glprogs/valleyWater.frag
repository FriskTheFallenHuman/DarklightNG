#version 120

uniform samplerCube u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform vec4 u_vertexParm[32];
uniform vec4 u_vertexLocalParm[16];
uniform vec4 u_fragmentParm[32];

const float PI = 3.14159265358979323846;
const vec2 ATLAS_SIZE = vec2( 1320.0, 528.0 );
const vec2 CELL_SIZE = vec2( 264.0, 264.0 );
const vec2 FRAME_SIZE = vec2( 256.0, 256.0 );
const vec2 GUTTER = vec2( 4.0, 4.0 );

vec3 sampleWaterNormal( float frame, vec2 texCoord ) {
	float frameIndex = mod( frame, 10.0 );
	vec2 tile = vec2( mod( frameIndex, 5.0 ), floor( frameIndex / 5.0 ) );
	vec2 pixel = tile * CELL_SIZE + GUTTER + fract( texCoord ) * ( FRAME_SIZE - 1.0 ) + 0.5;
	return texture2D( u_texture1, pixel / ATLAS_SIZE ).rgb;
}

float atmosphereExtinction( vec3 position, vec3 eye ) {
	if ( abs( eye.z - position.z ) < 5.0 ) {
		eye.z += 5.0;
	}
	vec3 ray = position - eye;
	float rayLength = max( length( ray ), 0.0001 );
	float cosTheta = ray.z / rayLength;
	float z1 = max( position.z + u_vertexLocalParm[5].z, 0.0 );
	float z2 = max( eye.z + u_vertexLocalParm[5].z, 0.0 );
	float density = pow( 0.5, z2 * u_vertexLocalParm[5].y ) - pow( 0.5, z1 * u_vertexLocalParm[5].y );
	float thickness = ( density / cosTheta ) * rayLength * u_vertexLocalParm[5].x * u_vertexLocalParm[5].w;
	return 1.0 - clamp( pow( 0.5, max( thickness, 0.0 ) ), 0.0, 1.0 );
}

void main() {
	vec2 animatedTexCoord = gl_TexCoord[0].xy + u_vertexLocalParm[0].xy;
	float frame = mod( u_vertexLocalParm[0].z, 10.0 );
	float firstFrame = floor( frame );
	float nextFrame = mod( firstFrame + 1.0, 10.0 );
	vec3 firstNormal = sampleWaterNormal( firstFrame, animatedTexCoord );
	vec3 nextNormal = sampleWaterNormal( nextFrame, animatedTexCoord );
	vec2 normalXY = mix( firstNormal.rg, nextNormal.rg, fract( frame ) ) * 2.0 - 1.0;
	normalXY *= u_vertexLocalParm[1].z;
	// Match water/simple_cube_interpolate: reverse the close-range XY normal
	// and fade it to flat over 10,000 world units.  Omitting this step made
	// steep normals fold the reflection below the horizon into valley_nz.
	float normalDistanceFade = clamp( length( gl_TexCoord[1].xyz ) * 0.0001, 0.0, 1.0 );
	normalXY *= normalDistanceFade - 1.0;
	vec3 localNormal = normalize( vec3( normalXY, sqrt( max( 1.0 - dot( normalXY, normalXY ), 0.001 ) ) ) );
	vec3 worldNormal = normalize( vec3(
		dot( localNormal, gl_TexCoord[2].xyz ),
		dot( localNormal, gl_TexCoord[3].xyz ),
		dot( localNormal, gl_TexCoord[4].xyz ) ) );
	vec3 surfaceToEye = normalize( gl_TexCoord[1].xyz );
	vec3 reflectionVector = 2.0 * dot( surfaceToEye, worldNormal ) * worldNormal - surfaceToEye;
	// This is the above-water surface: its off-screen fallback must represent
	// sky, never the Valley capture's olive ground face.  Steep animated
	// normals can otherwise push the cube lookup below the horizon even though
	// the actual reflected scene ray has no valid cubemap ground reflection.
	reflectionVector.z = abs( reflectionVector.z );

	vec2 screenTexCoord = gl_FragCoord.xy * u_fragmentParm[1].xy;
	screenTexCoord += worldNormal.xy * gl_TexCoord[6].x * u_vertexLocalParm[1].x;
	screenTexCoord = clamp( screenTexCoord, 0.0, 1.0 ) * u_fragmentParm[0].xy;
	vec3 refractedScene = texture2D( u_texture2, screenTexCoord ).rgb;
	float refractedLuma = dot( refractedScene, vec3( 0.2126, 0.7152, 0.0722 ) );
	// Darklight does not yet have all of Valley's original riverbed shading.
	// Suppress the raw yellow/olive chroma before applying water absorption;
	// cubemap reflection deliberately bypasses this filter.
	refractedScene = mix( refractedScene, vec3( refractedLuma ), 0.35 );
	vec3 refraction = refractedScene * u_vertexLocalParm[2].rgb;
	vec3 reflection = textureCube( u_texture0, reflectionVector ).rgb;
	reflection = mix( reflection, vec3( 0.5 ), u_vertexLocalParm[1].w );

	float facing = max( dot( surfaceToEye, worldNormal ), 0.0 );
	float fresnel = clamp( 1.0 / pow( 1.0 + facing, u_vertexLocalParm[1].y ), 0.0, 1.0 );
	// Keep enough cubemap response at head-on angles that reflection does not
	// disappear into the refraction branch.
	fresnel = 0.32 + fresnel * 0.68;
	vec3 waterColor = mix( refraction, reflection, fresnel );
	// ETQW's high-quality simple_cube_interpolate path does not add the
	// low-quality sun-glare term.  On a large nearly planar surface that term
	// becomes a solid warm sheet at the alignment angle and hides reflection.

	vec3 worldPosition = gl_TexCoord[5].xyz;
	vec3 eye = u_vertexParm[1].xyz;
	vec3 rayDirection = normalize( worldPosition - eye );
	vec2 skyUV = vec2(
		atan( rayDirection.y, rayDirection.x ) / ( 2.0 * PI ) + 0.5,
		asin( clamp( rayDirection.z, -1.0, 1.0 ) ) / PI + 0.5 );
	vec3 atmosphereColor = mix( u_vertexLocalParm[6].rgb, texture2D( u_texture3, skyUV ).rgb, 0.75 );
	waterColor = mix( waterColor, atmosphereColor, atmosphereExtinction( worldPosition, eye ) );
	gl_FragColor = vec4( waterColor, 1.0 );
}
