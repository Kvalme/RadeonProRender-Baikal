#include "hybrid_renderer.h"

namespace Baikal
{
    HybridRenderer::HybridRenderer(VkDevice device, vkw::MemoryManager& memory_manager,
        vkw::ShaderManager& shader_manager,
        vkw::RenderTargetManager& render_target_manager,
        vkw::PipelineManager& pipeline_manager,
        uint32_t graphics_queue_index,
        uint32_t compute_queue_index)
        : device_(device)
        , memory_manager_(memory_manager)
        , render_target_manager_(render_target_manager)
        , shader_manager_(shader_manager)
        , pipeline_manager_(pipeline_manager)
        , utils_(device_)
        , graphics_queue_index_(graphics_queue_index)
        , compute_queue_index_(compute_queue_index)
        , framebuffer_width_(0)
        , framebuffer_height_(0)
    {
        command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, graphics_queue_index_));

        InitializeResources();

        nearest_sampler_ = utils_.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        gbuffer_signal_ = utils_.CreateSemaphore();

        vkGetDeviceQueue(device_, graphics_queue_index, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, compute_queue_index, 0, &compute_queue_);
    }

    void HybridRenderer::Clear(RadeonRays::float3 const& val,
        Output& output) const
    {

    }

    void HybridRenderer::BuildDeferredCommandBuffer(VkwOutput const& output)
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 1.0f }
        };

        VkDeviceSize offsets[1] = { 0 };

        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer2 = command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer2, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_pipeline_.pipeline.get());

        command_buffer_builder_->BeginRenderPass(clear_values, output.GetRenderTarget());

        vkCmdSetViewport(command_buffer2, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer2, 0, 1, &scissor_);

        VkDescriptorSet desc_set = deferred_frag_shader_.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer2, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer2, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer2, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer2, 6, 1, 0, 0, 0);

        command_buffer_builder_->EndRenderPass();

        deferred_cmd_ = command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildGbufferCommandBuffer(VkwScene const& scene)
    {
        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = command_buffer_builder_->GetCurrentCommandBuffer();

        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 1.0f },
            { 0.f, 0.f, 0.f, 1.0f },
            { 0.f, 0.f, 0.f, 1.0f },
            { 1.0f, 0.f }
        };

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.pipeline.get());

        VkDeviceSize offsets[1] = { 0 };

        VkBuffer vb = scene.mesh_vertex_buffer.get();
        command_buffer_builder_->BeginRenderPass(clear_values, g_buffer_);

        VkDescriptorSet desc_set = mrt_vert_shader_.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, scene.mesh_index_buffer.get(), 0, VK_INDEX_TYPE_UINT32);

        for (auto mesh : scene.meshes)
        {
            vkCmdDrawIndexed(command_buffer, mesh.index_count, 1, mesh.index_base, 0, 0);
        }

        command_buffer_builder_->EndRenderPass();
        g_buffer_cmd_ = command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::DrawDeferredPass(VkwOutput const& output)
    {
        VkPipelineStageFlags stage_wait_bits = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkCommandBuffer deferred_cmd_buf = deferred_cmd_.get();

        VkSemaphore g_buffer_finised = gbuffer_signal_.get();
        VkSemaphore render_finished = output.GetSemaphore();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &render_finished;
        submit_info.signalSemaphoreCount = 1;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &g_buffer_finised;
        submit_info.pWaitDstStageMask = &stage_wait_bits;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &deferred_cmd_buf;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::DrawGbufferPass()
    {
        VkCommandBuffer g_buffer_cmd = g_buffer_cmd_.get();
        VkSemaphore g_buffer_finised = gbuffer_signal_.get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &g_buffer_finised;
        submit_info.signalSemaphoreCount = 1;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &g_buffer_cmd;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::Render(VkwScene const& scene)
    {
        Output* output = GetOutput(OutputType::kColor);

        if (output == nullptr)
            throw std::runtime_error("HybridRenderer: output buffer is not set");

        VkwOutput* vk_output = dynamic_cast<VkwOutput*>(output);

        if (vk_output == nullptr)
            throw std::runtime_error("HybridRenderer: Internal error");

        mrt_vert_shader_.SetArg(0, scene.camera.get());
        mrt_vert_shader_.CommitArgs();

        if (scene.rebuild_cmd_buffers_)
        {
            BuildGbufferCommandBuffer(scene);
            scene.rebuild_cmd_buffers_ = false;
        }

        DrawGbufferPass();
        DrawDeferredPass(*vk_output);
    }

    void HybridRenderer::RenderTile(VkwScene const& scene,
        RadeonRays::int2 const& tile_origin,
        RadeonRays::int2 const& tile_size)
    {

    }

    void HybridRenderer::SetOutput(OutputType type, Output* output)
    {
        Renderer::SetOutput(type, output);

        if (output != nullptr)
        {
            if (framebuffer_width_ != output->width() || framebuffer_height_ != output->height())
            {
                ResizeRenderTargets(output->width(), output->height());

                framebuffer_width_ = output->width();
                framebuffer_height_ = output->height();

                viewport_ = vkw::Utils::CreateViewport(
                    static_cast<float>(output->width()),
                    static_cast<float>(output->height()),
                    0.f, 1.f);

                scissor_ =
                {
                    {0, 0},
                    { output->width(), output->height() }
                };
            }

            if (type == OutputType::kColor)
            {
                VkwOutput* vk_output = dynamic_cast<VkwOutput*>(output);

                if (vk_output == nullptr)
                    throw std::runtime_error("HybridRenderer: Internal error");

                deferred_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(fsq_vert_shader_, deferred_frag_shader_, vk_output->GetRenderTarget().render_pass.get());

                deferred_frag_shader_.SetArg(1, g_buffer_.attachments[1].view.get(), nearest_sampler_.get());
                deferred_frag_shader_.CommitArgs();

                BuildDeferredCommandBuffer(*vk_output);
            }
        }
    }

    void HybridRenderer::SetRandomSeed(std::uint32_t seed)
    {

    }

    void HybridRenderer::InitializeResources()
    {
        struct Vertex
        {
            float position[4];
        };

        Vertex vertices[4] =
        {
            { -1.0f, 1.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 0.0f, 1.0f },
            { 1.0f, -1.0f, 0.0f, 1.0f },
            { -1.0f, -1.0f, 0.0f, 1.0f }
        };

        uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

        fullscreen_quad_vb_ = memory_manager_.CreateBuffer(4 * sizeof(Vertex),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertices);

        fullscreen_quad_ib_ = memory_manager_.CreateBuffer(6 * sizeof(uint32_t),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indices);

        mrt_vert_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_VERTEX_BIT, "../Baikal/Kernels/VK/mrt.vert.spv");
        mrt_frag_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../Baikal/Kernels/VK/mrt.frag.spv");

        fsq_vert_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_VERTEX_BIT, "../Baikal/Kernels/VK/fullscreen_quad.vert.spv");
        deferred_frag_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../Baikal/Kernels/VK/deferred.frag.spv");
    }

    void HybridRenderer::SetMaxBounces(std::uint32_t max_bounces)
    {

    }

    void HybridRenderer::ResizeRenderTargets(uint32_t width, uint32_t height)
    {
        static std::vector<vkw::RenderTargetCreateInfo> attachments = {
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
            { width, height, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
        };

        std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;

        for (size_t i = 0; i < attachments.size(); i++)
        {
            if ((attachments[i].usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
                continue;

            VkPipelineColorBlendAttachmentState blend_attachment_state = {};
            blend_attachment_state.colorWriteMask = 0xF;
            blend_attachment_state.blendEnable = false;

            blend_attachment_states.push_back(blend_attachment_state);
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state = {};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.pNext = nullptr;
        color_blend_state.attachmentCount = static_cast<uint32_t>(blend_attachment_states.size());
        color_blend_state.pAttachments = blend_attachment_states.data();

        VkPipelineRasterizationStateCreateInfo rasterization_state = {};
        rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state.pNext = nullptr;
        rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization_state.flags = 0;
        rasterization_state.depthClampEnable = VK_FALSE;
        rasterization_state.lineWidth = 1.0f;

        vkw::GraphicsPipelineState pipeline_state;
        pipeline_state.color_blend_state = &color_blend_state;
        pipeline_state.rasterization_state = &rasterization_state;

        g_buffer_ = render_target_manager_.CreateRenderTarget(attachments);
        mrt_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(mrt_vert_shader_, mrt_frag_shader_, g_buffer_.render_pass.get(), &pipeline_state);
    }
}