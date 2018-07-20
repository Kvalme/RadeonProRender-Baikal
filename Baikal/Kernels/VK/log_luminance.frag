#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common_structures.glsl"
#include "tonemap.glsl"
#include "common.glsl"
#include "utils.glsl"

layout (binding = 0) uniform sampler2D image;

layout (location = 0) in vec2 tex_coord;
layout (location = 0) out vec4 color;

void main()
{
	color = log(Luminance(max(textureLod(image, tex_coord, 0.f).rgb, 0.0f) + 0.00001f).xxxx);
}