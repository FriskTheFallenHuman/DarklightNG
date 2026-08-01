#version 120

uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform sampler2D u_texture5;
uniform sampler2D u_texture6;
uniform vec4 u_fragmentParm[8];

void main() {
	vec3 toLight = normalize( gl_TexCoord[0].xyz );
	vec3 toViewer = normalize( gl_TexCoord[6].xyz );
	vec4 normalSample = texture2D( u_texture1, gl_TexCoord[1].xy );
	vec3 localNormal = normalize( normalSample.xyz * 2.0 - 1.0 );

	float light = dot( toLight, localNormal );
	light *= texture2DProj( u_texture3, gl_TexCoord[3] ).r;
	light *= texture2DProj( u_texture2, gl_TexCoord[2] ).r;

	vec4 color = texture2D( u_texture4, gl_TexCoord[4].xy ) * u_fragmentParm[0];
	vec3 reflection = reflect( -toLight, localNormal );
	float specular = dot( toViewer, reflection );
	vec4 specularTerm = texture2D( u_texture6, vec2( specular, 0.2 ) ) * u_fragmentParm[1];
	specularTerm *= texture2D( u_texture5, gl_TexCoord[5].xy ) * 2.0;
	gl_FragColor = ( color + specularTerm ) * light * gl_Color;
}
