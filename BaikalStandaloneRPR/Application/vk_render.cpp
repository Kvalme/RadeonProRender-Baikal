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

#include "RadeonProRender_VK.h"

namespace BaikalRPR
{
    #define CHECK(x) if ((x) != RPR_SUCCESS) assert(false);

    AppVkRender::AppVkRender(AppSettings& settings) :
        m_settings(settings)
    {
        InitVk(settings);
        LoadScene(settings);

        rpr_framebuffer_desc desc = {(rpr_uint)m_settings.width, (rpr_uint)m_settings.height };
        rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

        rprContextCreateFrameBuffer(m_context, fmt, &desc, &m_framebuffer);
        rprContextSetAOV(m_context, RPR_AOV_COLOR, m_framebuffer);
    }

    void AppVkRender::InitVk(AppSettings& settings)
    {
        VkConfigManager::CreateConfig(m_cfg, settings.vk_required_extensions);
        void *params[3] = { m_cfg.instance_.get(), m_cfg.device_.get(), m_cfg.physical_device_};

        CHECK(rprCreateContext(RPR_API_VERSION, nullptr, 0, RPR_CREATION_FLAGS_ENABLE_GPU0 | RPR_CREATION_FLAGS_ENABLE_VK_INTEROP, params, nullptr, &m_context));
        CHECK(rprContextCreateMaterialSystem(m_context, 0, &m_matsys));

    }

    void AppVkRender::Update(AppSettings& settings)
    {
    }

    void AppVkRender::UpdateScene()
    {
        m_cfg.renderer_->Clear(RadeonRays::float3(0, 0, 0), *m_output.output);

        /*rpr_framebuffer_desc desc = {(rpr_uint)m_settings.width, (rpr_uint)m_settings.height };
        rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

        rprContextCreateFrameBuffer(m_context, fmt, &desc, &m_framebuffer);
        rprContextSetAOV(m_context, RPR_AOV_COLOR, m_framebuffer);*/
    }

    VkImageView AppVkRender::GetRendererImageView()
    {
        VkImageView img_view;
        rprFrameBufferGetInfo(m_framebuffer, RPR_VK_IMAGE_VIEW_OBJECT, sizeof(VkImageView), &img_view, 0);
        return img_view;
    }

    VkSemaphore AppVkRender::GetSemaphore()
    {
        VkSemaphore s;
        rprFrameBufferGetInfo(m_framebuffer, RPR_VK_SEMAPHORE_OBJECT, sizeof(VkSemaphore), &s, 0);
        return s;
    }

    void AppVkRender::Render(int sample_cnt)
    {
        //m_cfg.renderer_->Render(scene);
        rpr_int result = rprContextRender(m_context);
        assert(result == RPR_SUCCESS);
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
