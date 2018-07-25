
/**********************************************************************
 Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ********************************************************************/
#include "Application/render.h"

#include "scene_io.h"
#include "material_io.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "math/mathutils.h"

#include "RadeonProRender_VK.h"
#include "ProRenderGLTF.h"

namespace BaikalRPR
{
#define CHECK(x) if ((x) != RPR_SUCCESS) assert(false);

    rpr_shape AppRender::AddSphere(std::string const& name, std::uint32_t lat, std::uint32_t lon, float r, RadeonRays::float3 const& c)
    {
        size_t num_verts = (lat - 2) * lon + 2;
        size_t num_tris = (lat - 2) * (lon - 1) * 2;

        std::vector<RadeonRays::float3> vertices(num_verts);
        std::vector<RadeonRays::float3> normals(num_verts);
        std::vector<RadeonRays::float2> uvs(num_verts);
        std::vector<std::uint32_t> indices(num_tris * 3);

        auto t = 0U;
        for (auto j = 1U; j < lat - 1; j++)
        {
            for (auto i = 0U; i < lon; i++)
            {
                float theta = float(j) / (lat - 1) * (float)M_PI;
                float phi = float(i) / (lon - 1) * (float)M_PI * 2;
                vertices[t].x = r * sinf(theta) * cosf(phi) + c.x;
                vertices[t].y = r * cosf(theta) + c.y;
                vertices[t].z = r * -sinf(theta) * sinf(phi) + c.z;
                normals[t].x = sinf(theta) * cosf(phi);
                normals[t].y = cosf(theta);
                normals[t].z = -sinf(theta) * sinf(phi);
                uvs[t].x = phi / (2 * (float)M_PI);
                uvs[t].y = theta / ((float)M_PI);
                ++t;
            }
        }

        vertices[t].x = c.x; vertices[t].y = c.y + r; vertices[t].z = c.z;
        normals[t].x = 0; normals[t].y = 1; normals[t].z = 0;
        uvs[t].x = 0; uvs[t].y = 0;
        ++t;
        vertices[t].x = c.x; vertices[t].y = c.y - r; vertices[t].z = c.z;
        normals[t].x = 0; normals[t].y = -1; normals[t].z = 0;
        uvs[t].x = 1; uvs[t].y = 1;
        ++t;

        t = 0U;
        for (auto j = 0U; j < lat - 3; j++)
        {
            for (auto i = 0U; i < lon - 1; i++)
            {
                indices[t++] = j * lon + i;
                indices[t++] = (j + 1) * lon + i + 1;
                indices[t++] = j * lon + i + 1;
                indices[t++] = j * lon + i;
                indices[t++] = (j + 1) * lon + i;
                indices[t++] = (j + 1) * lon + i + 1;
            }
        }

        for (auto i = 0U; i < lon - 1; i++)
        {
            indices[t++] = (lat - 2) * lon;
            indices[t++] = i;
            indices[t++] = i + 1;
            indices[t++] = (lat - 2) * lon + 1;
            indices[t++] = (lat - 3) * lon + i + 1;
            indices[t++] = (lat - 3) * lon + i;
        }

        std::vector<int> faces(indices.size() / 3, 3);

        rpr_shape sphere = nullptr;
        CHECK(rprContextCreateMesh(m_context,
                                       (rpr_float const*)vertices.data(), vertices.size(), sizeof(RadeonRays::float3),
                                       (rpr_float const*)normals.data(), normals.size(), sizeof(RadeonRays::float3),
                                       (rpr_float const*)uvs.data(), uvs.size(), sizeof(RadeonRays::float2),
                                       (rpr_int const*)indices.data(), sizeof(rpr_int),
                                       (rpr_int const*)indices.data(), sizeof(rpr_int),
                                       (rpr_int const*)indices.data(), sizeof(rpr_int),
                                       faces.data(), faces.size(), &sphere));
        return sphere;
    }

    rpr_material_node AppRender::AddDiffuseMaterial(std::string const& name, RadeonRays::float3 color)
    {
        rpr_material_node material = nullptr;
        CHECK(rprMaterialSystemCreateNode(m_matsys, RPR_MATERIAL_NODE_UBERV2, &material));

        CHECK(rprMaterialNodeSetInputU_ext(material, RPR_UBER_MATERIAL_LAYERS, RPR_UBER_MATERIAL_LAYER_DIFFUSE));
        CHECK(rprMaterialNodeSetInputF_ext(material, RPR_UBER_MATERIAL_DIFFUSE_COLOR, color.x, color.y, color.z, 0.0f));
        return material;
    }


    void AppRender::LoadScene(AppSettings& settings)
    {
        RadeonRays::rand_init();
        std::string fname = settings.path + "/" + settings.modelname;
        std::cout<<"Loading "<<fname<<std::endl;

        CHECK(rprContextCreateScene(m_context, &m_scene));
        CHECK(rprContextSetScene(m_context, m_scene));
        CHECK(rprContextCreateCamera(m_context, &m_camera));

        CHECK(rprCameraSetMode(m_camera, RPR_CAMERA_MODE_PERSPECTIVE));
        // Set default sensor size 36x36 mm because we're rendering to square viewport
        // Adjust sensor size based on current aspect ratio
        float aspect = (float)settings.width / settings.height;
        settings.camera_sensor_size.y = settings.camera_sensor_size.x / aspect;

/*        m_sphere = AddSphere("sphere", 64, 32, 2.0f, RadeonRays::float3(0.0f, 0.0f, 0.0f));
        rprSceneAttachShape(m_scene, m_sphere);

        m_material = AddDiffuseMaterial("sphere_mtl", RadeonRays::float3(0.8f, 0.8f, 0.8f));
        CHECK(rprShapeSetMaterial(m_sphere, m_material));*/

        CHECK(rprImportFromGLTF(fname.c_str(), m_context, m_matsys, m_rprxcontext, &m_scene));

        CHECK(rprCameraLookAt(m_camera, -500.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
        CHECK(rprCameraSetSensorSize(m_camera, settings.camera_sensor_size.x * 1000.f, settings.camera_sensor_size.y * 1000.f));
        CHECK(rprSceneSetCamera(m_scene, m_camera));

        rpr_light light = nullptr;
        CHECK(rprContextCreatePointLight(m_context, &light));
        RadeonRays::matrix lightm = translation(RadeonRays::float3(3.0f, 6.0f, 0.0f));
        CHECK(rprLightSetTransform(light, true, &lightm.m00));
        CHECK(rprPointLightSetRadiantPower3f(light, 300.0f, 200.5f, 200.5f));
        CHECK(rprSceneAttachLight(m_scene, light));
    }
}
