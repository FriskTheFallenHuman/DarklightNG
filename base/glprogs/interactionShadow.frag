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
	vec4 shadowCoord = vec4( gl_TexCoord[2].zw, gl_TexCoord[3].z, gl_TexCoord[0].w );
	vec2 faceCoord = shadowCoord.xy / shadowCoord.w;
	if ( faceCoord.x < 0.0 || faceCoord.y < 0.0 || faceCoord.x > 1.0 || faceCoord.y > 1.0 || shadowCoord.w < 0.0 ) {
		discard;
	}
	float shadowDepth = texture2DProj( u_texture6, shadowCoord ).r;
	float receiverDepth = shadowCoord.z / shadowCoord.w;
	float lit = shadowDepth >= receiverDepth ? 1.0 : 0.0;

	vec3 halfAngle = normalize( gl_TexCoord[6].xyz );
	vec3 lightDirection = textureCube( u_texture0, gl_TexCoord[0].xyz ).xyz * 2.0 - 1.0;
	vec4 normalSample = texture2D( u_texture1, gl_TexCoord[1].xy );
	normalSample.x = normalSample.a;
	vec3 localNormal = normalSample.xyz * 2.0 - 1.0;
	float light = dot( lightDirection, localNormal );
	light *= texture2DProj( u_texture3, gl_TexCoord[3] ).r;
	light *= texture2D( u_texture2, gl_TexCoord[2].xy ).r * lit;

	vec4 color = texture2D( u_texture4, gl_TexCoord[4].xy ) * u_fragmentParm[0];
	float specular = clamp( ( dot( halfAngle, localNormal ) - 0.75 ) * 4.0, 0.0, 1.0 );
	specular *= specular;
	vec4 specularTerm = vec4( specular ) * u_fragmentParm[1];
	specularTerm *= texture2D( u_texture5, gl_TexCoord[5].xy ) * 2.0;
	gl_FragColor = ( color + specularTerm ) * light * gl_Color;
}
