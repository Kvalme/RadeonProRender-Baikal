
#include "common.glsl"

const float CosineA0 = PI;
const float CosineA1 = (2.0f * PI) / 3.0f;
const float CosineA2 = PI / 4.0f;

struct SH9
{
    float sh[9];
};

SH9 SH_Get2ndOrderCoeffs(vec3 d)
{
    SH9 sh9;

    d = normalize(d);

    float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
    float pz2 = d.z * d.z;

    sh9.sh[0] = 0.2820947917738781f * CosineA0;
    sh9.sh[2] = 0.4886025119029199f * d.z * CosineA1;
    sh9.sh[6] = 0.9461746957575601f * pz2 + -0.3153915652525201f;
    fC0 = d.x;
    fS0 = d.y;
    fTmpA = -0.48860251190292f;
    sh9.sh[3] = fTmpA * fC0 * CosineA1;
    sh9.sh[1] = fTmpA * fS0 * CosineA1;
    fTmpB = -1.092548430592079f * d.z;
    sh9.sh[7] = fTmpB * fC0 * CosineA2;
    sh9.sh[5] = fTmpB * fS0 * CosineA2;
    fC1 = d.x*fC0 - d.y*fS0;
    fS1 = d.x*fS0 + d.y*fC0;
    fTmpC = 0.5462742152960395f;
    sh9.sh[8] = fTmpC * fC1 * CosineA2;
    sh9.sh[4] = fTmpC * fS1 * CosineA2;

    return sh9;
}

vec3 EvaluateSHIrradiance(vec3 dir, VkSH9Color radiance)
{
    SH9 shBasis = SH_Get2ndOrderCoeffs(dir);

    vec3 irradiance = vec3(0.0f);

    for(int i = 0; i < 9; ++i)
    {
        irradiance += radiance.coefficients[i].xyz * shBasis.sh[i];
    }

    return irradiance;
}