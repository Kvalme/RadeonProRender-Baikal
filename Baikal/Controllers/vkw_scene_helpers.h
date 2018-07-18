#pragma once

#include "math/matrix.h"

namespace Baikal
{
    class Light;
    class Camera;
    class PerspectiveCamera;

    int GetLightType(Light const& light);

    RadeonRays::matrix PerspectiveProjFovyRhVulkan(float fovy, float aspect, float n, float f);
    RadeonRays::matrix MakeViewMatrix(Camera const& camera);
    RadeonRays::matrix MakeProjectionMatrix(PerspectiveCamera const& camera);
}