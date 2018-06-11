#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common.glsl"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;

layout (location = 0) out vec4 proj_pos;
layout (location = 1) out vec4 normal;
layout (location = 2) out vec2 uv;
layout (location = 3) out vec3 view_vector;

layout (binding = 0) uniform CameraInfo
{
	VkCamera data;
} camera;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	proj_pos 	= inPosition * camera.data.view_projection;
	normal 		= inNormal;
	uv			= inUV.xy;

	view_vector = camera.data.camera_position.xyz - inPosition.xyz;

	gl_Position = proj_pos;
}