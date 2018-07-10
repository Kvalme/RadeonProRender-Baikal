#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 color;

layout (binding = 1) uniform sampler2D tex;

void main()
{
	color = texture(tex, texCoord);
}