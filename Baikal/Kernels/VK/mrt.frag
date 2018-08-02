#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"
#include "utils.glsl"
#include "fragment_shader_utils.glsl"
#include "common_structures.glsl"

layout (location = 0) in vec4 proj_pos;
layout (location = 1) in vec4 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 position;
layout (location = 5) in flat matrix view;
layout (location = 9) in vec4 position_ps;
layout (location = 10) in vec4 prev_position_ps;
layout (location = 11) in flat vec2 camera_jitter;
layout (location = 12) in flat uint mesh_id;

layout (location = 0) out vec4 out_gbuffer_0;	// normals
layout (location = 1) out vec4 out_gbuffer_1;   // albedo, ior
layout (location = 2) out vec4 out_gbuffer_2;   // motion
layout (location = 3) out vec4 out_gbuffer_3;	// roughness, metalness, mesh id, transparency

layout(push_constant) uniform PushConsts {
    layout(offset = 16) VkMaterialConstants data;
} material;

layout (binding = 4) uniform sampler2D textures[kMaxTextures];

void main()
{
	vec3 n = normalize(normal.xyz);

	int diffuse_idx 		= int(material.data.diffuse.w);
	int normal_idx 			= int(material.data.normal.w);
	int metalness_idx   	= int(material.data.metalness.w);
	int roughness_idx   	= int(material.data.roughness.w);
	int ior_idx   			= int(material.data.ior.w);
	int transparency_idx 	= int(material.data.transparency.w);

	vec4 diffuse 	 	= diffuse_idx < 0 ? material.data.diffuse : texture(textures[diffuse_idx], uv);
	vec3 metalness   	= metalness_idx < 0 ? material.data.metalness.xyz : texture(textures[metalness_idx], uv).xyz;
	vec3 roughness 	 	= roughness_idx < 0 ? material.data.roughness.xyz : texture(textures[roughness_idx], uv).xyz;
	vec3 ior	 	 	= ior_idx < 0 ? material.data.ior.xyz : texture(textures[ior_idx], uv).xyz;
	vec3 transparency 	= transparency_idx < 0 ? material.data.transparency.xyz : texture(textures[transparency_idx], uv).xyz;

	// convert to linear space
	diffuse.xyz = diffuse_idx >=0 ? pow(diffuse.xyz, vec3(2.2f)) : diffuse.xyz;

	if (normal_idx >= 0)
	{
		// TBN calculation on the fly
		mat3 TBN = CotangentFrame(n, position, uv);

		vec3 bump2normal = ConvertBumpToNormal(textures[normal_idx], uv).xyz;
		n = TBN * bump2normal;
	}

	vec2 position_ss 		= (position_ps.xy / position_ps.w) * vec2(0.5f, 0.5f) + 0.5f;
	vec2 prev_position_ss 	= (prev_position_ps.xy / prev_position_ps.w) * vec2(0.5f, 0.5f) + 0.5f;

	vec2 motion = position_ss - prev_position_ss - camera_jitter;

	out_gbuffer_0 = vec4(n * 0.5f + 0.5f, transparency.x);
	out_gbuffer_1 = vec4(diffuse.xyz, 1.0);
	out_gbuffer_2 = vec4(motion, 0, 0);
	out_gbuffer_3 = vec4(roughness.x, metalness.x, mesh_id, ior.x);
}
