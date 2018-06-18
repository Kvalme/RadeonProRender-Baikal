#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "utils.glsl"
#include "common_structures.glsl"

layout (location = 0) in vec4 proj_pos;
layout (location = 1) in vec4 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 position;
layout (location = 4) in matrix view;

layout (location = 0) out uvec4 out_gbuffer_0;  // xy - packed normals, next 24 bits - depth, 8 bits - mesh id
layout (location = 1) out vec4 out_gbuffer_1;   // albedo
layout (location = 2) out vec4 out_gbuffer_2;   // xy - motion, zw - roughness, metaliness

layout(push_constant) uniform PushConsts {
    uvec4 	data; 			// mesh id
	vec4 	diffuse;		// xyz - color, w - texture id
	vec4 	reflection;
    vec4 	roughness;
    vec4 	ior;
    vec4	normal;
} consts;

layout (binding = 1) uniform sampler2D textures[kMaxTextures];

void main()
{
	vec3 n = normalize(normal.xyz);

	float mesh_id 		= consts.data.x;

	int diffuse_idx 	= int(consts.diffuse.w);
	int normal_idx 		= int(consts.normal.w);
	int reflection_idx  = int(consts.reflection.w);
	int roughness_idx   = int(consts.roughness.w);
	int ior_idx			= int(consts.ior.w);

	vec2 uv_ = vec2(uv.x, 1.0f - uv.y);

	vec3 diffuse 	= diffuse_idx < 0 ? consts.diffuse.xyz : pow(texture(textures[diffuse_idx], uv_).xyz, vec3(2.2f));
	vec3 reflection = reflection_idx < 0 ? consts.reflection.xyz : texture(textures[reflection_idx], uv_).xyz;
	vec3 roughness 	= roughness_idx < 0 ? consts.roughness.xyz : texture(textures[roughness_idx], uv_).xyz;
	vec3 ior 		= ior_idx < 0 ? consts.ior.xyz : texture(textures[ior_idx], uv_).xyz;
	
	if (normal_idx >= 0)
	{
		// TBN calculation on the fly
		mat3 TBN = CotangentFrame(n, position, uv_);

		vec3 bump2normal = ConvertBumpToNormal(textures[normal_idx], uv_).xyz;
		n = TBN * bump2normal;
	}

	out_gbuffer_0 = uvec4(EncodeNormal((vec4(n, 0.0f) * view).xyz), EncodeDepthAndMeshID(proj_pos.z / proj_pos.w, mesh_id));
	out_gbuffer_1 = vec4(diffuse, roughness.x);
	out_gbuffer_2 = vec4(reflection.xyz, ior.x);
}