#version 120

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform vec4 u_fragmentParm[8];

void main() {
	vec2 screenTexCoord = gl_FragCoord.xy * u_fragmentParm[1].xy * u_fragmentParm[0].xy;
	vec3 distortion = texture2D( u_texture0, gl_TexCoord[0].xy ).xyz * 2.0 - 1.0;
	vec2 offset = distortion.xy * 0.01;
	vec4 firstView = texture2D( u_texture1, clamp( screenTexCoord + offset, 0.0, 1.0 ) );
	vec4 secondView = texture2D( u_texture2, clamp( screenTexCoord - offset, 0.0, 1.0 ) );
	gl_FragColor = mix( firstView, secondView, 0.5 ) * gl_Color;
}
