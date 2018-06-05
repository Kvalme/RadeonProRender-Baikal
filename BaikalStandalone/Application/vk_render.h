
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
#pragma once

#include "Application/render.h"
#include "Output/output.h"
#include "Output/vkwoutput.h"
#include "SceneGraph/vkwscene.h"
#include "Utils/vk_config_manager.h"
#include "math/float3.h"
#include "app_utils.h"

#include <memory>
#include <vector>

namespace Baikal
{
    class AppVkRender : public AppRender
    {
        struct OutputData
        {
            std::unique_ptr<Baikal::Output> output;
            std::vector<RadeonRays::float3> fdata;
            std::vector<unsigned char> udata;
        };

    public:
        AppVkRender(AppSettings& settings);
        //copy data from to GL
        void Update(AppSettings& settings);

        //compile scene
        void UpdateScene();
        //render
        void Render(int sample_cnt);
        void StartRenderThreads();
        void StopRenderThreads();
        void RunBenchmark(AppSettings& settings);

        //save vk frame buffer to file
        void SaveFrameBuffer(AppSettings& settings);
        void SaveImage(const std::string& name, int width, int height, const RadeonRays::float3* data);

        inline OutputType GetOutputType() { return m_output_type; };

        void SetNumBounces(int num_bounces);
        void SetOutputType(OutputType type);

        VkDevice            GetDevice() { return m_cfg.device_.get(); }
        VkInstance          GetInstance() { return m_cfg.instance_.get(); }
        VkPhysicalDevice    GetPhysicalDevice() { return m_cfg.physical_device_; }
        uint32_t            GetGraphicsQueueFamilyIndex() { return m_cfg.graphics_queue_family_idx_; }
        Output*             GetRendererOutput() { return m_output.output.get(); }
        
        vkw::MemoryManager& GetMemoryManager() { return *m_cfg.memory_manager_; }
        vkw::ShaderManager& GetShaderManager() { return *m_cfg.shader_manager_; }
        vkw::RenderTargetManager& GetRenderTargetManager() { return *m_cfg.render_target_manager_; }
        vkw::PipelineManager& GetPipelineManager() { return *m_cfg.pipeline_manager_; }
        vkw::Utils& GetUtils() { return *m_cfg.utils_; }

        std::future<int> GetShapeId(std::uint32_t x, std::uint32_t y);
        Baikal::Shape::Ptr GetShapeById(int shape_id);
    private:
        void InitVk(AppSettings& settings);

        VkConfigManager::VkConfig m_cfg;

        OutputType m_output_type;
        OutputData m_output;
        std::promise<int> m_promise;
    };
}
