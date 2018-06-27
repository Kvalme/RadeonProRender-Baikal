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
#include "vk_render.h"
#include "scene_io.h"
#include "material_io.h"
#include "Output/vkwoutput.h"

namespace Baikal
{
    AppVkRender::AppVkRender(AppSettings& settings)
    {
        InitVk(settings);
        LoadScene(settings);
    }

    void AppVkRender::InitVk(AppSettings& settings)
    {
        VkConfigManager::CreateConfig(m_cfg, settings.vk_required_extensions);

        m_output.output = m_cfg.factory_->CreateOutput(settings.width, settings.height);
        m_output.fdata.resize(settings.width * settings.height);
        m_output.udata.resize(settings.width * settings.height * 4);

        m_output_type = OutputType::kColor;

        SetOutputType(OutputType::kColor);
    }

    void AppVkRender::Update(AppSettings& settings)
    {
    }

    void AppVkRender::UpdateScene()
    {
        VkwScene& scene = m_cfg.controller_->CompileScene(m_scene);
        m_cfg.controller_->PostUpdate(*m_scene.get(), scene);
    }

    void AppVkRender::Render(int sample_cnt)
    {
        auto& scene = m_cfg.controller_->GetCachedScene(m_scene);
        m_cfg.renderer_->Render(scene);
    }

    void AppVkRender::StartRenderThreads()
    {

    }

    void AppVkRender::StopRenderThreads()
    {

    }

    void AppVkRender::RunBenchmark(AppSettings& settings)
    {

    }

    void AppVkRender::SetNumBounces(int num_bounces)
    {

    }

    void AppVkRender::SetOutputType(OutputType type)
    {
        m_cfg.renderer_->SetOutput(type, m_output.output.get());

        m_output_type = type;
    }

    void AppVkRender::SaveFrameBuffer(AppSettings& settings)
    {

    }

    void AppVkRender::SaveImage(const std::string& name, int width, int height, const RadeonRays::float3* data)
    {

    }

    std::future<int> AppVkRender::GetShapeId(std::uint32_t x, std::uint32_t y)
    {
        m_promise = std::promise<int>();
        return m_promise.get_future();
    }

    Baikal::Shape::Ptr AppVkRender::GetShapeById(int shape_id)
    {
        return nullptr;
    }

} // Baikal
