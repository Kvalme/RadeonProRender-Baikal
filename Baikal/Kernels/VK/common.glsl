const int kPoint = 0x1;
const int kDirectional = 0x2;
const int kSpot = 0x3;
const int kIbl = 0x4;
const int kArea = 0x5;

const int kMaxLights = 4;

struct VkLight
{
    // w - type
    // xyz - position for spot and point lights
    // x - texture index for image based light
    float4 data0;

    // xyz - direction for spot and directional lights
    // w - inner attenuation for spot light
    float4 data1;

    // xyz - radiance for analytical types of lights
    // w - outer attenuation for spot light
    float4 data2;
};

struct VkCamera
{
    float4    camera_position;

    matrix    view_proj;
    matrix    inv_view;
    matrix    inv_proj;
    matrix    proj;
};

struct VkDeferredPushConstants
{
    int num_lights;
};