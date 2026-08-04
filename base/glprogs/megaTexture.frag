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

void main() {
	vec4 levelMask = clamp( 16.0 - 32.0 * max( abs( megaMaskX ), abs( megaMaskY ) ), 0.0, 1.0 );
	levelMask *= megaLevelOpacity;
	vec4 combined = texture2D( u_texture1, megaST );
	combined = mix( combined, texture2D( u_texture2, megaST * 2.0 ), levelMask.x );
	combined = mix( combined, texture2D( u_texture3, megaST * 4.0 ), levelMask.y );
	combined = mix( combined, texture2D( u_texture4, megaST * 8.0 ), levelMask.z );
	combined = mix( combined, texture2D( u_texture5, megaST * 16.0 ), levelMask.w );
	vec4 detail = texture2D( u_texture6, megaDetailST.zw ) * 2.0 - 1.0;
	vec4 detailMask = texture2D( u_texture7, megaDetailST.xy );
	combined.rgb += combined.rgb * dot( detailMask, detail );
	gl_FragColor = vec4( combined.rgb * gl_Color.rgb, 1.0 );
}
