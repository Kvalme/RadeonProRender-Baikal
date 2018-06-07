#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 normal;
layout (location = 2) in vec2 uv;
//layout (location = 2) in vec2 uv;

layout (location = 0) out vec4 out_positions;
layout (location = 1) out vec4 out_normals;
layout (location = 2) out vec4 out_uvs;

void main()
{
	out_positions = position;
	out_normals = normal;
	out_uvs = vec4(uv, 0, 1);
}