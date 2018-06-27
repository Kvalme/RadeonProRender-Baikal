const int kPoint = 0x1;
const int kDirectional = 0x2;
const int kSpot = 0x3;
const int kIbl = 0x4;
const int kArea = 0x5;

const int kMaxTextures = 64;
const int kMaxLights = 20;
const int kMaxTransforms = 1024;

struct VkPointLight
{
    float4 position;    // xyz - position, w - inner attenuation
    float4 radiance;
};

struct VkSpotLight
{
    float4 position;    // xyz - position, w - inner attenuation
    float4 direction;   // xyz - direction, w - outer attenuations
    float4 radiance;
};

struct VkDirectionalLight
{
    float4 direction;
    float4 radiance;
};

struct VkCamera
{
    float4    position;

    matrix    view_proj;
    matrix    view;
    matrix    inv_view;
    matrix    inv_proj;
};

struct VkDeferredPushConstants
{
    int         num_lights[4];
    float4      cascade_splits;
};