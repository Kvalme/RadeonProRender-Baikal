#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common_structures.glsl"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;

layout (binding = 0) uniform LightInfo
{
	matrix view_proj[16];
} lights;

layout (binding = 1) uniform TransformInfo
{
	matrix data[kMaxTransforms];
} transforms;

layout(push_constant) uniform VsPushConsts {
    uvec4 	data; // mesh id, light id
} vs_consts;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	uint transform_idx 	= clamp(vs_consts.data.x, 0, kMaxTransforms);
	matrix	transform 	= transforms.data[transform_idx];

	uint light_idx 		= clamp(vs_consts.data.y, 0, 16);
	matrix view_proj 	= lights.view_proj[light_idx];
	
	gl_Position 		= inPosition * transform * view_proj;
}