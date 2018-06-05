#include "hybrid_renderer.h"

#include "Output/output.h"
#include "Output/vkwoutput.h"

namespace Baikal
{
    HybridRenderer::HybridRenderer(VkDevice device,  vkw::MemoryManager& memory_manager,
                                                     vkw::ShaderManager& shader_manager,
                                                     vkw::RenderTargetManager& render_target_manager,
                                                     vkw::PipelineManager& pipeline_manager)
    : device_(device)
    , memory_manager_(memory_manager)
    , render_target_manager_(render_target_manager)
    , shader_manager_(shader_manager)
    , pipeline_manager_(pipeline_manager)
    {
        uint32_t graphics_family_idx = 0;

        command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, graphics_family_idx));

        InitializeResources();
    }

    void HybridRenderer::Clear(RadeonRays::float3 const& val,
               Output& output) const
    {

    }

    void HybridRenderer::Render(VkwScene const& scene)
    {
        Output* output = GetOutput(OutputType::kColor);
        
        if (output == nullptr)
            throw std::runtime_error("HybridRenderer: output buffer is not set");
        
        VkwOutput* vk_output = dynamic_cast<VkwOutput*>(output);

        if (vk_output == nullptr)
            throw std::runtime_error("HybridRenderer: Internal error");

        vkw::RenderTarget const& output_rt =  vk_output->GetRenderTarget();

        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = command_buffer_builder_-> GetCurrentCommandBuffer();

        VkViewport viewport = vkw::Utils::InitializeViewport(
            static_cast<float>(vk_output->width()),
            static_cast<float>(vk_output->height()),
            0.f, 1.f);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.extent.width = vk_output->width();
        scissor.extent.height =vk_output->height();
        scissor.offset.x = 0;
        scissor.offset.y = 0;

        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        std::vector<VkClearValue> clear_values = {
            { 0.0f, 0.0f, 0.0f, 0.0f } // color
        };

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.pipeline.get());

        VkDeviceSize offsets[1] = { 0 };

        VkBuffer vb = fullscreen_quad_vb_.get();
        command_buffer_builder_->BeginRenderPass(clear_values, output_rt);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        command_buffer_builder_->EndRenderPass();
        vkw::CommandBuffer cmd_buffer = command_buffer_builder_->EndCommandBuffer();

        VkCommandBuffer command_buffers[1] = { cmd_buffer.get() };

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = nullptr;
        submit_info.signalSemaphoreCount = 0;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = command_buffers;

        uint32_t graphics_family_idx = 0;
        VkQueue graphics_queue;
        vkGetDeviceQueue(device_, graphics_family_idx, 0, &graphics_queue);

        VkResult result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

        vkQueueWaitIdle(graphics_queue);
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
            ResizeRenderTargets(output->width(), output->height());
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

        std::vector<Vertex> vertices =
        {
            { 1.0f, 1.0f, 0.0f, 1.0f },
            { -1.0f, 1.0f, 0.0f, 1.0f },
            { -1.0f, -1.0f, 0.0f, 1.0f },
            { 1.0f, -1.0f, 0.0f, 1.0f }
        };

        std::vector<uint32_t> indices = { 1, 0, 2, 3, 2, 0 };

        fullscreen_quad_vb_ = memory_manager_.CreateBuffer(vertices.size() * sizeof(Vertex),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertices.data());

        fullscreen_quad_ib_ = memory_manager_.CreateBuffer(indices.size() * sizeof(uint32_t),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indices.data());

        fsq_vert_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_VERTEX_BIT, "../Baikal/Kernels/VK/fullscreen_quad.vert.spv");
        mrt_frag_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../Baikal/Kernels/VK/mrt.frag.spv");
    }

    void HybridRenderer::SetMaxBounces(std::uint32_t max_bounces)
    {

    }

    void HybridRenderer::ResizeRenderTargets(uint32_t width, uint32_t height)
    {
        std::vector<vkw::RenderTargetCreateInfo> attachments = {
            //{ width, height, VK_FORMAT_R16G16B16A16_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
            //{ width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT }
        };

        g_buffer_ = render_target_manager_.CreateRenderTarget(attachments);
        mrt_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(fsq_vert_shader_, mrt_frag_shader_, g_buffer_.render_pass.get());
    }
}