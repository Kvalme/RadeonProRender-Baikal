#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 color;

#include "common_structures.glsl"
#include "utils.glsl"
#include "brdf.glsl"

layout (binding = 0) uniform usampler2D g_buffer_0;
layout (binding = 1) uniform sampler2D g_buffer_1;
layout (binding = 2) uniform sampler2D g_buffer_2;
layout (binding = 3) uniform sampler2D g_buffer_3;

layout (binding = 4) uniform CameraInfo
{
	VkCamera data;
} camera;

layout (binding = 5) uniform LightsInfo
{
	VkLight data[kMaxLights];
} lights;

layout(push_constant) uniform PushConsts {
    uvec4 num_lights;
} push_constants;

void main()
{
	uvec4 data = texture(g_buffer_0, uv);

	vec3 N 				= DecodeNormal(data.xy);
	vec2 depth_mesh_id 	= DecodeDepthAndMeshID(data.zw);

	float depth			= depth_mesh_id.x;
	float mesh_id		= depth_mesh_id.y;

	float is_geometry	= mesh_id > 0.f ? 1.f : 0.f;

	vec3 view_pos = ReconstructVSPositionFromDepth(camera.data.inv_proj, uv, depth);
	vec3 world_pos = (vec4(view_pos, 1.0f) * camera.data.inv_view).xyz;
	vec3 V = normalize(camera.data.position.xyz - world_pos);

	uint num_lights = push_constants.num_lights.x;
	
	BRDFInputs brdf_inputs;
	brdf_inputs.albedo = vec3(1.f, 1.f, 1.f);
	brdf_inputs.roughness = 0.f;
	brdf_inputs.metallic = 0.f;
	brdf_inputs.transparency = 0.f;

	vec3 lighting = vec3(0.f);

	if (is_geometry == 1.f)
	{
		for (uint i = 0; i < num_lights; i++)
		{
			vec4 data0 = lights.data[i].data0;
			vec4 data1 = lights.data[i].data1;
			vec4 data2 = lights.data[i].data2;

			uint type = uint(data0.w);
			
			if (type == kIbl || type == kArea)
				continue;

			if (type == kDirectional)
			{
				vec3 L = -data1.xyz;
				vec3 light_intensity = data2.xyz;

				float NdotL = max(dot(N, L), 0.f);

				vec3 BRDF = BRDF_Evaluate(brdf_inputs, V, N, L);

				lighting += NdotL.xxx * BRDF * light_intensity;
			}

			if (type == kPoint)
			{
				vec3 light_pos = data0.xyz;
				vec3 light_intensity = data2.xyz;

				vec3 L = light_pos - world_pos;

				float dist = max(length(L), EPS);
				L = L / dist;

				float NdotL = max(dot(N, L), 0.f);

				vec3 BRDF = BRDF_Evaluate(brdf_inputs, V, N, L);

				lighting += NdotL.xxx * BRDF * light_intensity / (dist * dist);
			}

			if (type == kSpot)
			{
				vec3 light_pos = data0.xyz;
				vec3 light_dir = normalize(data1.xyz);
				vec3 light_intensity = data2.xyz;

				vec3 L = light_pos - world_pos;

				float dist = length(L);
				L = L / dist;

				float light_ia = data1.w;
				float light_oa = data2.w;

				float cos_dir = dot(-L, light_dir);
				
				vec3 spot = vec3(0.f);

				if (cos_dir > light_oa)
				{
					spot = cos_dir > light_ia ? light_intensity : light_intensity * (1.f - (light_ia - cos_dir) / (light_ia - light_oa));
				}
				
				vec3 BRDF = BRDF_Evaluate(brdf_inputs, V, N, L);

				float NdotL = max(dot(N, L), 0.f);
				lighting += NdotL * spot * BRDF / (dist * dist);
			}
		}
	}

	//color	= vec4(n, 1.f);
	//color	= vec4(p, 1.f);
	//color	= vec4(mesh_id.xxx / 8, 1.f);
	color	= pow(vec4(lighting, 1.f), vec4(1.f / 2.2f));
	//color = vec4(world_pos, 1.0f);
}