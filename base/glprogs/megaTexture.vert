#version 120

attribute vec4 attr_TexCoord;

uniform vec4 u_vertexParm[32];

varying vec2 megaST;
varying vec4 megaMaskX;
varying vec4 megaMaskY;
varying vec4 megaLevelOpacity;
varying vec4 megaDetailST;

void main() {
	vec4 atlasScale = vec4( u_vertexParm[1].w, u_vertexParm[2].w, u_vertexParm[3].w, u_vertexParm[4].w );
	vec4 atlasOffsetX = vec4( u_vertexParm[1].x, u_vertexParm[2].x, u_vertexParm[3].x, u_vertexParm[4].x );
	vec4 atlasOffsetY = vec4( u_vertexParm[1].y, u_vertexParm[2].y, u_vertexParm[3].y, u_vertexParm[4].y );

	megaST = attr_TexCoord.xy;
	megaMaskX = 0.5 - ( megaST.x * atlasScale + atlasOffsetX );
	megaMaskY = 0.5 - ( megaST.y * atlasScale + atlasOffsetY );
	megaLevelOpacity = u_vertexParm[7];
	megaDetailST.xy = megaST;
	megaDetailST.zw = ( megaST - 0.5 ) * u_vertexParm[8].x;
	gl_FrontColor = gl_Color;
	gl_Position = ftransform();
}
