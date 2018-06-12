#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 color;

#include "common.glsl"
#include "utils.glsl"

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

	vec3 n 				= DecodeNormal(data.xy);
	vec2 depth_mesh_id 	= DecodeDepthAndMeshID(data.zw);

	float depth			= depth_mesh_id.x;
	float mesh_id		= depth_mesh_id.y;

	float is_geometry	= mesh_id > 0.f ? 1.f : 0.f;

	vec3 p = ReconstructVSPositionFromDepth(camera.data.inv_proj, uv, depth);

	uint num_lights = push_constants.num_lights.x;
	
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
				vec3 l = normalize(-data1.xyz);
				float NdotL = max(dot(n, l), 0.f);

				lighting += NdotL.xxx;
			}
		}
	}

	//color	= vec4(n, 1.f);
	//color	= vec4(p, 1.f);
	//color	= vec4(mesh_id.xxx / 8, 1.f);
	color	= vec4(lighting, 1.f);
}