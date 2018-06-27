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
layout (location = 4) out matrix view;

layout (binding = 0) uniform CameraInfo
{
	VkCamera data;
} camera;

layout (binding = 1) uniform TransformInfo
{
	matrix data[1024];
} transforms;

layout(push_constant) uniform VsPushConsts {
    uvec4 	data; // mesh id
} vs_consts;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	uint transform_idx 	= max(0, vs_consts.data.x - 1);
	matrix	transform 	= transforms.data[transform_idx];

	proj_pos 	= inPosition * transform * camera.data.view_proj;
	normal 		= inNormal * transform;
	uv			= inUV.xy;

	view		= camera.data.view;
	position 	= inPosition.xyz;

	gl_Position = proj_pos;
}