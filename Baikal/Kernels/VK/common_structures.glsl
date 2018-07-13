const int kPoint = 0x1;
const int kDirectional = 0x2;
const int kSpot = 0x3;
const int kIbl = 0x4;
const int kArea = 0x5;

const int kMaxTextures = 512;
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
    matrix    prev_view_proj;
    matrix    view;
    matrix    inv_view;
    matrix    inv_proj;
    matrix    inv_view_proj;

    float4    params; // x - aspect ratio, y - fov
};

struct VkMaterialConstants
{
    int data[4];

    float4 diffuse;
    float4 normal;
    float4 roughness;
    float4 metalness;
    float4 ior;
    float4 transparency;
};

struct VkDeferredPushConstants
{
    int         num_lights[4];
    float4      cascade_splits;
};

struct VkSH9Color
{
    float4 coefficients[9];
};

struct VkJitterBuffer
{
    matrix jitter;
    matrix prev_jitter;
    float4 offsets;
};