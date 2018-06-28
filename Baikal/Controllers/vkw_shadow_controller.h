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

    ///< Shadow controller implementation
    class VkwShadowController
    {
    public:
        VkwShadowController(VkDevice device, vkw::MemoryManager& memory_manager,
            vkw::ShaderManager& shader_manager,
            vkw::RenderTargetManager& render_target_manager,
            vkw::PipelineManager& pipeline_manager,
            uint32_t graphics_queue_index,
            uint32_t compute_queue_index);

        ~VkwShadowController() = default;

        void UpdateShadows(bool geometry_changed, bool camera_changed, std::vector<bool> const& lights_changed, Scene1 const& scene, VkwScene& out);
    protected:
        void BuildCommandBuffer(uint32_t shadow_map_idx, uint32_t view_proj_light_idx, VkwScene const& scene);
        void BuildDirectionalLightCommandBuffer(uint32_t shadow_map_idx, uint32_t& light_idx, VkwScene const& scene);
        void UpdateShadowMap(uint32_t shadow_pass_idx, VkwScene& out);
        void CreateShadowRenderPipeline(VkRenderPass render_pass);
        void GenerateShadowViewProjForCascadeSlice(uint32_t cascade_idx, PerspectiveCamera const& camera, DirectionalLight const& light, RadeonRays::matrix& view_proj);
    protected:
        const float                                     split_dists_[4] = { 0.01f, 0.03f, 0.1f, 0.3f };

        VkDevice                                        device_;
        vkw::MemoryManager&                             memory_manager_;
        vkw::RenderTargetManager&                       render_target_manager_;
        vkw::ShaderManager&                             shader_manager_;
        vkw::PipelineManager&                           pipeline_manager_;
        
        std::vector<vkw::RenderTargetCreateInfo>        shadow_attachments;

        vkw::Shader                                     shadowmap_shader_;
        std::unique_ptr<vkw::CommandBufferBuilder>      command_buffer_builder_;
        std::vector<vkw::CommandBuffer>                 shadowmap_cmd_;
        vkw::GraphicsPipeline                           shadowmap_pipeline_;

        vkw::VkScopedObject<VkSampler>                  nearest_sampler_;
        vkw::VkScopedObject<VkSampler>                  linear_sampler_;

        std::vector<vkw::VkScopedObject<VkSemaphore>>   shadowmap_syncs_;

        vkw::Utils                                      utils_;

        vkw::VkScopedObject<VkBuffer>                   view_proj_buffer_;

        VkViewport                                      viewport_;
        VkRect2D                                        scissor_;

        uint32_t                                        graphics_queue_index_;
        uint32_t                                        compute_queue_index_;

        uint32_t                                        shadowmap_width_;
        uint32_t                                        shadowmap_height_;

        VkQueue                                         graphics_queue_;
        VkQueue                                         compute_queue_;
    };
}
