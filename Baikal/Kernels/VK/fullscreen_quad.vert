#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec4 inPosition;

layout (location = 0) out vec2 texCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	gl_Position = inPosition;
	texCoord = inPosition.xy * 0.5f + 0.5f;
}
