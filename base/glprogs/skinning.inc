#extension GL_ARB_uniform_buffer_object : require

attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;
attribute vec3 attr_Normal;
attribute vec4 attr_JointIndices;
attribute vec4 attr_JointWeights;

layout(std140) uniform matrices_ubo {
	vec4 u_jointMatrices[408];
};

uniform bool u_gpuSkinning;

void gpuSkinRows( out vec4 rowX, out vec4 rowY, out vec4 rowZ ) {
	int joint = int( attr_JointIndices.x * 255.1 ) * 3;
	rowX = u_jointMatrices[joint + 0] * attr_JointWeights.x;
	rowY = u_jointMatrices[joint + 1] * attr_JointWeights.x;
	rowZ = u_jointMatrices[joint + 2] * attr_JointWeights.x;

	joint = int( attr_JointIndices.y * 255.1 ) * 3;
	rowX += u_jointMatrices[joint + 0] * attr_JointWeights.y;
	rowY += u_jointMatrices[joint + 1] * attr_JointWeights.y;
	rowZ += u_jointMatrices[joint + 2] * attr_JointWeights.y;

	joint = int( attr_JointIndices.z * 255.1 ) * 3;
	rowX += u_jointMatrices[joint + 0] * attr_JointWeights.z;
	rowY += u_jointMatrices[joint + 1] * attr_JointWeights.z;
	rowZ += u_jointMatrices[joint + 2] * attr_JointWeights.z;

	joint = int( attr_JointIndices.w * 255.1 ) * 3;
	rowX += u_jointMatrices[joint + 0] * attr_JointWeights.w;
	rowY += u_jointMatrices[joint + 1] * attr_JointWeights.w;
	rowZ += u_jointMatrices[joint + 2] * attr_JointWeights.w;
}

vec4 gpuSkinnedPosition() {
	if ( !u_gpuSkinning ) {
		return gl_Vertex;
	}
	vec4 rowX;
	vec4 rowY;
	vec4 rowZ;
	gpuSkinRows( rowX, rowY, rowZ );
	return vec4( dot( rowX, gl_Vertex ), dot( rowY, gl_Vertex ), dot( rowZ, gl_Vertex ), 1.0 );
}

vec3 gpuSkinDirection( vec3 direction ) {
	if ( !u_gpuSkinning ) {
		return direction;
	}
	vec4 rowX;
	vec4 rowY;
	vec4 rowZ;
	gpuSkinRows( rowX, rowY, rowZ );
	vec4 vector = vec4( direction, 0.0 );
	return normalize( vec3( dot( rowX, vector ), dot( rowY, vector ), dot( rowZ, vector ) ) );
}

vec3 gpuSkinnedNormal() {
	return gpuSkinDirection( attr_Normal );
}

vec3 gpuSkinnedTangent() {
	return gpuSkinDirection( attr_Tangent );
}

vec3 gpuSkinnedBitangent() {
	return gpuSkinDirection( attr_Bitangent );
}
