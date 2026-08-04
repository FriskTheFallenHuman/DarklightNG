#version 120

uniform sampler2D u_texture0;
uniform vec4 u_fragmentParm[32];

const float PI = 3.14159265358979323846;

void main() {
	vec3 position = gl_TexCoord[0].xyz;
	vec3 eye = u_fragmentParm[5].xyz;

	// ETQW's original atmospheric integration nudges near-horizontal rays
	// to avoid division by a vanishing vertical component.
	if ( abs( eye.z - position.z ) < 5.0 ) {
		eye.z += 5.0;
	}

	vec3 ray = position - eye;
	float rayLength = max( length( ray ), 0.0001 );
	vec3 rayDirection = ray / rayLength;
	float cosTheta = rayDirection.z;

	float z1 = max( position.z + u_fragmentParm[0].z, 0.0 );
	float z2 = max( eye.z + u_fragmentParm[0].z, 0.0 );
	float density = pow( 0.5, z2 * u_fragmentParm[0].y ) -
		pow( 0.5, z1 * u_fragmentParm[0].y );
	float thickness = ( density / cosTheta ) * rayLength *
		u_fragmentParm[0].x * u_fragmentParm[0].w;
	float transmittance = clamp( pow( 0.5, max( thickness, 0.0 ) ), 0.0, 1.0 );
	float extinction = 1.0 - transmittance;

	vec2 skyUV;
	skyUV.x = atan( rayDirection.y, rayDirection.x ) / ( 2.0 * PI ) + 0.5;
	skyUV.y = asin( clamp( rayDirection.z, -1.0, 1.0 ) ) / PI + 0.5;
	vec3 gradient = texture2D( u_texture0, skyUV ).rgb;
	gradient = mix( u_fragmentParm[1].rgb, gradient, 0.75 );

	vec3 sunDirection = normalize( u_fragmentParm[2].xyz );
	float halo = max( dot( rayDirection, sunDirection ) * u_fragmentParm[4].x +
		u_fragmentParm[4].y, 0.0 );
	vec3 atmosphereColor = gradient + halo * u_fragmentParm[3].rgb;
	gl_FragColor = vec4( atmosphereColor * extinction, extinction );
}
