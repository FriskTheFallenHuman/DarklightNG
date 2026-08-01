#version 120

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform vec4 u_fragmentParm[8];

void main() {
	vec4 localNormal = texture2D( u_texture1, gl_TexCoord[1].xy );
	localNormal.x = localNormal.a;
	localNormal = localNormal * 2.0 - 1.0;
	vec4 screenTexCoord = gl_FragCoord * u_fragmentParm[1];
	screenTexCoord = clamp( screenTexCoord + localNormal * gl_TexCoord[2], 0.0, 1.0 );
	screenTexCoord *= u_fragmentParm[0];
	gl_FragColor = vec4( texture2D( u_texture0, screenTexCoord.xy ).rgb, 1.0 );
}
