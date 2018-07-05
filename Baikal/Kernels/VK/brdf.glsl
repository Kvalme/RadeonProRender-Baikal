#include "common.glsl"

struct BRDFInputs
{
    vec3 albedo;
    float roughness;
    float metallic;
    float transparency;
};

float GGX_D(float roughness, float NdotH)
{
    float a2 = roughness * roughness;
    float v = (NdotH * NdotH * (a2 - 1) + 1);
    return a2 / (PI * v * v);
}

float GGX_G1(vec3 N, vec3 V, float k)
{
    const float NdotV = clamp(dot(N, V), 0.0, 1.0);
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GGX_G(float roughness, vec3 N, vec3 V, vec3 L)
{
    const float t = roughness + 1.0;
    const float k = t * t / 8.0;

    return GGX_G1(N, V, k) * GGX_G1(N, L, k);
}

vec3 FresnelSchlick(vec3 albedo, vec3 H, vec3 V)
{
    const float VdotH = clamp(dot(V, H), 0.0, 1.0);
    return (albedo + (1.0f - albedo) * pow(1.0 - VdotH, 5.0));
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 BRDF_Evaluate(BRDFInputs inputs,
    vec3 V,
    vec3 N,
    vec3 L)
{
    vec3 specAlbedo = mix(vec3(0.03f), inputs.albedo, inputs.metallic);

    vec3 H = normalize(L + V);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);

    vec3 diffuse = (1.0 - inputs.metallic) * inputs.albedo.xyz / PI;

    float D = GGX_D(inputs.roughness, NdotH);
    float G = GGX_G(inputs.roughness, N, V, L);
    vec3  F = FresnelSchlick(specAlbedo, H, V);
    vec3 specular = (D * G * F) / (4.0f * NdotL * NdotV + EPS);

    return diffuse + specular;
}