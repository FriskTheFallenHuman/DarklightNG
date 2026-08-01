#version 120

uniform samplerCube u_texture0;
uniform sampler2D u_texture1;

void main() {
	vec4 normalSample = texture2D( u_texture1, gl_TexCoord[0].xy );
	normalSample.x = normalSample.a;
	vec3 localNormal = normalize( normalSample.xyz * 2.0 - 1.0 );
	vec3 globalNormal = vec3( dot( localNormal, gl_TexCoord[2].xyz ), dot( localNormal, gl_TexCoord[3].xyz ), dot( localNormal, gl_TexCoord[4].xyz ) );
	vec3 globalEye = normalize( gl_TexCoord[1].xyz );
	vec3 reflection = 2.0 * dot( globalEye, globalNormal ) * globalNormal - globalEye;
	gl_FragColor = vec4( textureCube( u_texture0, reflection ).rgb, 1.0 );
}
