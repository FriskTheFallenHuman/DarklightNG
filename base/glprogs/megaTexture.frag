#version 120

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform sampler2D u_texture5;
uniform sampler2D u_texture6;
uniform sampler2D u_texture7;
uniform vec4 u_fragmentParm[32];

varying vec2 megaST;
varying vec4 megaMaskX;
varying vec4 megaMaskY;
varying vec4 megaLevelOpacity;
varying vec4 megaDetailST;
varying vec2 megaLightST;

vec2 decodeMegaNormalXY( float alpha ) {
	float packed = floor( alpha * 255.0 + 0.5 );
	float packedX = mod( packed, 16.0 );
	float packedY = floor( packed / 16.0 );
	return vec2( ( packedX - 8.0 ) / ( packedX < 8.0 ? 8.0 : 7.0 ),
		( packedY - 8.0 ) / ( packedY < 8.0 ? 8.0 : 7.0 ) );
}

void main() {
	vec4 levelMask = clamp( 16.0 - 32.0 * max( abs( megaMaskX ), abs( megaMaskY ) ), 0.0, 1.0 );
	levelMask *= megaLevelOpacity;
	vec4 level1 = texture2D( u_texture1, megaST );
	vec4 level2 = texture2D( u_texture2, megaST * 2.0 );
	vec4 level3 = texture2D( u_texture3, megaST * 4.0 );
	vec4 level4 = texture2D( u_texture4, megaST * 8.0 );
	vec4 level5 = texture2D( u_texture5, megaST * 16.0 );
	vec3 combinedColor = level1.rgb;
	combinedColor = mix( combinedColor, level2.rgb, levelMask.x );
	combinedColor = mix( combinedColor, level3.rgb, levelMask.y );
	combinedColor = mix( combinedColor, level4.rgb, levelMask.z );
	combinedColor = mix( combinedColor, level5.rgb, levelMask.w );
	vec2 normalXY = decodeMegaNormalXY( level1.a );
	normalXY = mix( normalXY, decodeMegaNormalXY( level2.a ), levelMask.x );
	normalXY = mix( normalXY, decodeMegaNormalXY( level3.a ), levelMask.y );
	normalXY = mix( normalXY, decodeMegaNormalXY( level4.a ), levelMask.z );
	normalXY = mix( normalXY, decodeMegaNormalXY( level5.a ), levelMask.w );
	vec3 normal = normalize( vec3( normalXY, sqrt( max( 0.0, 1.0 - dot( normalXY, normalXY ) ) ) ) );
	vec3 bakedIrradiance = texture2D( u_texture0, megaLightST ).rgb * u_fragmentParm[8].x;
	vec3 direction = normalize( texture2D( u_texture6, megaLightST ).xyz * 2.0 - 1.0 );
	float directionFactor = clamp( max( dot( normal, direction ), 0.0 ) / max( direction.z, 0.25 ), 0.0, 2.0 );
	combinedColor *= mix( vec3( 1.0 ), bakedIrradiance * directionFactor, u_fragmentParm[8].w );
	// Terrain vertex colors are authoring layer weights. The compiler has
	// already resolved those weights into the streamed MegaTexture, and dmap's
	// proc surfaces do not serialize them. They must not tint the final image.
	gl_FragColor = vec4( combinedColor, 1.0 );
}
