#version 120

uniform samplerCube u_texture0;

void main() {
	vec3 normal = normalize( gl_TexCoord[0].xyz );
	vec3 toEye = normalize( gl_TexCoord[1].xyz );
	vec3 reflection = 2.0 * dot( toEye, normal ) * normal - toEye;
	gl_FragColor = textureCube( u_texture0, reflection ) * gl_Color;
}
