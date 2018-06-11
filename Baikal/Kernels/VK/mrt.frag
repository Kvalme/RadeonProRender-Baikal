#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "utils.glsl"

layout (location = 0) in vec4 proj_pos;
layout (location = 1) in vec4 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 view_vector;

layout (location = 0) out uvec4 out_gbuffer_0;  // xy - packed normals, next 24 bits - depth, 8 bits - mesh id
layout (location = 1) out vec4 out_gbuffer_1;   // albedo
layout (location = 2) out vec4 out_gbuffer_2;   // xy - motion, zw - roughness, metaliness

layout(push_constant) uniform PushConsts {
    uvec4 mesh_id;
} push_constants;

void main()
{
	vec3 n = normalize(normal.xyz);

	// matrix for TBN calculation on the fly
	// mat3 contangentFrame = CotangentFrame(n, -view_vector, uv);

	float mesh_id = 0.f;

	out_gbuffer_0 = uvec4(EncodeNormal(n), EncodeDepthAndMeshID(proj_pos.z / proj_pos.w, push_constants.mesh_id.x));
	out_gbuffer_1 = vec4(0,0,0,1);
	out_gbuffer_2 = vec4(0,0, 0, 1);
}