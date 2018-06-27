#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 color;

#include "common_structures.glsl"
#include "utils.glsl"
#include "brdf.glsl"

layout (binding = 0) uniform usampler2D g_buffer_0;// packed normals, mesh id + 24 bit depth
layout (binding = 1) uniform sampler2D g_buffer_1; // albedo
layout (binding = 2) uniform sampler2D g_buffer_2; // reflections & ior
layout (binding = 3) uniform sampler2D g_buffer_3; // 32-bit depth

layout (binding = 4) uniform CameraInfo
{
	VkCamera data;
} camera;

layout (binding = 5) uniform sampler2D shadow_map[kMaxLights];

// Lights info
layout (binding = 6) uniform PointLightsInfo
{
	VkPointLight data[kMaxLights];
} point_lights;

layout (binding = 7) uniform SpotLightsInfo
{
	VkSpotLight data[kMaxLights];
} spot_lights;

layout (binding = 8) uniform DirectionalLightsInfo
{
	VkDirectionalLight data[kMaxLights];
} directional_lights;

// Light transforms info (view-proj matrices)
layout (binding = 9) uniform PointLightTransformInfo
{
	matrix view_proj[kMaxLights][6]; // view-proj for each side, unused for now
} point_lights_transforms;

layout (binding = 10) uniform SpotLightTransformInfo
{
	matrix view_proj[kMaxLights];
} spot_lights_view_proj;

layout (binding = 11) uniform DirectionalLightTransformInfo
{
	matrix 	view_proj[kMaxLights][4]; // view-proj for each cascade
} directional_lights_view_proj;

layout(push_constant) uniform PushConsts {
	VkDeferredPushConstants data;
} push_constants;

float SampleShadow(uint light_idx, vec3 p, vec3 shadow_uv, float bias, float scale)
{
	float shadow = 1.0f;

	for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
	{
		int index = int(16.0 * Rnd(floor(p.xyz * 255.0f), sampleIdx)) % 16;

		float occluded = texture(shadow_map[light_idx], vec2(shadow_uv.st + PoissonDisk[index] * scale)).r < shadow_uv.z - bias ? 1.0f : 0.0f;
		shadow -= 0.25 * occluded;
	}

	return shadow;
}

void main()
{
	uvec4 	data 		 = texture(g_buffer_0, uv);
	vec4 	buffer_data1 = texture(g_buffer_1, uv);

	vec3 N 				= (vec4(DecodeNormal(data.xy), 0.f) * camera.data.inv_view).xyz;
	vec2 depth_mesh_id 	= DecodeDepthAndMeshID(data.zw);

	// 32 bit depth instead 24 bits solves precision issues with shadows. TODO: Use inverse Z
	float depth			= texture(g_buffer_3, uv).x;
	//float depth			= depth_mesh_id.x;
	float mesh_id		= depth_mesh_id.y;

	float is_geometry	= mesh_id > 0.f ? 1.f : 0.f;

	vec3 view_pos = ReconstructVSPositionFromDepth(camera.data.inv_proj, uv, depth);
	vec3 world_pos = (vec4(view_pos, 1.0f) * camera.data.inv_view).xyz;
	vec3 V = normalize(camera.data.position.xyz - world_pos);

	uint num_lights = clamp(push_constants.data.num_lights[0], 0, kMaxLights);

	uint num_point_lights = clamp(push_constants.data.num_lights[1], 0, kMaxLights);
	uint num_spot_lights = clamp(push_constants.data.num_lights[2], 0, kMaxLights);
	uint num_directional_lights = clamp(push_constants.data.num_lights[3], 0, kMaxLights);

	BRDFInputs brdf_inputs;
	brdf_inputs.albedo = buffer_data1.xyz;
	brdf_inputs.roughness = buffer_data1.w;
	brdf_inputs.metallic = 0.f;
	brdf_inputs.transparency = 0.f;

	vec3 lighting = vec3(0.f);
	
	if (is_geometry == 1.f)
	{
		for (uint i = 0; i < num_spot_lights; i++)
		{
			vec3 light_pos = spot_lights.data[i].position.xyz;
			vec3 light_dir = spot_lights.data[i].direction.xyz;
			vec3 light_rad = spot_lights.data[i].radiance.xyz;

			vec3 L = light_pos - world_pos;

			float dist = length(L);
			L = L / dist;

			float light_ia = spot_lights.data[i].position.w;
			float light_oa = spot_lights.data[i].direction.w;
			float cos_dir = dot(-L, light_dir);
			
			vec3 spot = vec3(0.f);

			if (cos_dir > light_oa)
			{
				spot = cos_dir > light_ia ? light_rad : light_rad * (1.f - (light_ia - cos_dir) / (light_ia - light_oa));
			}
			
			vec3 BRDF = BRDF_Evaluate(brdf_inputs, V, N, L);
			vec4 light_ps = vec4(world_pos, 1.0f) * spot_lights_view_proj.view_proj[i];
			vec4 shadow_uv = light_ps / light_ps.w;

   			shadow_uv.st = shadow_uv.st * 0.5f + vec2(0.5f);
			float NdotL = max(dot(N, L), 0.f);
			
			uint shadow_map_idx = uint(spot_lights.data[i].radiance.w);

			float bias = clamp(0.0005f * tan(acos(NdotL)), 0.0005f, 0.01f);
			int shadow_size = textureSize(shadow_map[shadow_map_idx], 0).x;

			float shadow = SampleShadow(shadow_map_idx, world_pos, shadow_uv.xyz, bias, 1.f / shadow_size);
			lighting += shadow.x * NdotL * spot * BRDF / (dist * dist);
		}

		for (uint i = 0; i < num_directional_lights; i++)
		{
			vec3 light_dir = -directional_lights.data[i].direction.xyz;
			vec3 light_rad = directional_lights.data[i].radiance.xyz;

			float NdotL = max(dot(N, light_dir), 0.f);

			vec3 BRDF = BRDF_Evaluate(brdf_inputs, V, N, light_dir);
				
			vec4 cs_dist = push_constants.data.cascade_splits;
			float cascade_dists[4] = {cs_dist.x, cs_dist.y, cs_dist.z, cs_dist.w};

			uint cascade_idx = 0;
			for (uint i = 0; i < 4; i++)
			{
				if (-view_pos.z > cascade_dists[i])
					cascade_idx = i + 1;
			}
		
			const vec2 cascade_offsets[4] = vec2[] (vec2(0.25f, 0.25f), vec2(0.75f, 0.25f), vec2(0.25f, 0.75f), vec2(0.75f, 0.75f));	
			const vec2 cascade_scales[4] = vec2[] (vec2(0.25f, 0.25f), vec2(0.25f, 0.25f), vec2(0.25f, 0.25f), vec2(0.25f, 0.25f));

			vec4 shadow_coords = vec4(world_pos, 1.0f) * directional_lights_view_proj.view_proj[i][cascade_idx];
			shadow_coords.xy = shadow_coords.xy * cascade_scales[cascade_idx] + cascade_offsets[cascade_idx];
			
			uint shadow_map_idx = uint(directional_lights.data[i].radiance.w);
			ivec2 half_shadow_size = textureSize(shadow_map[shadow_map_idx], 0) >> 1;
			
			float bias = clamp(0.005f * tan(acos(NdotL)), 0.0f, 0.01f);
			float shadow = SampleShadow(shadow_map_idx, world_pos, shadow_coords.xyz, bias / (cascade_idx + 1), 1.f / (half_shadow_size.x * (cascade_idx + 1)));

			lighting += NdotL.xxx * shadow * BRDF * light_rad;
/*
			bool visualise_cascades = true;
			const vec3 cascade_color[4] = vec3[](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1), vec3(1,1,0));
			vec3 color = visualise_cascades ? cascade_color[cascade_idx] : vec3(1,1,1);

			lighting += color * NdotL.xxx * shadow * BRDF * light_rad;
*/
		}

		for (uint i = 0; i < num_point_lights; i++)
		{
			vec3 light_pos = point_lights.data[i].position.xyz;
			vec3 light_intensity = point_lights.data[i].radiance.xyz;

			vec3 L = light_pos - world_pos;

			float dist = max(length(L), EPS);
			L = L / dist;

			float NdotL = max(dot(N, L), 0.f);

			vec3 BRDF = BRDF_Evaluate(brdf_inputs, V, N, L);

			lighting += NdotL.xxx * BRDF * light_intensity / (dist * dist);
		}
	}

	vec3 ambient	= vec3(0.0f);//buffer_data1.xyz * 0.01f;
	vec3 final 		= lighting + ambient;
	color			= pow(vec4(final, 1.f), vec4(1.f / 2.2f));
}