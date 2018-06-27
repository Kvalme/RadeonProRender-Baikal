#include "hybrid_renderer.h"

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
    {
        command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, graphics_queue_index_));

        InitializeResources();

        nearest_sampler_ = utils_.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        linear_sampler_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

        gbuffer_signal_ = utils_.CreateSemaphore();

        vkGetDeviceQueue(device_, graphics_queue_index, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, compute_queue_index, 0, &compute_queue_);

        VkExtent3D tex_size = {2, 2, 1};
        
        float black_pixel_data[4] = { 0.f, 0.f, 0.f, 0.f };
        float inf_pixel_data[4] = { 1e20f, 1e20f, 1e20f, 1e20f };

        black_pixel_.SetTexture(&memory_manager_, tex_size, VK_FORMAT_R16_SFLOAT, sizeof(black_pixel_data), &black_pixel_data);
        inf_pixel_.SetTexture(&memory_manager_, tex_size, VK_FORMAT_R16_SFLOAT, sizeof(inf_pixel_data), &inf_pixel_data);

        dummy_buffer_ = memory_manager.CreateBuffer(sizeof(RadeonRays::matrix), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    void HybridRenderer::Clear(RadeonRays::float3 const& val,
        Output& output) const
    {

    }

    void HybridRenderer::BuildDeferredCommandBuffer(VkwOutput const& output, VkDeferredPushConstants const& push_consts)
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 1.0f }
        };

        VkDeviceSize offsets[1] = { 0 };

        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_pipeline_.pipeline.get());

        command_buffer_builder_->BeginRenderPass(clear_values, output.GetRenderTarget());

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        VkDescriptorSet desc_set = deferred_frag_shader_.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        VkBuffer vb = fullscreen_quad_vb_.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);
        
        vkCmdPushConstants(command_buffer, deferred_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkDeferredPushConstants), &push_consts);
        
        vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

        command_buffer_builder_->EndRenderPass();

        deferred_cmd_ = command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::BuildGbufferCommandBuffer(VkwScene const& scene)
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 0.f, 0.f, 0.f, 0.0f },
            { 0.f, 0.f, 0.f, 1.0f },
            { 0.f, 0.f, 0.f, 1.0f },
            { 1.0f, 0.f }
        };

        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.pipeline.get());

        VkDeviceSize offsets[1] = { 0 };

        VkBuffer vb = scene.mesh_vertex_buffer.get();
        command_buffer_builder_->BeginRenderPass(clear_values, g_buffer_);

        VkDescriptorSet desc_set = mrt_shader_.descriptor_set.get();
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrt_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, scene.mesh_index_buffer.get(), 0, VK_INDEX_TYPE_UINT32);

        uint32_t mesh_id = 1;
        for (auto const& mesh : scene.meshes)
        {
            struct VkMaterialConstants
            {
                uint32_t data[4];
                
                float4 diffuse_data;              
                float4 reflection_data;
                float4 roughness_data;
                float4 ior_data;
                float4 shading_normal;
            };

            VkwScene::Material::Value const diffuse     = scene.materials[mesh.material_id].diffuse_color;
            VkwScene::Material::Value const reflection  = scene.materials[mesh.material_id].reflection_color;
            VkwScene::Material::Value const ior         = scene.materials[mesh.material_id].reflection_ior;
            VkwScene::Material::Value const roughness   = scene.materials[mesh.material_id].reflection_roughness;
            VkwScene::Material::Value const normal      = scene.materials[mesh.material_id].shading_normal;

            float diffuse_tex_id = diffuse.isTexture ? static_cast<float>(diffuse.texture_id) : -1;
            float reflection_tex_id = reflection.isTexture ? static_cast<float>(reflection.texture_id) : -1;
            float ior_tex_id = ior.isTexture ? static_cast<float>(ior.texture_id) : -1;
            float roughness_tex_id = roughness.isTexture ? static_cast<float>(roughness.texture_id) : -1;
            float shading_normal_tex_id = normal.isTexture ? static_cast<float>(normal.texture_id) : -1;

            VkMaterialConstants push_constants;
            push_constants.data[0] = mesh_id++;
            push_constants.diffuse_data     = float4(diffuse.color.x, diffuse.color.y, diffuse.color.z, diffuse_tex_id);
            push_constants.reflection_data  = float4(reflection.color.x, reflection.color.y, reflection.color.z, reflection_tex_id);
            push_constants.ior_data         = float4(ior.color.x, ior.color.y, ior.color.z, ior_tex_id);
            push_constants.roughness_data   = float4(roughness.color.x, roughness.color.y, roughness.color.z, roughness_tex_id);
            push_constants.shading_normal   = float4(normal.color.x, normal.color.y, normal.color.z, shading_normal_tex_id);

            uint32_t vs_push_data[4] = { push_constants.data[0], 0, 0, 0 };
            vkCmdPushConstants(command_buffer, mrt_pipeline_.layout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vs_push_data), &vs_push_data);
            vkCmdPushConstants(command_buffer, mrt_pipeline_.layout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vs_push_data), sizeof(VkMaterialConstants), &push_constants);

            vkCmdDrawIndexed(command_buffer, mesh.index_count, 1, mesh.index_base, 0, 0);
        }

        command_buffer_builder_->EndRenderPass();

        g_buffer_cmd_ = command_buffer_builder_->EndCommandBuffer();
    }

    void HybridRenderer::DrawDeferredPass(VkwOutput const& output, VkwScene const& scene)
    {
        VkCommandBuffer deferred_cmd_buf = deferred_cmd_.get();

        VkSemaphore g_buffer_finised = gbuffer_signal_.get();
        VkSemaphore render_finished = output.GetSemaphore();

        std::vector<VkSemaphore> wait_semaphores = { g_buffer_finised };
        std::vector<VkPipelineStageFlags> stage_wait_bits = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        for (auto const& shadow_semaphore : scene.shadows_finished_signal)
        {
            wait_semaphores.push_back(shadow_semaphore.get());
            stage_wait_bits.push_back(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }
        scene.shadows_finished_signal.clear();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &render_finished;
        submit_info.signalSemaphoreCount = 1;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = stage_wait_bits.data();
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

        if (scene.rebuild_cmd_buffers)
        {
            std::vector<VkImageView> texture_image_views;
            texture_image_views.reserve(kMaxTextures);

            std::vector<VkImageView> shadow_image_views;

            for (auto const& tex : scene.shadows)
            {
                shadow_image_views.push_back(tex.attachments[0].view.get());
            }

            for (auto const& tex : scene.textures)
            {
                texture_image_views.push_back(tex.GetImageView());
            }

            // Fill array with dummy textures to make validation layer happy
            while (texture_image_views.size() < kMaxTextures)
            {
                texture_image_views.push_back(black_pixel_.GetImageView());
            }

            // Fill array with dummy textures to make validation layer happy
            while (shadow_image_views.size() < kMaxLights)
            {
                shadow_image_views.push_back(inf_pixel_.GetImageView());
            }

            VkBuffer point_lights = scene.point_lights != VK_NULL_HANDLE ? scene.point_lights.get() 
                                                                         : dummy_buffer_.get();

            VkBuffer spot_lights = scene.spot_lights != VK_NULL_HANDLE ? scene.spot_lights.get() 
                                                                       : dummy_buffer_.get();

            VkBuffer directional_lights = scene.directional_lights != VK_NULL_HANDLE ? scene.directional_lights.get() 
                                                                                     : dummy_buffer_.get();

            deferred_frag_shader_.SetArg(4, scene.camera.get());
            deferred_frag_shader_.SetArgArray(5, shadow_image_views, nearest_sampler_.get());
            deferred_frag_shader_.SetArg(6, point_lights);
            deferred_frag_shader_.SetArg(7, spot_lights);
            deferred_frag_shader_.SetArg(8, directional_lights);
            deferred_frag_shader_.SetArg(9, scene.point_lights_transforms.get());
            deferred_frag_shader_.SetArg(10, scene.spot_lights_transforms.get());
            deferred_frag_shader_.SetArg(11, scene.directional_lights_transforms.get());
            deferred_frag_shader_.CommitArgs();
            
            mrt_shader_.SetArg(0, scene.camera.get());
            mrt_shader_.SetArg(1, scene.mesh_transforms.get());
            mrt_shader_.SetArgArray(2, texture_image_views, linear_sampler_.get());
            mrt_shader_.CommitArgs();

            VkDeferredPushConstants push_consts = { 
                static_cast<int>(scene.light_count), 
                static_cast<int>(scene.num_point_lights),
                static_cast<int>(scene.num_spot_lights),
                static_cast<int>(scene.num_directional_lights),
                scene.cascade_splits_dist
            };

            BuildDeferredCommandBuffer(*vk_output, push_consts);
            BuildGbufferCommandBuffer(scene);

            scene.rebuild_cmd_buffers = false;
        }

        DrawGbufferPass();
        DrawDeferredPass(*vk_output, scene);
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
                    { 0, 0 },
                    { output->width(), output->height() }
                };
            }

            if (type == OutputType::kColor)
            {
                VkwOutput* vk_output = dynamic_cast<VkwOutput*>(output);

                if (vk_output == nullptr)
                    throw std::runtime_error("HybridRenderer: Internal error");

                deferred_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(deferred_vert_shader_, deferred_frag_shader_, vk_output->GetRenderTarget().render_pass.get());

                for (uint32_t idx = 0; idx < static_cast<uint32_t>(g_buffer_.attachments.size()); idx++)
                {
                    deferred_frag_shader_.SetArg(idx, g_buffer_.attachments[idx].view.get(), nearest_sampler_.get());
                }

                deferred_frag_shader_.CommitArgs();
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

        deferred_vert_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_VERTEX_BIT, "../Baikal/Kernels/VK/deferred.vert.spv");
        deferred_frag_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../Baikal/Kernels/VK/deferred.frag.spv");

        mrt_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/mrt.vert.spv", "../Baikal/Kernels/VK/mrt.frag.spv");
    }

    void HybridRenderer::SetMaxBounces(std::uint32_t max_bounces)
    {

    }

    void HybridRenderer::ResizeRenderTargets(uint32_t width, uint32_t height)
    {
        static std::vector<vkw::RenderTargetCreateInfo> attachments = {
            { width, height, VK_FORMAT_R16G16B16A16_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT},   // xy - packed normals, next 24 bits - depth, 8 bits - mesh id
            { width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },      // albedo
            { width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },      // xy - motion, zw - roughness, metaliness
            { width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
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
        mrt_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(mrt_shader_, g_buffer_.render_pass.get(), &pipeline_state);
    }
}