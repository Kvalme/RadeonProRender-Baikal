#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 color;

#include "common.glsl"
#include "utils.glsl"

layout (binding = 0) uniform CameraInfo
{
	VkCamera data;
} camera;

layout (binding = 1) uniform usampler2D g_buffer_0;
layout (binding = 2) uniform sampler2D g_buffer_1;
layout (binding = 3) uniform sampler2D g_buffer_2;
layout (binding = 4) uniform sampler2D g_buffer_3;

void main()
{
	uvec4 data = texture(g_buffer_0, uv);

	vec3 n 				= DecodeNormal(data.xy);
	vec2 depth_mesh_id 	= DecodeDepthAndMeshID(data.zw);

	float depth			= depth_mesh_id.x;
	float mesh_id		= depth_mesh_id.y;

	vec3 p = ReconstructVSPositionFromDepth(camera.data.inv_projection, uv, depth);

	color	= vec4(n, 1.f);
	//color	= vec4(p, 1.f);
	//color	= vec4(mesh_id.xxx / 8, 1.f);
}