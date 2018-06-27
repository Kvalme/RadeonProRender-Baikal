#include "vkw_scene_helpers.h"

#include "SceneGraph/vkwscene.h"
#include "SceneGraph/light.h"
#include "SceneGraph/camera.h"

namespace Baikal
{
    int GetLightType(Light const& light)
    {
        if (dynamic_cast<PointLight const*>(&light))
        {
            return kPoint;
        }
        else if (dynamic_cast<DirectionalLight const*>(&light))
        {
            return kDirectional;
        }
        else if (dynamic_cast<SpotLight const*>(&light))
        {
            return kSpot;
        }
        else if (dynamic_cast<ImageBasedLight const*>(&light))
        {
            return kIbl;
        }
        else
        {
            return kArea;
        }
    }

    RadeonRays::matrix MakeViewMatrix(Camera const& camera)
    {
        const float3 up = camera.GetUpVector();
        const float3 d = -camera.GetForwardVector();
        const float3 p = camera.GetPosition();

        return lookat_lh_dx(p, p + d, up);
    }

    RadeonRays::matrix MakeProjectionMatrix(PerspectiveCamera const& camera)
    {
        const float focal_length = camera.GetFocalLength();
        const float2 sensor_size = camera.GetSensorSize();

        float2 z_range = camera.GetDepthRange();

        // Nan-avoidance in perspective matrix
        z_range.x = std::max(z_range.x, std::numeric_limits<float>::epsilon());

        const float fovy = atan(sensor_size.y / (2.0f * focal_length));

        return perspective_proj_fovy_rh_vulkan(fovy, camera.GetAspectRatio(), z_range.x, z_range.y);
    }
}