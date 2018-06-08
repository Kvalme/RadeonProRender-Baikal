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
#include "renderer.h"

#include "SceneGraph/vkwscene.h"
#include "Controllers/vkw_scene_controller.h"

#include "VKW.h"

#include <memory>

#include "Output/vkwoutput.h"

namespace Baikal
{
    ///< Hybrid renderer implementation
    class HybridRenderer : public Renderer<VkwScene>
    {
    public:

        HybridRenderer(VkDevice device, vkw::MemoryManager& memory_manager,
            vkw::ShaderManager& shader_manager,
            vkw::RenderTargetManager& render_target_manager,
            vkw::PipelineManager& pipeline_manager,
            uint32_t graphics_queue_index,
            uint32_t compute_queue_index);

        ~HybridRenderer() = default;

        // Renderer overrides
        void Clear(RadeonRays::float3 const& val,
            Output& output) const override;

        // Render the scene into the output
        void Render(VkwScene const& scene) override;

        // Render single tile
        void RenderTile(VkwScene const& scene,
            RadeonRays::int2 const& tile_origin,
            RadeonRays::int2 const& tile_size) override;

        // Set output
        void SetOutput(OutputType type, Output* output) override;

        void SetRandomSeed(std::uint32_t seed) override;

        // Set max number of light bounces
        void SetMaxBounces(std::uint32_t max_bounces);
    protected:
        void InitializeResources();
        void ResizeRenderTargets(uint32_t width, uint32_t height);
        void BuildDeferredCommandBuffer(VkwOutput const& output);
        void BuildGbufferCommandBuffer(VkwScene const& scene);

        void DrawGbufferPass();
        void DrawDeferredPass(VkwOutput const& output);

    protected:
        vkw::VkScopedObject<VkBuffer>					fullscreen_quad_vb_;
        vkw::VkScopedObject<VkBuffer>					fullscreen_quad_ib_;

        vkw::Shader										mrt_vert_shader_;
        vkw::Shader										mrt_frag_shader_;
        vkw::GraphicsPipeline							mrt_pipeline_;

        vkw::Shader                                     fsq_vert_shader_;
        vkw::Shader                                     deferred_frag_shader_;
        vkw::GraphicsPipeline                           deferred_pipeline_;

        static const uint32_t                           num_queued_frames_ = 2;
    protected:
        vkw::MemoryManager&                             memory_manager_;
        vkw::RenderTargetManager&                       render_target_manager_;
        vkw::ShaderManager&                             shader_manager_;
        vkw::PipelineManager&                           pipeline_manager_;

        vkw::RenderTarget                               g_buffer_;

        std::unique_ptr<vkw::CommandBufferBuilder>      command_buffer_builder_;

        VkDevice                                        device_;

        vkw::VkScopedObject<VkSampler>                  nearest_sampler_;
        vkw::VkScopedObject<VkSemaphore>                gbuffer_signal_;

        vkw::Utils                                      utils_;

        vkw::CommandBuffer                              g_buffer_cmd_[num_queued_frames_];
        vkw::CommandBuffer                              deferred_cmd_[num_queued_frames_];

        VkViewport                                      viewport_;
        VkRect2D                                        scissor_;

        uint32_t                                        graphics_queue_index_;
        uint32_t                                        compute_queue_index_;

        uint32_t                                        framebuffer_width_;
        uint32_t                                        framebuffer_height_;

        VkQueue                                         graphics_queue_;
        VkQueue                                         compute_queue_;

        uint32_t                                        frame_idx_;
    };
}
