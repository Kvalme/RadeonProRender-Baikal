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
#pragma once

#include "math/int2.h"

#include "SceneGraph/vkwscene.h"

#include "VKW.h"

#include <memory>

namespace Baikal
{
    class Scene1;
    class Collector;
    class Light;
    class DirectionalLight;
    class PerspectiveCamera;

    ///< Probe controller implementation
    class VkwProbeController
    {
    public:
        VkwProbeController(VkDevice device, vkw::MemoryManager& memory_manager,
            vkw::ShaderManager& shader_manager,
            vkw::RenderTargetManager& render_target_manager,
            vkw::PipelineManager& pipeline_manager,
            vkw::ExecutionManager& execution_manager,
            uint32_t graphics_queue_index,
            uint32_t compute_queue_index);

        ~VkwProbeController() = default;

        void PrefilterEnvMap(VkwScene& out);
    protected:
        const uint32_t                                  env_map_size = 1024;

        VkDevice                                        device_;
        vkw::MemoryManager&                             memory_manager_;
        vkw::RenderTargetManager&                       render_target_manager_;
        vkw::ShaderManager&                             shader_manager_;
        vkw::PipelineManager&                           pipeline_manager_;
        vkw::ExecutionManager&                          execution_manager_;

        vkw::Utils                                      utils_;

        std::unique_ptr<vkw::CommandBufferBuilder>      command_buffer_builder_;
        std::vector<vkw::VkScopedObject<VkBuffer>>      sh9_buffers_;
        std::vector<vkw::VkScopedObject<VkSemaphore>>   sh9_semaphores_;

        vkw::Texture                                    env_cube_map_;

        vkw::VkScopedObject<VkSampler>                  nearest_sampler_;
        vkw::VkScopedObject<VkSampler>                  linear_sampler_;
        vkw::VkScopedObject<VkSampler>                  linear_sampler_cube_map_;

        vkw::Shader                                     convert_to_cubemap_shader_;
        vkw::ComputePipeline                            convert_to_cubemap_pipeline_;

        vkw::Shader                                     generate_cubemap_mips_shader_;
        vkw::ComputePipeline                            generate_cubemap_mips_pipeline_;

        vkw::Shader                                     generate_brdf_lut_shader_;
        vkw::ComputePipeline                            generate_brdf_lut_pipeline_;

        vkw::Shader                                     project_cubemap_to_sh9_shader_;
        vkw::ComputePipeline                            project_cubemap_to_sh9_pipeline_;

        vkw::Shader                                     prefilter_reflections_shader_;
        vkw::ComputePipeline                            prefilter_reflections_pipeline_;

        vkw::Shader                                     downsample_sh9_shader_;
        vkw::ComputePipeline                            downsample_sh9_pipeline_;


        vkw::Shader                                     final_sh9_shader_;
        vkw::ComputePipeline                            final_sh9_pipeline_;

        VkViewport                                      viewport_;
        VkRect2D                                        scissor_;

        uint32_t                                        graphics_queue_index_;
        uint32_t                                        compute_queue_index_;

        VkQueue                                         graphics_queue_;
        VkQueue                                         compute_queue_;
    };
}