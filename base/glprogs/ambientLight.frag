#version 120

uniform samplerCube u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform vec4 u_fragmentParm[8];

void main() {
	vec4 normalSample = texture2D( u_texture1, gl_TexCoord[0].xy );
	normalSample.x = normalSample.a;
	vec3 localNormal = normalSample.xyz * 2.0 - 1.0;
	vec3 ambientDirection = vec3( dot( localNormal, gl_TexCoord[4].xyz ), dot( localNormal, gl_TexCoord[5].xyz ), dot( localNormal, gl_TexCoord[6].xyz ) );
	vec4 light = textureCube( u_texture0, ambientDirection );
	light *= texture2DProj( u_texture4, gl_TexCoord[1] );
	light *= texture2DProj( u_texture2, gl_TexCoord[2] );
	light *= texture2DProj( u_texture3, gl_TexCoord[3] );
	gl_FragColor = light * u_fragmentParm[0] * gl_Color;
}
