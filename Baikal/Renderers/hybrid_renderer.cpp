#include "hybrid_renderer.h"

#include "Controllers/vkw_scene_helpers.h"

#include <chrono>

namespace Baikal
{
    HybridRenderer::HybridRenderer(VkDevice device, vkw::MemoryManager& memory_manager,
                                   vkw::ShaderManager& shader_manager,
                                   vkw::RenderTargetManager& render_target_manager,
                                   vkw::PipelineManager& pipeline_manager,
                                   uint32_t graphics_queue_index,
                                   uint32_t compute_queue_index)
            : memory_manager_(memory_manager)
            , render_target_manager_(render_target_manager)
            , shader_manager_(shader_manager)
            , pipeline_manager_(pipeline_manager)
            , device_(device)
            , utils_(device_)
            , graphics_queue_index_(graphics_queue_index)
            , compute_queue_index_(compute_queue_index)
            , framebuffer_width_(0)
            , framebuffer_height_(0)
            , inv_framebuffer_width_(0)
            , inv_framebuffer_height_(0)
            , txaa_frame_idx_(0)
            , dt_(1.0f / 60.f)
            , tonemap_output_(true)
    {
        graphics_command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, graphics_queue_index_));
        compute_command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, compute_queue_index_));
        compute_execution_manager_.reset(new vkw::ExecutionManager(device, compute_queue_index_));

        InitializeResources();

        vkGetDeviceQueue(device_, graphics_queue_index, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, compute_queue_index, 0, &compute_queue_);
    }

    void HybridRenderer::Clear(RadeonRays::float3 const& val,
                               Output& output) const
    {

    }

    void HybridRenderer::BuildDeferredCommandBuffer(VkDeferredPushConstants const& push_consts)
    {
        static std::vector<VkClearValue> clear_values =
                {
                        { 0.f, 0.f, 0.f, 1.0f }
                };

        VkDeviceSize offsets[1] = { 0 };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, deferred_buffer_);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        VkDescriptorSet desc_set = deferred_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdPushConstants(command_buffer, deferred_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkDeferredPushConstants), &push_consts);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        deferred_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildGbufferCommandBuffer(VkwScene const& scene)
    {
        static std::vector<VkClearValue> clear_values =
                {
                        { 0.f, 0.f, 0.f, 0.f },
                        { 0.f, 0.f, 0.f, 1.f },
                        { 0.f, 0.f, 0.f, 1.f },
                        { 0.f, 0.f, 0.f, 1.f },
                        { 0.f, 0.f }
                };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.pipeline.get());

        VkDeviceSize offsets[1] = { 0 };

        VkBuffer vb = scene.mesh_vertex_buffer.get();
        graphics_command_buffer_builder_->BeginRenderPass(clear_values, g_buffer_);

        for (size_t transform_pass = 0; transform_pass < scene.mesh_transforms.size(); transform_pass++)
        {
            VkDescriptorSet desc_set = mrt_descriptor_sets[transform_pass].descriptor_set.get();
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

            vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

            vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
            vkCmdBindIndexBuffer(command_buffer, scene.mesh_index_buffer.get(), 0, VK_INDEX_TYPE_UINT32);

            size_t max_range = std::min((transform_pass + 1) * kMaxTransforms, scene.meshes.size());
            size_t transform_id = 0;

            for (size_t mesh_id = transform_pass * kMaxTransforms; mesh_id < max_range; mesh_id++)
            {
                VkwScene::VkwMesh const& mesh = scene.meshes[mesh_id];

                VkwScene::Material::Value const diffuse = scene.materials[mesh.material_id].diffuse_color;
                VkwScene::Material::Value const metalness = scene.materials[mesh.material_id].reflection_metalness;
                VkwScene::Material::Value const roughness = scene.materials[mesh.material_id].reflection_roughness;
                VkwScene::Material::Value const normal = scene.materials[mesh.material_id].shading_normal;
                VkwScene::Material::Value const ior = scene.materials[mesh.material_id].reflection_ior;
                VkwScene::Material::Value const transparency = scene.materials[mesh.material_id].transparency;

                float diffuse_tex_id = diffuse.isTexture ? static_cast<float>(diffuse.texture_id) : -1;
                float metalness_tex_id = metalness.isTexture ? static_cast<float>(metalness.texture_id) : -1;
                float roughness_tex_id = roughness.isTexture ? static_cast<float>(roughness.texture_id) : -1;
                float ior_tex_id = ior.isTexture ? static_cast<float>(ior.texture_id) : -1;
                float shading_normal_tex_id = normal.isTexture ? static_cast<float>(normal.texture_id) : -1;
                float transparency_tex_id = transparency.isTexture ? static_cast<float>(transparency.texture_id) : -1;

                VkMaterialConstants push_constants;
                push_constants.diffuse      = float4(diffuse.color.x, diffuse.color.y, diffuse.color.z, diffuse_tex_id);
                push_constants.metalness    = float4(metalness.color.x, metalness.color.y, metalness.color.z, metalness_tex_id);
                push_constants.roughness    = float4(roughness.color.x, roughness.color.y, roughness.color.z, roughness_tex_id);
                push_constants.normal       = float4(normal.color.x, normal.color.y, normal.color.z, shading_normal_tex_id);
                push_constants.ior          = float4(ior.color.x, ior.color.y, ior.color.z, ior_tex_id);
                push_constants.transparency = float4(transparency.color.x, transparency.color.y, transparency.color.z, transparency_tex_id);

                uint32_t vs_push_data[4] = { static_cast<uint32_t>(transform_id), static_cast<uint32_t>(mesh_id % 255 + 1), 0, 0 };
                vkCmdPushConstants(command_buffer, mrt_pipeline_.layout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vs_push_data), &vs_push_data);
                vkCmdPushConstants(command_buffer, mrt_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vs_push_data), sizeof(VkMaterialConstants), &push_constants);

                vkCmdDrawIndexed(command_buffer, mesh.index_count, 1, mesh.index_base, 0, 0);

                transform_id++;
                transform_id = transform_id % kMaxTransforms;
            }
        }

        graphics_command_buffer_builder_->EndRenderPass();

        g_buffer_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildEdgeDetectCommandBuffer()
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 1.0f }
        };

        VkDeviceSize offsets[1] = { 0 };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, edge_detect_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, edge_detect_buffer_);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        VkDescriptorSet desc_set = edge_detect_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, edge_detect_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        int push_consts = msaa_num_samples_;

        vkCmdPushConstants(command_buffer, edge_detect_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &push_consts);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        edge_detect_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildTXAACommandBuffer()
    {
        static std::vector<VkClearValue> clear_values =
                {
                        { 0.f, 0.f, 0.f, 1.0f }
                };

        VkDeviceSize offsets[1] = { 0 };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, txaa_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, txaa_buffer_);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        VkDescriptorSet desc_set = txaa_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, txaa_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        txaa_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildCopyCommandBuffer()
    {
        std::vector<VkClearValue> clear_values;

        VkDeviceSize offsets[1] = { 0 };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, copy_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, history_buffer_);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        VkDescriptorSet desc_set = copy_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, copy_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        copy_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildTonemapperCommandBuffer(VkwOutput const& output)
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 1.0f }
        };

        VkDeviceSize offsets[1] = { 0 };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemap_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, output.GetRenderTarget());

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        VkDescriptorSet desc_set = tonemap_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemap_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        VkTonemapperPushConstants push_consts;
        push_consts.data = RadeonRays::float4(static_cast<float>(tonemap_output_),
                                             static_cast<float>(luminance_downsampled_buffer_.GetNumMips() - 1),
                                             0.f, 0.f);

        vkCmdPushConstants(command_buffer, deferred_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkTonemapperPushConstants), &push_consts);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        tonemap_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildCalcLuminanceCommandBuffer()
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 1.0f }
        };

        VkDeviceSize offsets[1] = { 0 };

        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, log_luminance_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, luminance_buffer_);

        uint32_t width = luminance_buffer_.attachments[0].width;
        uint32_t height = luminance_buffer_.attachments[0].height;

        VkViewport viewport = utils_.CreateViewport(static_cast<float>(width), static_cast<float>(height), 0.f, 1.0f);

        VkRect2D scissor =
        {
            { 0, 0 },
            { width, height }
        };

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        VkDescriptorSet desc_set = log_luminance_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, log_luminance_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        VkImageSubresourceRange sub_range = {};
        sub_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        sub_range.baseMipLevel = 0;
        sub_range.levelCount = 1;
        sub_range.layerCount = 1;

        VkImage image = luminance_buffer_.attachments[0].image.get();
        VkFormat format = luminance_buffer_.attachments[0].format;

        vkw::MemoryManager::TransitionImageLayout(command_buffer, image, luminance_buffer_.attachments[0].format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sub_range);

        // Blit from luminance framebuffer to luminance texture with linear filtering
        VkImageBlit image_blit = {};
        image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit.srcSubresource.layerCount = 1;
        image_blit.srcSubresource.mipLevel = 0;
        image_blit.srcOffsets[1].x = int32_t(width);
        image_blit.srcOffsets[1].y = int32_t(height);
        image_blit.srcOffsets[1].z = 1;
        image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit.dstSubresource.layerCount = 1;
        image_blit.dstSubresource.mipLevel = 0;
        image_blit.dstOffsets[1].x = int32_t(width >> 1);
        image_blit.dstOffsets[1].y = int32_t(height >> 1);
        image_blit.dstOffsets[1].z = 1;

        vkw::MemoryManager::TransitionImageLayout(command_buffer, luminance_downsampled_buffer_.GetImage(), luminance_downsampled_buffer_.GetFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, sub_range);

        vkCmdBlitImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, luminance_downsampled_buffer_.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

        // Generate mips for log luminance
        luminance_downsampled_buffer_.GenerateMips(command_buffer);

        vkw::MemoryManager::TransitionImageLayout(command_buffer, image, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sub_range);

        // Luminance adaptation pass
        VkViewport viewport_1x1 = utils_.CreateViewport(static_cast<float>(1), static_cast<float>(1), 0.f, 1.0f);
        VkRect2D scissor_1x1 =
        {
            { 0, 0 },
            { 1, 1 }
        };

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, log_luminance_adapt_pipeline_.pipeline.get());

        graphics_command_buffer_builder_->BeginRenderPass(clear_values, adapted_lum_buffer_);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_1x1);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_1x1);

        const float tau = 0.07f;

        VkLumaAdaptPushConstants push_consts;
        push_consts.data = RadeonRays::float4(dt_, tau, 0.f, 0.f);

        VkDescriptorSet log_lum_adapt_desc_set = log_luminance_adapt_shader_.descriptor_set.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, log_luminance_adapt_pipeline_.layout.get(), 0, 1, &log_lum_adapt_desc_set, 0, NULL);

        vkCmdPushConstants(command_buffer, log_luminance_adapt_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkLumaAdaptPushConstants), &push_consts);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        graphics_command_buffer_builder_->EndRenderPass();

        // Copy adapted luminance to prev luminance buffer
        image_blit = {};
        image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit.srcSubresource.layerCount = 1;
        image_blit.srcSubresource.mipLevel = 0;
        image_blit.srcOffsets[1].x = 1;
        image_blit.srcOffsets[1].y = 1;
        image_blit.srcOffsets[1].z = 1;

        image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit.dstSubresource.layerCount = 1;
        image_blit.dstSubresource.mipLevel = 0;
        image_blit.dstOffsets[1].x = 1;
        image_blit.dstOffsets[1].y = 1;
        image_blit.dstOffsets[1].z = 1;

        vkw::MemoryManager::TransitionImageLayout(command_buffer, adapted_lum_buffer_.attachments[0].image.get(), luminance_downsampled_buffer_.GetFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sub_range);
        vkw::MemoryManager::TransitionImageLayout(command_buffer, prev_lum_buffer_.attachments[0].image.get(), prev_lum_buffer_.attachments[0].format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, sub_range);

        vkCmdBlitImage(command_buffer, adapted_lum_buffer_.attachments[0].image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, prev_lum_buffer_.attachments[0].image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

        vkw::MemoryManager::TransitionImageLayout(command_buffer, adapted_lum_buffer_.attachments[0].image.get(), luminance_downsampled_buffer_.GetFormat(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sub_range);
        vkw::MemoryManager::TransitionImageLayout(command_buffer, prev_lum_buffer_.attachments[0].image.get(), prev_lum_buffer_.attachments[0].format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sub_range);

        calc_luminance_cmd_ = graphics_command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::DrawGbufferPass(VkwScene const& scene)
    {
        VkCommandBuffer g_buffer_cmd = g_buffer_cmd_.get();
        VkSemaphore g_buffer_finised = g_buffer_finisned_.get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &g_buffer_finised;
        submit_info.signalSemaphoreCount = 1;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &g_buffer_cmd;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::DrawEdgeDetectPass()
    {
        VkCommandBuffer edge_detect_buffer_cmd = edge_detect_cmd_.get();
        VkSemaphore edge_detect_buffer_finised = edge_detect_finisned_.get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &edge_detect_buffer_finised;
        submit_info.signalSemaphoreCount = 1;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &edge_detect_buffer_cmd;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::DrawDeferredPass(VkwScene const& scene)
    {
        VkCommandBuffer deferred_cmd_buf = deferred_cmd_.get();

        std::vector<VkSemaphore> wait_semaphores;
        std::vector<VkPipelineStageFlags> stage_wait_bits;

        wait_semaphores.push_back(g_buffer_finisned_.get());
        wait_semaphores.push_back(edge_detect_finisned_.get());

        stage_wait_bits.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        stage_wait_bits.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        for (auto const& shadow_semaphore : scene.shadows_finished_signal)
        {
            wait_semaphores.push_back(shadow_semaphore.get());
            stage_wait_bits.push_back(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }
        scene.shadows_finished_signal.clear();

        VkSemaphore deferred_finished = deferred_finished_.get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &deferred_finished;
        submit_info.signalSemaphoreCount = 1;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = stage_wait_bits.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &deferred_cmd_buf;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::DrawTXAAPass()
    {
        VkCommandBuffer txaa_cmd_buf = txaa_cmd_.get();

        VkSemaphore txaa_finished = txaa_finished_.get();

        std::vector<VkSemaphore> wait_semaphores;
        std::vector<VkPipelineStageFlags> stage_wait_bits;

        wait_semaphores.push_back(deferred_finished_.get());
        stage_wait_bits.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &txaa_finished;
        submit_info.signalSemaphoreCount = 1;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = stage_wait_bits.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &txaa_cmd_buf;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::DrawCalcLuminancePass()
    {
        VkCommandBuffer calc_lum_cmd = calc_luminance_cmd_.get();
        VkSemaphore calc_lum_finised = calc_luminance_finished_.get();

        std::vector<VkSemaphore> wait_semaphores;
        std::vector<VkPipelineStageFlags> stage_wait_bits;

        wait_semaphores.push_back(txaa_finished_.get());
        stage_wait_bits.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &calc_lum_finised;
        submit_info.signalSemaphoreCount = 1;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &calc_lum_cmd;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = stage_wait_bits.data();

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::DrawTonemapPass()
    {
        VkCommandBuffer tonemap_cmd_buf = tonemap_cmd_.get();
        VkSemaphore tonemap_finished = tonemap_finished_.get();

        std::vector<VkSemaphore> wait_semaphores;
        std::vector<VkPipelineStageFlags> stage_wait_bits;

        wait_semaphores.push_back(calc_luminance_finished_.get());

        stage_wait_bits.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &tonemap_finished;
        submit_info.signalSemaphoreCount = 1;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = stage_wait_bits.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &tonemap_cmd_buf;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::CopyToHistoryBuffer(VkwOutput const& output)
    {
        VkCommandBuffer copy_cmd = copy_cmd_.get();

        VkSemaphore render_finished = output.GetSemaphore();

        std::vector<VkSemaphore> wait_semaphores;
        std::vector<VkPipelineStageFlags> stage_wait_bits;

        wait_semaphores.push_back(tonemap_finished_.get());
        stage_wait_bits.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &render_finished;
        submit_info.signalSemaphoreCount = 0;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = stage_wait_bits.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &copy_cmd;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");
    }

    void HybridRenderer::UpdateDeferredPass(VkwScene const& scene, VkwOutput const& vk_output)
    {
        std::vector<VkImageView> shadow_image_views;
        std::vector<VkSampler> shadow_samplers;

        for (auto const& tex : scene.shadows)
        {
            shadow_image_views.push_back(tex.GetImageView());
            shadow_samplers.push_back(shadow_sampler_.get());
        }

        // Fill array with dummy textures to make validation layer happy
        while (shadow_image_views.size() < kMaxLights)
        {
            shadow_image_views.push_back(inf_pixel_.GetImageView());
            shadow_samplers.push_back(shadow_sampler_.get());
        }

        VkBuffer point_lights = scene.point_lights != VK_NULL_HANDLE ? scene.point_lights.get()
                                                                     : dummy_buffer_.get();

        VkBuffer spot_lights = scene.spot_lights != VK_NULL_HANDLE ? scene.spot_lights.get()
                                                                   : dummy_buffer_.get();

        VkBuffer directional_lights = scene.directional_lights != VK_NULL_HANDLE ? scene.directional_lights.get()
                                                                                 : dummy_buffer_.get();

        VkBuffer point_lights_transforms = scene.point_lights_transforms != VK_NULL_HANDLE ? scene.point_lights_transforms.get()
                                                                                           : dummy_buffer_.get();

        VkBuffer spot_lights_transforms = scene.spot_lights_transforms != VK_NULL_HANDLE ? scene.spot_lights_transforms.get()
                                                                                         : dummy_buffer_.get();

        VkBuffer directional_lights_transforms = scene.directional_lights_transforms != VK_NULL_HANDLE ? scene.directional_lights_transforms.get()
                                                                                                       : dummy_buffer_.get();

        VkImageView env_map = scene.env_map_idx == 0xFFFFFFFF ? black_pixel_.GetImageView()
                                                              : scene.textures[scene.env_map_idx].GetImageView();

        VkImageView env_map_prefiltered_reflections = scene.env_map_idx == 0xFFFFFFFF ? black_pixel_.GetImageView()
                                                                                      : scene.ibl_skylight_reflections.GetImageView();

        VkImageView brdf_lut = scene.env_map_idx == 0xFFFFFFFF ? black_pixel_.GetImageView()
                                                               : scene.ibl_brdf_lut.GetImageView();

        VkBuffer env_map_irradiance = scene.env_map_irradiance_sh9 != VK_NULL_HANDLE ? scene.env_map_irradiance_sh9.get()
                                                                                     : dummy_buffer_.get();

        deferred_shader_.SetArg(5, scene.camera.get());
        deferred_shader_.SetArgArray(6, shadow_image_views, shadow_samplers);
        deferred_shader_.SetArg(7, point_lights);
        deferred_shader_.SetArg(8, spot_lights);
        deferred_shader_.SetArg(9, directional_lights);
        deferred_shader_.SetArg(10, point_lights_transforms);
        deferred_shader_.SetArg(11, spot_lights_transforms);
        deferred_shader_.SetArg(12, directional_lights_transforms);
        deferred_shader_.SetArg(13, env_map, linear_samplers_repeat_[0].get());
        deferred_shader_.SetArg(14, env_map_irradiance);
        deferred_shader_.SetArg(15, env_map_prefiltered_reflections, prefiltered_reflections_clamp_sampler_.get());
        deferred_shader_.SetArg(16, brdf_lut, linear_sampler_clamp_.get());
        deferred_shader_.SetArg(17, edge_detect_buffer_.GetImageView(), nearest_sampler_.get());
        
        deferred_shader_.CommitArgs();

        VkDeferredPushConstants push_consts = {
                static_cast<int>(scene.light_count),
                static_cast<int>(scene.num_point_lights),
                static_cast<int>(scene.num_spot_lights),
                static_cast<int>(scene.num_directional_lights),
                scene.cascade_splits_dist,
                static_cast<float>(msaa_num_samples_),
                scene.ibl_multiplier
        };

        BuildDeferredCommandBuffer(push_consts);

        txaa_shader_.SetArg(1, deferred_buffer_.GetImageView(), nearest_sampler_.get());
        txaa_shader_.SetArg(2, history_buffer_.GetImageView(), linear_sampler_clamp_.get());
        txaa_shader_.SetArg(3, g_buffer_.GetImageView(2), nearest_sampler_.get());
        txaa_shader_.SetArg(4, g_buffer_.GetImageView(4), nearest_sampler_.get());
        txaa_shader_.CommitArgs();

        uint32_t last_mip = luminance_downsampled_buffer_.GetNumMips() - 1;
        log_luminance_adapt_shader_.SetArg(0, luminance_downsampled_buffer_.GetImageView(last_mip), linear_sampler_clamp_.get());
        log_luminance_adapt_shader_.SetArg(1, prev_lum_buffer_.GetImageView(), nearest_sampler_.get());
        log_luminance_adapt_shader_.CommitArgs();

        tonemap_shader_.SetArg(0, txaa_buffer_.GetImageView(), nearest_sampler_.get());
        tonemap_shader_.SetArg(1, adapted_lum_buffer_.GetImageView(), linear_sampler_clamp_.get());
        tonemap_shader_.CommitArgs();

        BuildTXAACommandBuffer();
        BuildTonemapperCommandBuffer(vk_output);

        edge_detect_shader_.SetArg(0, g_buffer_.GetImageView(0), nearest_sampler_.get());
        edge_detect_shader_.SetArg(1, g_buffer_.GetImageView(4), nearest_sampler_.get());
        edge_detect_shader_.SetArg(2, g_buffer_.GetImageView(3), nearest_sampler_.get());

        edge_detect_shader_.CommitArgs();

        BuildEdgeDetectCommandBuffer();

        copy_shader_.SetArg(1, txaa_buffer_.GetImageView(), nearest_sampler_.get());
        copy_shader_.CommitArgs();

        BuildCopyCommandBuffer();
    }

    void HybridRenderer::UpdateGbufferPass(VkwScene const& scene)
    {
        mrt_texture_image_views_.clear();
        mrt_texture_samplers_.clear();

        mrt_texture_image_views_.reserve(kMaxTextures);
        mrt_texture_samplers_.reserve(kMaxTextures);

        for (auto const& tex : scene.textures)
        {
            mrt_texture_image_views_.push_back(tex.GetImageView());

            uint32_t num_mips = tex.GetNumMips();

            if (static_cast<uint32_t>(linear_samplers_repeat_.size()) < num_mips)
            {
                for (size_t i = linear_samplers_repeat_.size(); i < num_mips; i++)
                    linear_samplers_repeat_.push_back(utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.f, 1.0f + static_cast<float>(i)));
            }

            mrt_texture_samplers_.push_back(linear_samplers_repeat_[num_mips - 1].get());
        }

        // Fill array with dummy textures to make validation layer happy
        while (mrt_texture_image_views_.size() < kMaxTextures)
        {
            mrt_texture_image_views_.push_back(black_pixel_.GetImageView());
            mrt_texture_samplers_.push_back(linear_samplers_repeat_[0].get());
        }

        /*if (mrt_descriptor_sets.size() < scene.mesh_transforms.size())
        {*/
            mrt_descriptor_sets.clear();
            mrt_descriptor_sets.resize(scene.mesh_transforms.size());

            for (size_t idx = 0; idx < scene.mesh_transforms.size(); idx++)
            {
                mrt_descriptor_sets[idx] = shader_manager_.CreateDescriptorSet(mrt_shader_);
                mrt_descriptor_sets[idx].SetArg(0, scene.camera.get());
                mrt_descriptor_sets[idx].SetArg(1, scene.mesh_transforms[idx].get());
                mrt_descriptor_sets[idx].SetArg(2, scene.prev_mesh_transforms[idx].get());
                mrt_descriptor_sets[idx].SetArg(3, jitter_buffer_.get());
                mrt_descriptor_sets[idx].SetArgArray(4, mrt_texture_image_views_, mrt_texture_samplers_);
                mrt_descriptor_sets[idx].CommitArgs();
            }
        //}

        BuildGbufferCommandBuffer(scene);
    }

    void HybridRenderer::UpdateJitterBuffer()
    {
        RadeonRays::float2 inv_screen_size = RadeonRays::float2(inv_framebuffer_width_, inv_framebuffer_height_);
        RadeonRays::float2 sub_sample = txaa_sample_locations_[txaa_frame_idx_] * inv_screen_size;

        RadeonRays::float2 jitter = RadeonRays::float2(sub_sample.x, sub_sample.y);
        RadeonRays::float2 jitter_offset = (jitter - float2(prev_jitter_.m03, prev_jitter_.m13)) * 0.5f;

        matrix jitter_matrix;
        jitter_matrix.m03 = jitter.x;
        jitter_matrix.m13 = jitter.y;

        VkJitterBuffer jitter_buffer;
        jitter_buffer.jitter = jitter_matrix;
        jitter_buffer.prev_jitter = prev_jitter_;
        jitter_buffer.offsets = RadeonRays::float4(jitter_offset.x, jitter_offset.y, 0.f, 0.f);

        memory_manager_.WriteBuffer(jitter_buffer_.get(), 0u, sizeof(VkJitterBuffer), &jitter_buffer);

        prev_jitter_ = jitter_matrix;
        txaa_frame_idx_ = (txaa_frame_idx_ + 1) % txaa_num_samples_;
    }

    void HybridRenderer::Render(VkwScene const& scene)
    {
        Output* output = GetOutput(OutputType::kColor);

        if (output == nullptr)
            throw std::runtime_error("HybridRenderer: output buffer is not set");

        VkwOutput* vk_output = dynamic_cast<VkwOutput*>(output);

        if (vk_output == nullptr)
            throw std::runtime_error("HybridRenderer: Internal error");

        if (scene.rebuild_deferred_pass)
        {
            UpdateDeferredPass(scene, *vk_output);
            scene.rebuild_deferred_pass = false;
        }

        if (scene.rebuild_mrt_pass)
        {
            UpdateGbufferPass(scene);
            scene.rebuild_mrt_pass = false;
        }

        BuildCalcLuminanceCommandBuffer();

        UpdateJitterBuffer();

        DrawGbufferPass(scene);
        DrawEdgeDetectPass();
        DrawDeferredPass(scene);
        DrawTXAAPass();
        DrawCalcLuminancePass();
        DrawTonemapPass();

        CopyToHistoryBuffer(*vk_output);

        static auto prevtime = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(time - prevtime);

        dt_ = static_cast<float>(dt.count() / 100.0f);
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

                inv_framebuffer_width_ = 1.f / static_cast<float>(framebuffer_width_);
                inv_framebuffer_height_ = 1.f / static_cast<float>(framebuffer_height_);

                viewport_ = vkw::Utils::CreateViewport(
                        static_cast<float>(output->width()),
                        static_cast<float>(output->height()),
                        0.f, 1.f);

                scissor_ =
                        {
                                { 0, 0 },
                                { output->width(), output->height() }
                        };
            }

            ClearTemporalHistory();

            if (type == OutputType::kColor)
            {
                VkwOutput* vk_output = dynamic_cast<VkwOutput*>(output);

                if (vk_output == nullptr)
                    throw std::runtime_error("HybridRenderer: Internal error");

                for (uint32_t idx = 0; idx < static_cast<uint32_t>(g_buffer_.attachments.size()); idx++)
                {
                    deferred_shader_.SetArg(idx, g_buffer_.GetImageView(idx), nearest_sampler_.get());
                }

                tonemap_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(tonemap_shader_, vk_output->GetRenderTarget().render_pass.get());

                deferred_shader_.CommitArgs();
            }
        }
    }


    void HybridRenderer::EnableTonemapper(bool enabled)
    {
        tonemap_output_ = enabled;
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

        dummy_buffer_ = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix),
                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        jitter_buffer_ = memory_manager_.CreateBuffer(sizeof(VkJitterBuffer),
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        mrt_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/mrt.vert.spv", "../Baikal/Kernels/VK/mrt.frag.spv");
        deferred_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/deferred.frag.spv");
        edge_detect_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/edge_detection.frag.spv");
        txaa_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/txaa.frag.spv");
        copy_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/copy_rt.frag.spv");
        tonemap_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/tonemap.frag.spv");
        log_luminance_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/log_luminance.frag.spv");
        log_luminance_adapt_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/deferred.vert.spv", "../Baikal/Kernels/VK/log_luminance_adapt.frag.spv");

        txaa_sample_locations_.resize(txaa_num_samples_);

        for (uint32_t i = 0; i < txaa_num_samples_; i++)
        {
            RadeonRays::float2 sample = Hammersley2D(i, txaa_num_samples_) * 2.0f - RadeonRays::float2(1.f, 1.f);
            txaa_sample_locations_[i] = sample;
        }

        VkExtent3D tex_size = { 2, 2, 1 };

        float black_pixel_data[4] = { 0.f, 0.f, 0.f, 0.f };
        float inf_pixel_data[4] = { 1e20f, 1e20f, 1e20f, 1e20f };

        black_pixel_.SetTexture(&memory_manager_, tex_size, VK_FORMAT_R16_SFLOAT, false, sizeof(black_pixel_data), &black_pixel_data);
        inf_pixel_.SetTexture(&memory_manager_, tex_size, VK_FORMAT_R16_SFLOAT, false, sizeof(inf_pixel_data), &inf_pixel_data);

        std::vector<vkw::RenderTargetCreateInfo> lum_buffer_attachments = {
            { 1, 1, VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL }
        };

        prev_lum_buffer_ = render_target_manager_.CreateRenderTarget(lum_buffer_attachments);
        adapted_lum_buffer_ = render_target_manager_.CreateRenderTarget(lum_buffer_attachments);
        log_luminance_adapt_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(log_luminance_adapt_shader_, adapted_lum_buffer_.render_pass.get());

        nearest_sampler_ = utils_.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        linear_sampler_clamp_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        prefiltered_reflections_clamp_sampler_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.f, 11.f);
        shadow_sampler_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.f, 1.f, VK_TRUE, VK_COMPARE_OP_LESS);
        linear_samplers_repeat_.push_back(utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.f, 1.0f));
        linear_lum_sampler_clamp_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.f, 1.0f);

        deferred_finished_          = utils_.CreateSemaphore();
        edge_detect_finisned_       = utils_.CreateSemaphore();
        g_buffer_finisned_          = utils_.CreateSemaphore();
        txaa_finished_              = utils_.CreateSemaphore();
        tonemap_finished_           = utils_.CreateSemaphore();
        calc_luminance_finished_    = utils_.CreateSemaphore();
    }

    void HybridRenderer::SetMaxBounces(std::uint32_t max_bounces)
    {

    }

    void HybridRenderer::ResizeRenderTargets(uint32_t width, uint32_t height)
    {
        VkSampleCountFlagBits samples_count = VkSampleCountFlagBits(msaa_num_samples_);

        static std::vector<vkw::RenderTargetCreateInfo> attachments = {

                { width, height, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                                 VK_ATTACHMENT_LOAD_OP_CLEAR,  
                                 samples_count },           // normals
                { width, height, VK_FORMAT_R8G8B8A8_UNORM, 
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, 
                                 samples_count },          // albedo
                { width, height, VK_FORMAT_R16G16_SFLOAT, 
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, 
                                 samples_count },          // motion
                { width, height, VK_FORMAT_R8G8B8A8_UNORM, 
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, 
                                 samples_count },          // roughness, metalness, mesh id
                { width, height, VK_FORMAT_D32_SFLOAT, 
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, 
                                 samples_count },          // depth
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
        rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_state.flags = 0;
        rasterization_state.depthClampEnable = VK_FALSE;
        rasterization_state.lineWidth = 1.0f;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {?};
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.pNext = nullptr;
        depth_stencil_state.depthTestEnable = VK_TRUE;
        depth_stencil_state.depthWriteEnable = VK_TRUE;
        depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        depth_stencil_state.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil_state.front = depth_stencil_state.back;

        VkPipelineMultisampleStateCreateInfo multisample_state = {};
        multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state.rasterizationSamples = samples_count;

        vkw::GraphicsPipelineState mrt_pipeline_state;
        mrt_pipeline_state.color_blend_state = &color_blend_state;
        mrt_pipeline_state.rasterization_state = &rasterization_state;
        mrt_pipeline_state.depth_stencil_state = &depth_stencil_state;
        mrt_pipeline_state.multisample_state = &multisample_state;

        g_buffer_ = render_target_manager_.CreateRenderTarget(attachments);

        static std::vector<vkw::RenderTargetCreateInfo> deferred_attachments = {
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT }
        };

        static std::vector<vkw::RenderTargetCreateInfo> history_attachments = {
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE }
        };

        static std::vector<vkw::RenderTargetCreateInfo> txaa_attachments = {
            { width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT }
        };

        static std::vector<vkw::RenderTargetCreateInfo> luminance_attachments = {
            { width >> 1, height >> 1, VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT }
        };

        static std::vector<vkw::RenderTargetCreateInfo> edge_detect_attachment = {
            { width, height, VK_FORMAT_R8_UNORM, 
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                             VK_ATTACHMENT_LOAD_OP_CLEAR }
        };

        deferred_buffer_    = render_target_manager_.CreateRenderTarget(deferred_attachments);
        history_buffer_     = render_target_manager_.CreateRenderTarget(history_attachments);
        txaa_buffer_        = render_target_manager_.CreateRenderTarget(txaa_attachments);
        luminance_buffer_   = render_target_manager_.CreateRenderTarget(luminance_attachments);
        edge_detect_buffer_ = render_target_manager_.CreateRenderTarget(edge_detect_attachment);

        luminance_downsampled_buffer_.SetTexture(&memory_manager_, { width >> 2, height >> 2, 1 }, VK_FORMAT_R16_SFLOAT, true);

        float num_mips = static_cast<float>(luminance_downsampled_buffer_.GetNumMips());
        linear_lum_sampler_clamp_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.f, num_mips);


        // Use specialization constants to pass number of samples to the shader (used for MSAA resolve)
        VkSpecializationMapEntry specializationEntry{};
        specializationEntry.constantID = 0;
        specializationEntry.offset = 0;
        specializationEntry.size = sizeof(uint32_t);

        uint32_t specializationData = samples_count;

        VkSpecializationInfo specializationInfo;
        specializationInfo.mapEntryCount = 1;
        specializationInfo.pMapEntries = &specializationEntry;
        specializationInfo.dataSize = sizeof(specializationData);
        specializationInfo.pData = &specializationData;

        vkw::GraphicsPipelineState msaa_pipeline_state;
        msaa_pipeline_state.fragment_specialization_info = &specializationInfo;

        deferred_pipeline_      = pipeline_manager_.CreateGraphicsPipeline(deferred_shader_, deferred_buffer_.render_pass.get(), &msaa_pipeline_state);
        edge_detect_pipeline_   = pipeline_manager_.CreateGraphicsPipeline(edge_detect_shader_, edge_detect_buffer_.render_pass.get(), &msaa_pipeline_state);

        mrt_pipeline_           = pipeline_manager_.CreateGraphicsPipeline(mrt_shader_, g_buffer_.render_pass.get(), &mrt_pipeline_state);
        copy_pipeline_          = pipeline_manager_.CreateGraphicsPipeline(copy_shader_, history_buffer_.render_pass.get());
        txaa_pipeline_          = pipeline_manager_.CreateGraphicsPipeline(txaa_shader_, txaa_buffer_.render_pass.get());
        log_luminance_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(log_luminance_shader_, luminance_buffer_.render_pass.get());

        log_luminance_shader_.SetArg(0, txaa_buffer_.GetImageView(), linear_lum_sampler_clamp_.get());
        log_luminance_shader_.CommitArgs();
    }

    void HybridRenderer::ClearTemporalHistory()
    {
        graphics_command_buffer_builder_->BeginCommandBuffer();

        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkClearColorValue clear_color = { 0, 0, 0, 1 };
        VkCommandBuffer clear_cmd_buffer = graphics_command_buffer_builder_->GetCurrentCommandBuffer();

        VkImageSubresourceRange image_sub_range = {};
        image_sub_range.baseMipLevel = 0;
        image_sub_range.levelCount = 1;
        image_sub_range.baseArrayLayer = 0;
        image_sub_range.layerCount = 1;

        memory_manager_.TransitionImageLayout(clear_cmd_buffer, history_buffer_.attachments[0].image.get(), VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image_sub_range);

        vkCmdClearColorImage(clear_cmd_buffer, history_buffer_.attachments[0].image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_range);

        memory_manager_.TransitionImageLayout(clear_cmd_buffer, history_buffer_.attachments[0].image.get(), VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image_sub_range);

        vkw::CommandBuffer cmd_buffer = graphics_command_buffer_builder_->EndCommandBuffer();

        clear_cmd_buffer = cmd_buffer.get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &clear_cmd_buffer;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("HybridRenderer: queue submission failed");

        vkDeviceWaitIdle(device_);
    }
}
