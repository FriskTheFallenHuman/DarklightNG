#version 120

uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform sampler2D u_texture5;
uniform sampler2D u_texture6;
uniform sampler2D u_texture7;

varying vec2 megaST;
varying vec4 megaMaskX;
varying vec4 megaMaskY;
varying vec4 megaLevelOpacity;
varying vec4 megaDetailST;

vec2 decodeMegaNormalXY( float alpha ) {
	float packedValue = floor( alpha * 255.0 + 0.5 );
	float packedX = mod( packedValue, 16.0 );
	float packedY = floor( packedValue / 16.0 );
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
	// Terrain vertex colors are authoring layer weights. The compiler has
	// already resolved those weights and the optional atmosphere bake into the
	// streamed MegaTexture. They must not tint the final image.
	gl_FragColor = vec4( combinedColor, 1.0 );
}
