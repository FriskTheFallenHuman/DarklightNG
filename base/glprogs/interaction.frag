#version 120

uniform samplerCube u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform sampler2D u_texture5;
uniform sampler2D u_texture6;
uniform vec4 u_fragmentParm[8];

void main() {
	vec3 halfAngle = normalize( gl_TexCoord[6].xyz );
	vec3 lightDirection = textureCube( u_texture0, gl_TexCoord[0].xyz ).xyz * 2.0 - 1.0;

	vec4 normalSample = texture2D( u_texture1, gl_TexCoord[1].xy );
	normalSample.x = normalSample.a;
	vec3 localNormal = normalSample.xyz * 2.0 - 1.0;
	float light = dot( lightDirection, localNormal );
	light *= texture2DProj( u_texture3, gl_TexCoord[3] ).r;
	light *= texture2DProj( u_texture2, gl_TexCoord[2] ).r;

	vec4 color = texture2D( u_texture4, gl_TexCoord[4].xy ) * u_fragmentParm[0];
	float specular = dot( halfAngle, localNormal );
	vec4 specularTerm = texture2D( u_texture6, vec2( specular ) ) * u_fragmentParm[1];
	specularTerm *= texture2D( u_texture5, gl_TexCoord[5].xy ) * 2.0;
	color += specularTerm;
	gl_FragColor = color * light * gl_Color;
}
