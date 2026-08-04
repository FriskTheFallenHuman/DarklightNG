#version 120

uniform samplerCube u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform vec4 u_fragmentParm[32];

void main() {
	vec4 normalSample = texture2D( u_texture1, gl_TexCoord[0].xy );
	normalSample.x = normalSample.a;
	vec3 localNormal = normalSample.xyz * 2.0 - 1.0;
	vec3 worldNormal = normalize( gl_TexCoord[2].xyz * localNormal.x +
		gl_TexCoord[3].xyz * localNormal.y + gl_TexCoord[4].xyz * localNormal.z );
	vec4 material = texture2D( u_texture2, gl_TexCoord[1].xy ) * u_fragmentParm[0] * gl_Color;

	vec3 cubeColor;
	if ( u_fragmentParm[1].x > 0.5 ) {
		cubeColor = textureCube( u_texture0, worldNormal ).rgb * u_fragmentParm[2].x;
	} else {
		vec3 incident = -normalize( gl_TexCoord[5].xyz );
		cubeColor = textureCube( u_texture0, reflect( incident, worldNormal ) ).rgb * 2.0;
	}
	gl_FragColor = vec4( material.rgb * cubeColor * u_fragmentParm[2].y, material.a );
}
