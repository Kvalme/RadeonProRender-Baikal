#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common_structures.glsl"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;

layout (location = 0) out vec4 proj_pos;
layout (location = 1) out vec4 normal;
layout (location = 2) out vec2 uv;
layout (location = 3) out vec3 position;
layout (location = 5) out flat matrix view;
layout (location = 9) out vec4 position_ps;
layout (location = 10) out vec4 prev_position_ps;
layout (location = 11) out flat vec2 camera_jitter;

layout (binding = 0) uniform CameraInfo
{
	VkCamera data;
} camera;

layout (binding = 1) uniform TransformInfo
{
	matrix data[kMaxTransforms];
} transforms;

layout (binding = 2) uniform JitterInfo
{
	VkJitterBuffer data;
} jitter;

layout(push_constant) uniform VsPushConsts {
    uint data[4]; // transform id
} vs_consts;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	uint transform_idx 	= clamp(vs_consts.data[0], 0, kMaxTransforms);
	matrix	transform 	= transforms.data[transform_idx];

	position_ps 		= inPosition * transform * camera.data.view_proj * jitter.data.jitter;
	prev_position_ps 	= inPosition * transform * camera.data.prev_view_proj * jitter.data.prev_jitter;

	normal 				= inNormal * transform;
	uv					= inUV.xy;
	position 			= (inPosition * transform).xyz;

	view				= camera.data.view;
	camera_jitter 		= jitter.data.offsets.xy;

	gl_Position 		= position_ps;
}