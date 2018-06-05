#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common.glsl"

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 color;

void main()
{
	color = vec4(texCoord, 0.f, 0.f);
}