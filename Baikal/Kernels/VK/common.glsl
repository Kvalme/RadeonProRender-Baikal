const int kPoint = 0x1;
const int kDirectional = 0x2;
const int kSpot = 0x3;
const int kIbl = 0x4;
const int kArea = 0x5;

struct VkLight
{
    float4 position;
    float4 direction;
    float4 radiance;
};

struct VkCamera
{
    float4    camera_position;

    matrix    view_projection;

    matrix    inv_view;
    matrix    inv_projection;
};

// Push constants
struct VkMaterialConstants
{
    int      data[4];   // x - mat_id, xyz - reserved
    float4   diffuse;
    float4   metallic;
    float4   roughness;
};