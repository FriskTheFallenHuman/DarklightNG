#version 120

uniform samplerCube u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform vec4 u_vertexParm[32];
uniform vec4 u_vertexLocalParm[16];
uniform vec4 u_fragmentParm[32];

const float PI = 3.14159265358979323846;
const vec2 ATLAS_SIZE = vec2( 1320.0, 528.0 );
const vec2 CELL_SIZE = vec2( 264.0, 264.0 );
const vec2 FRAME_SIZE = vec2( 256.0, 256.0 );
const vec2 GUTTER = vec2( 4.0, 4.0 );
// RB_EnterWeaponDepthHack writes first-person surfaces only into [0, 0.5].
// Excluding that interval makes weapon/fist pixels ineligible SSR samples.
const float VIEW_MODEL_DEPTH_MAX = 0.5005;

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

vec2 projectViewPosition( vec3 viewPosition ) {
	float inverseW = -1.0 / viewPosition.z;
	vec2 ndc = vec2(
		( viewPosition.x * u_fragmentParm[5].x + viewPosition.z * u_fragmentParm[5].z ) * inverseW,
		( viewPosition.y * u_fragmentParm[5].y + viewPosition.z * u_fragmentParm[5].w ) * inverseW );
	return ndc * 0.5 + 0.5;
}

float depthToViewZ( float depth ) {
	float ndcDepth = depth * 2.0 - 1.0;
	return -u_fragmentParm[6].y / ( ndcDepth + u_fragmentParm[6].x );
}

bool isViewModelDepth( float depth ) {
	return depth <= VIEW_MODEL_DEPTH_MAX;
}

bool touchesViewModel( vec2 screenUV ) {
	vec2 depthCoord = screenUV * u_fragmentParm[0].xy;
	vec2 texel = u_fragmentParm[1].xy * u_fragmentParm[0].xy;
	return isViewModelDepth( texture2D( u_texture4, depthCoord ).r ) ||
		isViewModelDepth( texture2D( u_texture4, depthCoord + vec2( texel.x, 0.0 ) ).r ) ||
		isViewModelDepth( texture2D( u_texture4, depthCoord - vec2( texel.x, 0.0 ) ).r ) ||
		isViewModelDepth( texture2D( u_texture4, depthCoord + vec2( 0.0, texel.y ) ).r ) ||
		isViewModelDepth( texture2D( u_texture4, depthCoord - vec2( 0.0, texel.y ) ).r );
}

vec4 traceScreenSpaceReflection( vec3 viewPosition, vec3 viewNormal ) {
	vec3 viewDirection = normalize( viewPosition );
	if ( dot( viewNormal, -viewDirection ) < 0.0 ) {
		viewNormal = -viewNormal;
	}

	vec3 reflectionDirection = normalize( reflect( viewDirection, viewNormal ) );
	vec3 rayOrigin = viewPosition + viewNormal * u_vertexLocalParm[7].z;
	float rayLength = u_vertexLocalParm[7].y;
	if ( reflectionDirection.z > 0.0 ) {
		// Stop at the camera near plane rather than letting projection wrap a
		// ray that has passed behind the viewer.
		rayLength = min( rayLength, ( -1.0 - rayOrigin.z ) / reflectionDirection.z );
	}
	if ( rayLength <= 0.0 ) {
		return vec4( 0.0 );
	}

	vec3 rayEnd = rayOrigin + reflectionDirection * rayLength;
	vec2 viewportSize = 1.0 / u_fragmentParm[1].xy;
	vec2 startPixel = projectViewPosition( rayOrigin ) * viewportSize;
	vec2 endPixel = projectViewPosition( rayEnd ) * viewportSize;
	vec2 pixelDelta = endPixel - startPixel;
	if ( dot( pixelDelta, pixelDelta ) < 0.25 ) {
		return vec4( 0.0 );
	}

	// Traverse the dominant screen axis.  Interpolating Q = viewPosition / W
	// and K = 1 / W makes the reconstructed ray depth perspective-correct;
	// fixed view-space distance steps caused reflection hits to slide as the
	// camera moved.
	bool permute = abs( pixelDelta.x ) < abs( pixelDelta.y );
	if ( permute ) {
		startPixel = startPixel.yx;
		endPixel = endPixel.yx;
		pixelDelta = pixelDelta.yx;
	}

	float stepDirection = pixelDelta.x < 0.0 ? -1.0 : 1.0;
	float inverseDeltaX = stepDirection / pixelDelta.x;
	vec2 pixelStep = vec2( stepDirection, pixelDelta.y * inverseDeltaX );
	float startK = -1.0 / rayOrigin.z;
	float endK = -1.0 / rayEnd.z;
	vec3 startQ = rayOrigin * startK;
	vec3 endQ = rayEnd * endK;
	vec3 qStep = ( endQ - startQ ) * inverseDeltaX;
	float kStep = ( endK - startK ) * inverseDeltaX;

	float pixelLength = abs( pixelDelta.x );
	float stride = clamp( pixelLength / 64.0, 1.0, 2.0 );
	pixelStep *= stride;
	qStep *= stride;
	kStep *= stride;

	vec2 pixel = startPixel;
	vec3 q = startQ;
	float k = startK;
	float previousRayZ = rayOrigin.z;
	float endDirection = endPixel.x * stepDirection;

	for ( int i = 0; i < 64; ++i ) {
		pixel += pixelStep;
		q += qStep;
		k += kStep;
		if ( pixel.x * stepDirection > endDirection ) {
			break;
		}

		vec2 hitPixel = permute ? pixel.yx : pixel;
		vec2 screenUV = ( floor( hitPixel ) + vec2( 0.5 ) ) * u_fragmentParm[1].xy;
		if ( screenUV.x <= 0.0 || screenUV.x >= 1.0 || screenUV.y <= 0.0 || screenUV.y >= 1.0 ) {
			break;
		}

		float currentRayZ = q.z / k;
		float rayZFront = max( previousRayZ, currentRayZ );
		float rayZBack = min( previousRayZ, currentRayZ );
		previousRayZ = currentRayZ;

		float sceneDepthSample = texture2D( u_texture4, screenUV * u_fragmentParm[0].xy ).r;
		// View-model depth is deliberately treated like empty screen.  It can
		// never become a hit or provide color to the reflection.
		if ( isViewModelDepth( sceneDepthSample ) || sceneDepthSample >= 0.9999 ) {
			continue;
		}

		float sceneViewZ = depthToViewZ( sceneDepthSample );
		float thickness = max( u_vertexLocalParm[7].x, -sceneViewZ * 0.015 );
		if ( rayZFront >= sceneViewZ - thickness && rayZBack <= sceneViewZ ) {
			if ( touchesViewModel( screenUV ) ) {
				continue;
			}
			float edgeDistance = min( min( screenUV.x, 1.0 - screenUV.x ), min( screenUV.y, 1.0 - screenUV.y ) );
			float edgeFade = smoothstep( 0.0, 0.08, edgeDistance );
			float depthFade = 1.0 - clamp( ( sceneViewZ - rayZFront ) / thickness, 0.0, 1.0 );
			vec3 hitColor = texture2D( u_texture2, screenUV * u_fragmentParm[0].xy ).rgb;
			return vec4( hitColor, edgeFade * depthFade );
		}
	}

	return vec4( 0.0 );
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
	vec3 viewNormal = normalize( vec3(
		dot( worldNormal, u_fragmentParm[2].xyz ),
		dot( worldNormal, u_fragmentParm[3].xyz ),
		dot( worldNormal, u_fragmentParm[4].xyz ) ) );

	vec2 screenTexCoord = ( gl_FragCoord.xy - u_fragmentParm[7].xy ) * u_fragmentParm[1].xy;
	screenTexCoord += worldNormal.xy * gl_TexCoord[6].x * u_vertexLocalParm[1].x;
	screenTexCoord = clamp( screenTexCoord, 0.0, 1.0 ) * u_fragmentParm[0].xy;
	vec3 refractedScene = texture2D( u_texture2, screenTexCoord ).rgb;
	float refractedLuma = dot( refractedScene, vec3( 0.2126, 0.7152, 0.0722 ) );
	// Darklight does not yet have all of Valley's original riverbed shading.
	// Suppress the raw yellow/olive chroma before applying water absorption;
	// cubemap and screen-space reflection deliberately bypass this filter.
	refractedScene = mix( refractedScene, vec3( refractedLuma ), 0.35 );
	vec3 refraction = refractedScene * u_vertexLocalParm[2].rgb;
	vec3 reflection = textureCube( u_texture0, reflectionVector ).rgb;
	reflection = mix( reflection, vec3( 0.5 ), u_vertexLocalParm[1].w );
	vec4 screenReflection = traceScreenSpaceReflection( gl_TexCoord[7].xyz, viewNormal );
	reflection = mix( reflection, screenReflection.rgb,
		clamp( screenReflection.a * u_vertexLocalParm[7].w, 0.0, 1.0 ) );

	float facing = max( dot( surfaceToEye, worldNormal ), 0.0 );
	float fresnel = clamp( 1.0 / pow( 1.0 + facing, u_vertexLocalParm[1].y ), 0.0, 1.0 );
	// Keep enough reflected scene at head-on angles that reflection does not
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
