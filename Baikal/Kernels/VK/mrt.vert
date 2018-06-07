#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common.glsl"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;

layout (location = 0) out vec4 position;
layout (location = 1) out vec4 normal;
layout (location = 2) out vec2 uv;

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
	vec4 positionPS = inPosition * camera.data.view_projection;

	gl_Position = positionPS;

	position 	= positionPS;
	normal 		= inNormal;
	uv			= inUV.xy;
}