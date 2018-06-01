#include "hybrid_renderer.h"

#include "Output/output.h"
#include "Output/vkwoutput.h"

namespace Baikal
{
    HybridRenderer::HybridRenderer(VkDevice device, vkw::MemoryManager& memory_manager, vkw::RenderTargetManager& render_target_manager)
    : device_(device)
    , memory_manager_(memory_manager)
    , render_target_manager_(render_target_manager)
    {
        uint32_t graphics_family_idx = 0;

        command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, graphics_family_idx));
        pipeline_manager_.reset(new vkw::PipelineManager(device));
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

        vkw::RenderTarget& output_rt =  vk_output->GetRenderTarget();

        command_buffer_builder_->BeginCommandBuffer();

        std::vector<VkClearValue> clear_values = {
            { 0.0f, 0.0f, 0.0f, 0.0f } // color
        };

        command_buffer_builder_->BeginRenderPass(clear_values, output_rt);
        command_buffer_builder_->EndRenderPass();

        vkw::CommandBuffer cmd_buffer = command_buffer_builder_->EndCommandBuffer();
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
    }
}