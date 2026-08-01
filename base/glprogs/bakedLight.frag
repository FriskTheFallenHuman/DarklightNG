#version 120

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform sampler2D u_texture3;
uniform sampler2D u_texture4;
uniform vec4 u_fragmentParm[8];

void main() {
	vec4 normalSample = texture2D( u_texture0, gl_TexCoord[0].xy );
	normalSample.x = normalSample.a;
	vec3 normal = normalSample.xyz * 2.0 - 1.0;

	vec3 direction = texture2D( u_texture3, gl_TexCoord[3].xy ).xyz * 2.0 - 1.0;
	direction = normalize( direction );
	float directionFactor = clamp( max( dot( normal, direction ), 0.0 ) / max( direction.z, 0.25 ), 0.0, 2.0 );

	vec4 material = texture2D( u_texture1, gl_TexCoord[1].xy );
	vec4 light = texture2D( u_texture2, gl_TexCoord[2].xy );
	vec4 resultColor = material * u_fragmentParm[0] * light * directionFactor * u_fragmentParm[1].x;

	vec3 viewDirection = normalize( gl_TexCoord[4].xyz );
	vec3 halfAngle = normalize( direction + viewDirection );
	float specular = dot( normal, halfAngle );
	vec4 specularTerm = texture2D( u_texture4, vec2( specular ) );
	specularTerm *= material * 2.0 * u_fragmentParm[0] * light * directionFactor * u_fragmentParm[1].y;
	gl_FragColor = ( resultColor + specularTerm ) * gl_Color;
}
