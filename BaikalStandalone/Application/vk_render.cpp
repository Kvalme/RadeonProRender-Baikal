/**********************************************************************
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

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
#include "OpenImageIO/imageio.h"

#include "Application/vk_render.h"
#include "Application/gl_render.h"

#include "SceneGraph/scene1.h"
#include "SceneGraph/camera.h"
#include "SceneGraph/material.h"
#include "SceneGraph/IO/scene_io.h"
#include "SceneGraph/IO/material_io.h"
#include "SceneGraph/material.h"

#include "Renderers/monte_carlo_renderer.h"
#include "Renderers/adaptive_renderer.h"

#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#include "PostEffects/wavelet_denoiser.h"
#include "Utils/clw_class.h"

namespace Baikal
{
    AppVkRender::AppVkRender(AppSettings& settings, GLuint tex) : m_tex(tex), m_output_type(OutputType::kColor)
    {
        InitVk(settings, m_tex);
        LoadScene(settings);

        
        m_output = m_cfgs[0].factory->CreateOutput(1920, 1080);
        m_primary = 0;
    }

    void AppVkRender::InitVk(AppSettings& settings, GLuint tex)
    {
        bool force_disable_itnerop = false;
        //create cl context
        try
        {
            ConfigManager::CreateConfigs(
                settings.mode,
                settings.interop,
                m_cfgs,
                settings.num_bounces,
                settings.platform_index,
                settings.device_index);
        }
        catch (CLWException &)
        {
            force_disable_itnerop = true;
            ConfigManager::CreateConfigs(settings.mode, false, m_cfgs, settings.num_bounces);
        }

        m_width = (std::uint32_t)settings.width;
        m_height = (std::uint32_t)settings.height;

        settings.interop = false;

        m_outputs.resize(m_cfgs.size());
        m_ctrl.reset(new ControlData[m_cfgs.size()]);
    }


    void AppVkRender::LoadScene(AppSettings& settings)
    {
        rand_init();

        // Load file
        std::string basepath = settings.path;
        basepath += "/";
        std::string filename = basepath + settings.modelname;

        {
            // Load OBJ scene
            bool is_fbx = filename.find(".fbx") != std::string::npos;
            bool is_gltf = filename.find(".gltf") != std::string::npos;
            std::unique_ptr<Baikal::SceneIo> scene_io;
            if (is_gltf)
            {
                assert(!"glTF loading not supported");
            }
            else if(is_fbx)
            {
                assert(!"FBX loading not supported");
            }
            else
            {
                scene_io = Baikal::SceneIo::CreateSceneIoObj();
            }
            auto scene_io1 = Baikal::SceneIo::CreateSceneIoTest();
            m_scene = scene_io->LoadScene(filename, basepath);

            // Check it we have material remapping
            std::ifstream in_materials(basepath + "materials.xml");
            std::ifstream in_mapping(basepath + "mapping.xml");

            if (in_materials && in_mapping)
            {
                in_materials.close();
                in_mapping.close();

                auto material_io = Baikal::MaterialIo::CreateMaterialIoXML();
                auto mats = material_io->LoadMaterials(basepath + "materials.xml");
                auto mapping = material_io->LoadMaterialMapping(basepath + "mapping.xml");

                material_io->ReplaceSceneMaterials(*m_scene, *mats, mapping);
            }
        }

        switch (settings.camera_type)
        {
        case CameraType::kPerspective:
            m_camera = Baikal::PerspectiveCamera::Create(
                settings.camera_pos
                , settings.camera_at
                , settings.camera_up);

            break;
        case CameraType::kOrthographic:
            m_camera = Baikal::OrthographicCamera::Create(
                settings.camera_pos
                , settings.camera_at
                , settings.camera_up);
            break;
        default:
            throw std::runtime_error("AppClRender::InitCl(...): unsupported camera type");
        }

        m_scene->SetCamera(m_camera);

        // Adjust sensor size based on current aspect ratio
        float aspect = (float)settings.width / settings.height;
        settings.camera_sensor_size.y = settings.camera_sensor_size.x / aspect;

        m_camera->SetSensorSize(settings.camera_sensor_size);
        m_camera->SetDepthRange(settings.camera_zcap);

        auto perspective_camera = std::dynamic_pointer_cast<Baikal::PerspectiveCamera>(m_camera);

        // if camera mode is kPerspective
        if (perspective_camera)
        {
            perspective_camera->SetFocalLength(settings.camera_focal_length);
            perspective_camera->SetFocusDistance(settings.camera_focus_distance);
            perspective_camera->SetAperture(settings.camera_aperture);
            std::cout << "Camera type: " << (perspective_camera->GetAperture() > 0.f ? "Physical" : "Pinhole") << "\n";
            std::cout << "Lens focal length: " << perspective_camera->GetFocalLength() * 1000.f << "mm\n";
            std::cout << "Lens focus distance: " << perspective_camera->GetFocusDistance() << "m\n";
            std::cout << "F-Stop: " << 1.f / (perspective_camera->GetAperture() * 10.f) << "\n";
        }

        std::cout << "Sensor size: " << settings.camera_sensor_size.x * 1000.f << "x" << settings.camera_sensor_size.y * 1000.f << "mm\n";
    }

    void AppVkRender::UpdateScene()
    {
        for (int i = 0; i < m_cfgs.size(); ++i)
        {
            if (i == m_primary)
            {
                m_cfgs[i].controller->CompileScene(m_scene);
                m_cfgs[i].renderer->Clear(float3(0, 0, 0), *m_outputs[i].output);
            }
            else
                m_ctrl[i].clear.store(true);
        }
    }

    void AppVkRender::Update(AppSettings& settings)
    {
        if (!settings.interop)
        {
        }
    }

    void AppVkRender::Render(int sample_cnt)
    {
        auto& scene = m_cfgs[m_primary].controller->GetCachedScene(m_scene);
        m_cfgs[m_primary].renderer->Render(scene);

        if (m_shape_id_requested)
        {
            // offset in OpenCl memory till necessary item
            auto offset = (std::uint32_t)(m_width * (m_height - m_shape_id_pos.y) + m_shape_id_pos.x);
            // copy shape id elem from OpenCl
            float4 shape_id;
            m_shape_id_data.output->GetData((float3*)&shape_id, offset, 1);
            m_promise.set_value(shape_id.x);
            // clear output to stop tracking shape id map in openCl
            m_cfgs[m_primary].renderer->SetOutput(OutputType::kShapeId, nullptr);
            m_shape_id_requested = false;
        }
    }

    void AppVkRender::SaveFrameBuffer(AppSettings& settings)
    {
    }

    void AppVkRender::SaveImage(const std::string& name, int width, int height, const RadeonRays::float3* data)
    {
        OIIO_NAMESPACE_USING;

        std::vector<float3> tempbuf(width * height);
        tempbuf.assign(data, data + width * height);

        for (auto y = 0; y < height; ++y)
            for (auto x = 0; x < width; ++x)
            {

                float3 val = data[(height - 1 - y) * width + x];
                tempbuf[y * width + x] = (1.f / val.w) * val;

                tempbuf[y * width + x].x = std::pow(tempbuf[y * width + x].x, 1.f / 2.2f);
                tempbuf[y * width + x].y = std::pow(tempbuf[y * width + x].y, 1.f / 2.2f);
                tempbuf[y * width + x].z = std::pow(tempbuf[y * width + x].z, 1.f / 2.2f);
            }

        ImageOutput* out = ImageOutput::create(name);

        if (!out)
        {
            throw std::runtime_error("Can't create image file on disk");
        }

        ImageSpec spec(width, height, 3, TypeDesc::FLOAT);

        out->open(name, spec);
        out->write_image(TypeDesc::FLOAT, &tempbuf[0], sizeof(float3));
        out->close();
    }

    void AppVkRender::RenderThread(ControlData& cd)
    {
    }

    void AppVkRender::StartRenderThreads()
    {
        /*for (int i = 0; i < m_cfgs.size(); ++i)
        {
            if (i != m_primary)
            {
                m_renderthreads.push_back(std::thread(&AppClRender::RenderThread, this, std::ref(m_ctrl[i])));
                m_renderthreads.back().detach();
            }
        }

        std::cout << m_cfgs.size() << " OpenCL submission threads started\n";
        */
    }

    void AppVkRender::StopRenderThreads()
    {
        for (int i = 0; i < m_cfgs.size(); ++i)
        {
            if (i == m_primary)
                continue;

            m_ctrl[i].stop.store(true);
        }
    }

    void AppVkRender::RunBenchmark(AppSettings& settings)
    {
        std::cout << "Running general benchmark...\n";
    }

    void AppVkRender::SetNumBounces(int num_bounces)
    {
    }

    void AppVkRender::SetOutputType(OutputType type)
    {
        for (int i = 0; i < m_cfgs.size(); ++i)
        {
            m_cfgs[i].renderer->SetOutput(m_output_type, nullptr);
            m_cfgs[i].renderer->SetOutput(type, m_outputs[i].output.get());
        }
        m_output_type = type;
    }


    std::future<int> AppVkRender::GetShapeId(std::uint32_t x, std::uint32_t y)
    {
        m_promise = std::promise<int>();
        if (x >= m_width || y >= m_height)
            throw std::logic_error(
                "AppClRender::GetShapeId(...): x or y cords beyond the size of image");

        if (m_cfgs.empty())
            throw std::runtime_error("AppVkRender::GetShapeId(...): config vector is empty");

        // enable aov shape id output from OpenCl
        m_cfgs[m_primary].renderer->SetOutput(
            OutputType::kShapeId, m_shape_id_data.output.get());
        m_shape_id_pos = RadeonRays::float2((float)x, (float)y);
        // request shape id from render
        m_shape_id_requested = true;
        return m_promise.get_future();
    }

    Baikal::Shape::Ptr AppVkRender::GetShapeById(int shape_id)
    {
        if (shape_id < 0)
            return nullptr;

        // find shape in scene by its id
        for (auto iter = m_scene->CreateShapeIterator(); iter->IsValid(); iter->Next())
        {
            auto shape = iter->ItemAs<Shape>();
            if (shape->GetId() == shape_id)
                return shape;
        }
        return nullptr;
    }
} // Baikal
