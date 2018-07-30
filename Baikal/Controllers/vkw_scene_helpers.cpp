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

    RadeonRays::matrix PerspectiveProjFovyRhVulkan(float l, float r, float b, float t, float n, float f)
    {
        return matrix(2 * n / (r - l), 0, 0, 0,
        0, 2 * n / (t - b), 0, (r + l) / (r - l),
        0, (t + b) / (t - b), f / (n - f), -(f * n) / (f - n),
        0, 0, -1, 0);
    }
    
    // valid for reverse z buffer (n = f)
    RadeonRays::matrix PerspectiveInfiniteReverseZProjFovyRhVulkan(float l, float r, float b, float t, float n, float f)
    {
        return matrix(2 * n / (r - l), 0, 0, 0,
            0, 2 * n / (t - b), 0, (r + l) / (r - l),
            0, (t + b) / (t - b), 0.f, f,
            0, 0, -1, 0);
    }

    RadeonRays::matrix PerspectiveInfiniteReverseZProjFovyRhVulkan(float fovy, float aspect, float n, float f)
    {
        float hH = tan(fovy) * n;
        float hW = hH * aspect;
        return PerspectiveInfiniteReverseZProjFovyRhVulkan(-hW, hW, -hH, hH, n, f);
    }

    RadeonRays::matrix PerspectiveProjFovyRhVulkan(float fovy, float aspect, float n, float f)
    {
        float hH = tan(fovy) * n;
        float hW = hH * aspect;
        return PerspectiveProjFovyRhVulkan(-hW, hW, -hH, hH, n, f);
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
        const float fovy = atan(sensor_size.y / (2.0f * focal_length));

        float2 z_range = camera.GetDepthRange();
        // Nan-avoidance in perspective matrix
        z_range.x = std::max(z_range.x, std::numeric_limits<float>::epsilon());

        return PerspectiveInfiniteReverseZProjFovyRhVulkan(fovy, camera.GetAspectRatio(), z_range.x, z_range.y);
    }

    RadeonRays::matrix MakeProjectionMatrix(PerspectiveCamera const& camera, float near_plane, float far_plane)
    {
        const float focal_length = camera.GetFocalLength();
        const float2 sensor_size = camera.GetSensorSize();
        const float fovy = atan(sensor_size.y / (2.0f * focal_length));

        return PerspectiveProjFovyRhVulkan(fovy, camera.GetAspectRatio(), near_plane, far_plane);
    }

    // From "Hacker's Delight"
    float RadicalInverseBase2(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
    }

    RadeonRays::float2 Hammersley2D(uint64_t sample_idx, uint64_t num_samples)
    {
        return RadeonRays::float2(float(sample_idx) / float(num_samples), RadicalInverseBase2(uint32_t(sample_idx)));
    }
}
