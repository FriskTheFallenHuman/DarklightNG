#version 120

uniform sampler2D u_texture0;
uniform vec4 u_fragmentParm[8];

void main() {
	vec2 screenTexCoord = gl_FragCoord.xy * u_fragmentParm[1].xy * u_fragmentParm[0].xy;
	vec4 sourceColor = texture2D( u_texture0, screenTexCoord );
	float gray = ( sourceColor.r + sourceColor.g + sourceColor.b ) * 0.33;
	vec3 targetColor = gray * gl_TexCoord[1].rgb;
	gl_FragColor = vec4( sourceColor.rgb * gl_TexCoord[0].rgb + targetColor, sourceColor.a );
}
