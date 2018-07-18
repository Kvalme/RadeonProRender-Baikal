#include "vkw_shadow_controller.h"

#include "vkw_scene_helpers.h"

#include "SceneGraph/vkwscene.h"
#include "SceneGraph/scene1.h"
#include "SceneGraph/camera.h"
#include "SceneGraph/light.h"
#include "SceneGraph/Collector/collector.h"

#include <algorithm>
#include <cmath>

namespace Baikal
{
    VkwShadowController::VkwShadowController(VkDevice device, vkw::MemoryManager& memory_manager,
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
        , view_proj_buffer_(VK_NULL_HANDLE)
        , graphics_queue_index_(graphics_queue_index)
        , compute_queue_index_(compute_queue_index)
        , shadowmap_width_(4096)
        , shadowmap_height_(4096)

    {
        command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, graphics_queue_index_));

        nearest_sampler_ = utils_.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        linear_sampler_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

        vkGetDeviceQueue(device_, graphics_queue_index, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, compute_queue_index, 0, &compute_queue_);

        shadowmap_shader_ = shader_manager_.CreateShader("../Baikal/Kernels/VK/depth_only.vert.spv", "../Baikal/Kernels/VK/depth_only.frag.spv");

        viewport_   = vkw::Utils::CreateViewport(static_cast<float>(shadowmap_width_), static_cast<float>(shadowmap_height_), 0.f, 1.f);
        scissor_    = { { 0, 0 }, { shadowmap_width_, shadowmap_height_ } };

        shadow_attachments = {
            { shadowmap_width_, shadowmap_height_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT }
        };
    }

    void VkwShadowController::BuildCommandBuffer(uint32_t shadow_map_idx, uint32_t view_proj_light_idx, VkwScene const& scene)
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 1.0f, 0.f }
        };

        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmap_pipeline_.pipeline.get());

        VkDeviceSize offsets[1] = { 0 };

        VkBuffer vb = scene.mesh_vertex_buffer.get();
        command_buffer_builder_->BeginRenderPass(clear_values, scene.shadows[shadow_map_idx]);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport_);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, scene.mesh_index_buffer.get(), 0, VK_INDEX_TYPE_UINT32);

        for (size_t transform_pass = 0; transform_pass < scene.mesh_transforms.size(); transform_pass++)
        {
            VkDescriptorSet desc_set = shadowmap_descriptor_sets_[transform_pass].descriptor_set.get();
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmap_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

            size_t max_range = std::min((transform_pass + 1) * kMaxTransforms, scene.meshes.size());
            size_t transform_id = 0;

            for (size_t mesh_id = transform_pass * kMaxTransforms; mesh_id < max_range; mesh_id++)
            {
                VkwScene::VkwMesh const& mesh = scene.meshes[mesh_id];

                uint32_t data[4] = { static_cast<uint32_t>(transform_id), view_proj_light_idx, 0, 0 };
                vkCmdPushConstants(command_buffer, shadowmap_pipeline_.layout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(data), &data);
                vkCmdDrawIndexed(command_buffer, mesh.index_count, 1, mesh.index_base, 0, 0);

                transform_id++;
                transform_id = transform_id % kMaxTransforms;
            }
        }

        command_buffer_builder_->EndRenderPass();

        shadowmap_cmd_[shadow_map_idx] = command_buffer_builder_->EndCommandBuffer();
    }

    void VkwShadowController::BuildDirectionalLightCommandBuffer(uint32_t shadow_map_idx, uint32_t& light_idx, VkwScene const& scene)
    {
        static std::vector<VkClearValue> clear_values =
        {
            { 1.0f, 0.f }
        };

        command_buffer_builder_->BeginCommandBuffer();

        VkCommandBuffer command_buffer = command_buffer_builder_->GetCurrentCommandBuffer();

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmap_pipeline_.pipeline.get());

        VkDeviceSize offsets[1] = { 0 };

        VkBuffer vb = scene.mesh_vertex_buffer.get();
        command_buffer_builder_->BeginRenderPass(clear_values, scene.shadows[shadow_map_idx]);

        vkCmdSetScissor(command_buffer, 0, 1, &scissor_);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(command_buffer, scene.mesh_index_buffer.get(), 0, VK_INDEX_TYPE_UINT32);

        uint32_t half_shadowmap_width = shadowmap_width_ >> 1;
        uint32_t half_shadowmap_height = shadowmap_height_ >> 1;

        for (uint32_t cascade_idx = 0; cascade_idx < 4; cascade_idx++)
        {
            uint32_t x = cascade_idx % 2;
            uint32_t y = cascade_idx / 2;

            VkViewport viewport =
            {
                static_cast<float>(x * half_shadowmap_width),
                static_cast<float>(y * half_shadowmap_height),
                static_cast<float>(half_shadowmap_width),
                static_cast<float>(half_shadowmap_height),
                0.f, 1.f
            };

            vkCmdSetViewport(command_buffer, 0, 1, &viewport);

            for (size_t transform_pass = 0; transform_pass < scene.mesh_transforms.size(); transform_pass++)
            {
                VkDescriptorSet desc_set = shadowmap_descriptor_sets_[transform_pass].descriptor_set.get();
                vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmap_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

                size_t max_range = std::min((transform_pass + 1) * kMaxTransforms, scene.meshes.size());
                size_t transform_id = 0;

                for (size_t mesh_id = transform_pass * kMaxTransforms; mesh_id < max_range; mesh_id++)
                {
                    VkwScene::VkwMesh const& mesh = scene.meshes[mesh_id];

                    uint32_t data[4] = { static_cast<uint32_t>(transform_id), light_idx, 0, 0 };
                    vkCmdPushConstants(command_buffer, shadowmap_pipeline_.layout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(data), &data);
                    vkCmdDrawIndexed(command_buffer, mesh.index_count, 1, mesh.index_base, 0, 0);

                    transform_id++;
                    transform_id = transform_id % kMaxTransforms;
                }
            }


            light_idx++;
        }
        command_buffer_builder_->EndRenderPass();

        shadowmap_cmd_[shadow_map_idx] = command_buffer_builder_->EndCommandBuffer();
    }

    void VkwShadowController::UpdateShadowMap(uint32_t shadow_pass_idx, VkwScene& out)
    {
        VkCommandBuffer cmd_buffer = shadowmap_cmd_[shadow_pass_idx].get();

        VkSemaphore signal_semaphore = shadowmap_syncs_[shadow_pass_idx].get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pSignalSemaphores = &signal_semaphore;
        submit_info.signalSemaphoreCount = 1;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buffer;

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            throw std::runtime_error("VkwShadowController: queue submission failed");

        VkwScene::SemaphoreArray& shadow_semaphores = out.shadows_finished_signal;

        shadow_semaphores.push_back(shadowmap_syncs_[shadow_pass_idx]);
    }

    void VkwShadowController::CreateShadowRenderPipeline(VkRenderPass render_pass)
    {
        std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;

        for (size_t i = 0; i < shadow_attachments.size(); i++)
        {
            if ((shadow_attachments[i].usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
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
        rasterization_state.cullMode = VK_CULL_MODE_NONE;
        rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization_state.flags = 0;
        rasterization_state.depthClampEnable = VK_TRUE;
        rasterization_state.lineWidth = 1.0f;

        vkw::GraphicsPipelineState pipeline_state;
        pipeline_state.color_blend_state = &color_blend_state;
        pipeline_state.rasterization_state = &rasterization_state;

        shadowmap_pipeline_ = pipeline_manager_.CreateGraphicsPipeline(shadowmap_shader_, render_pass, &pipeline_state);
    }

    matrix ortho_proj_rh_vulkan(float l, float r, float b, float t, float n, float f)
    {
        return matrix(2.f / (r - l), 0, 0, -(r + l) / (r - l),
                      0, 2.f / (t - b), 0, -(t + b) / (t - b),
                      0, 0, -1.f / (f - n), n / (f - n),
                      0, 0, 0, 1);
    }

    void VkwShadowController::GenerateShadowViewProjForCascadeSlice(uint32_t cascade_idx, PerspectiveCamera const& camera, DirectionalLight const& light, RadeonRays::matrix& view_proj)
    {
        std::vector<RadeonRays::float4> frustum_corners_ws =
        {
            RadeonRays::float4(-1.0f,  1.0f, 1.0f, 1.0f),
            RadeonRays::float4(1.0f,  1.0f, 1.0f, 1.0f),
            RadeonRays::float4(1.0f, -1.0f, 1.0f, 1.0f),
            RadeonRays::float4(-1.0f, -1.0f, 1.0f, 1.0f),
            RadeonRays::float4(-1.0f,  1.0f, 0.0f, 1.0f),
            RadeonRays::float4(1.0f,  1.0f, 0.0f, 1.0f),
            RadeonRays::float4(1.0f, -1.0f, 0.0f, 1.0f),
            RadeonRays::float4(-1.0f, -1.0f, 0.0f, 1.0f),
        };

        const matrix proj = MakeProjectionMatrix(camera);
        const matrix view = MakeViewMatrix(camera);
        const matrix inv_view_proj = inverse(proj * view);

        RadeonRays::float3 frustum_center;

        // Transform to world space and find frustum center
        for (auto& v : frustum_corners_ws)
        {
            v = inv_view_proj * v;
            v = RadeonRays::float4(v.x / v.w, v.y / v.w, v.z / v.w, 0.0f);
        }

        float prev_slice = cascade_idx == 0 ? 0.f : 2.0f * split_dists_[cascade_idx - 1];
        float slice = 2.0f * split_dists_[cascade_idx];

        // Find frustum world space corners of current cascade slice
        for (uint32_t corner_idx = 0; corner_idx < 4; corner_idx++)
        {
            RadeonRays::float3 ray = frustum_corners_ws[corner_idx + 4] - frustum_corners_ws[corner_idx];
            RadeonRays::float3 near_ray = ray * prev_slice;
            RadeonRays::float3 far_ray = ray * slice;

            frustum_corners_ws[corner_idx + 4] = frustum_corners_ws[corner_idx] + far_ray;
            frustum_corners_ws[corner_idx] = frustum_corners_ws[corner_idx] + near_ray;
        }

        for (auto& v : frustum_corners_ws)
        {
            frustum_center += v;
        }
        frustum_center /= 8.0f;
        
        // Find cascade slice bound sphere
        float sphere_radius = 0.0f;
        for (auto& v : frustum_corners_ws)
        {
            float d = std::sqrt((v - frustum_center).sqnorm());
            sphere_radius = std::max(d, sphere_radius);
        }

        // Stabilize sphere radius
        sphere_radius = std::ceil(sphere_radius / (cascade_idx + 2)) * ((cascade_idx + 2));

        RadeonRays::float3 max_extents = RadeonRays::float3(sphere_radius, sphere_radius, sphere_radius);
        RadeonRays::float3 min_extents = -max_extents;

        RadeonRays::float3 extents = max_extents - min_extents;

        RadeonRays::float3 light_dir = light.GetDirection();
        RadeonRays::float3 shadow_camera_pos = frustum_center - light_dir * (-min_extents.z);

        RadeonRays::matrix shadow_proj = ortho_proj_rh_vulkan(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.f, extents.z);
        RadeonRays::matrix shadow_view = lookat_lh_dx(shadow_camera_pos, shadow_camera_pos - light_dir, RadeonRays::float3(0.f, 1.0f, 0.f));

        view_proj = shadow_proj * shadow_view;

        uint32_t width = shadowmap_width_ >> 1;
        uint32_t height = shadowmap_height_ >> 1;

        // Stabilize shadows
        RadeonRays::float4 shadow_origin = RadeonRays::float4(0.f, 0.f, 0.f, 1.f);
        shadow_origin = view_proj * shadow_origin;
        shadow_origin = shadow_origin * RadeonRays::float3(static_cast<float>(width >> 1), static_cast<float>(height >> 1), 1.f, 1.f);

        RadeonRays::float3 rounded_origin;
        rounded_origin.x = roundf(shadow_origin.x);
        rounded_origin.y = roundf(shadow_origin.y);
        rounded_origin.z = roundf(shadow_origin.z);
        rounded_origin.w = roundf(shadow_origin.w);

        RadeonRays::float3 round_offset = rounded_origin - shadow_origin;
        round_offset = round_offset * RadeonRays::float3(2.0f / static_cast<float>(width), 2.f / static_cast<float>(height), 0.f, 0.f);
        
        shadow_proj.m[0][3] += round_offset.x;
        shadow_proj.m[1][3] += round_offset.y;

        view_proj = shadow_proj * shadow_view;
    }

    void VkwShadowController::UpdateShadows(bool geometry_changed, bool camera_changed, std::vector<bool> const& lights_changed, Scene1 const& scene, VkwScene& out)
    {
        uint32_t num_lights = static_cast<uint32_t>(scene.GetNumLights());

        std::vector<RadeonRays::matrix> spot_light_view_proj;
        std::vector<RadeonRays::matrix> directional_light_view_proj;
        std::vector<RadeonRays::matrix> lights_view_proj;

        spot_light_view_proj.reserve(static_cast<size_t>(num_lights));
        directional_light_view_proj.reserve(static_cast<size_t>(num_lights) * 4);

        std::unique_ptr<Iterator> light_iter(scene.CreateLightIterator());

        PerspectiveCamera* camera = dynamic_cast<PerspectiveCamera*>(scene.GetCamera().get());

        if (camera == nullptr)
        {
            throw std::runtime_error("VkwSceneController supports only perspective camera");
        }

        uint32_t num_directional_lights = 0;
        uint32_t num_spot_lights = 0;
        uint32_t num_point_lights = 0;
        
        uint32_t global_light_idx = 0;

        for (; light_iter->IsValid(); light_iter->Next())
        {
            auto light = light_iter->ItemAs<Light>();
            auto light_type = GetLightType(*light);
            
            switch (light_type)
            {
                case kPoint:
                {
                    num_point_lights++;
                    // not supported yet
                    break;
                };

                case kDirectional:
                {
                    auto directional_light = dynamic_cast<DirectionalLight*>(light.get());

                    RadeonRays::matrix view_proj;

                    for (uint32_t cascade_idx = 0; cascade_idx < 4; cascade_idx++)
                    {
                        GenerateShadowViewProjForCascadeSlice(cascade_idx, *camera, *directional_light, view_proj);

                        directional_light_view_proj.push_back(view_proj);
                        lights_view_proj.push_back(view_proj);
                    }

                    num_directional_lights++;

                    break;
                };

                case kSpot:
                {
                    if (!geometry_changed || !lights_changed[global_light_idx])
                        break;

                    float3 p = light->GetPosition();
                    float3 d = light->GetDirection();
                    float3 r = light->GetEmittedRadiance();

                    auto cone_shape = static_cast<SpotLight const&>(*light).GetConeShape();

                    const float max_emitted_radiance = std::max(std::max(r.x, r.y), r.z);
                    const float shadow_clip_threshold = 0.01f;
                    const float near_plane = 0.1f;
                    const float far_plane = std::sqrt(std::abs(max_emitted_radiance / shadow_clip_threshold));
                    
                    const matrix view = lookat_lh_dx(p, p - d * 2.0f, float3(0.f, 1.f, 0.f));
                    const matrix proj = PerspectiveProjFovyRhVulkan(acos(cone_shape.y), 1.0f, near_plane, far_plane);

                    spot_light_view_proj.push_back(proj * view);
                    lights_view_proj.push_back(proj * view);
                    num_spot_lights++;

                    break;
                };

                case kIbl:
                {                   
                    // not supported yet
                    lights_view_proj.push_back(RadeonRays::matrix());
                    break;
                };

                case kArea:
                {
                    // not supported yet
                    lights_view_proj.push_back(RadeonRays::matrix());
                    break;
                };

                default:
                {
                    break;
                };
            }

            global_light_idx++;
        }

        if (out.point_lights_transforms == VK_NULL_HANDLE || out.num_point_lights < num_point_lights)
        {
            uint32_t num_lights = std::max(num_point_lights, 1u);

            out.point_lights_transforms.reset();
            out.point_lights_transforms = memory_manager_.CreateBuffer(6 * sizeof(RadeonRays::matrix) * num_lights,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        }


        if (out.spot_lights_transforms == VK_NULL_HANDLE || out.num_spot_lights < num_spot_lights)
        {           
            uint32_t num_lights = std::max(num_spot_lights, 1u);

            out.spot_lights_transforms.reset();
            out.spot_lights_transforms = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix) * num_lights,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        }

        if (out.directional_lights_transforms == VK_NULL_HANDLE || out.num_directional_lights < num_directional_lights)
        {
            uint32_t num_lights = std::max(num_directional_lights, 1u);

            out.directional_lights_transforms.reset();
            out.directional_lights_transforms = memory_manager_.CreateBuffer(4 * sizeof(RadeonRays::matrix) * num_lights,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        }

        if (out.mesh_vertex_buffer == VK_NULL_HANDLE || out.mesh_index_buffer.get() == VK_NULL_HANDLE)
            return;

        if (view_proj_buffer_ == VK_NULL_HANDLE || out.light_count < num_lights)
        {
            view_proj_buffer_.reset();

            if (lights_view_proj.empty())
            {
                lights_view_proj.push_back(RadeonRays::matrix());
            }

            view_proj_buffer_ = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix) * lights_view_proj.size(),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, lights_view_proj.data());
        }

        if (num_spot_lights > 0)
            memory_manager_.WriteBuffer(out.spot_lights_transforms.get(), 0u, sizeof(RadeonRays::matrix) * spot_light_view_proj.size(), spot_light_view_proj.data());

        if (num_directional_lights > 0)
            memory_manager_.WriteBuffer(out.directional_lights_transforms.get(), 0u, sizeof(RadeonRays::matrix) * directional_light_view_proj.size(), directional_light_view_proj.data());

        memory_manager_.WriteBuffer(view_proj_buffer_.get(), 0u, sizeof(RadeonRays::matrix) * lights_view_proj.size(), lights_view_proj.data());

        uint32_t shadow_map_count = static_cast<uint32_t>(out.shadows.size());
        
        while (shadow_map_count < num_lights)
        {
            out.shadows.push_back(render_target_manager_.CreateRenderTarget(shadow_attachments));           
            shadowmap_syncs_.push_back(utils_.CreateSemaphore());

            shadow_map_count++;
        }

        if (shadow_map_count > num_lights)
        {
            out.shadows.resize(num_lights);
        }

        if (!out.shadows.empty() && shadowmap_pipeline_.pipeline == VK_NULL_HANDLE)
        {
            CreateShadowRenderPipeline(out.shadows[0].render_pass.get());
        }

        out.cascade_splits_dist = RadeonRays::float4(split_dists_[0], split_dists_[1], split_dists_[2], split_dists_[3]);
        out.cascade_splits_dist *= camera->GetDepthRange().x;
        out.cascade_splits_dist.w *= camera->GetDepthRange().x;

        if (shadowmap_descriptor_sets_.size() < out.mesh_transforms.size())
        {
            shadowmap_descriptor_sets_.resize(out.mesh_transforms.size());

            for (size_t i = 0; i < shadowmap_descriptor_sets_.size(); i++)
            {
                shadowmap_descriptor_sets_[i] = shader_manager_.CreateDescriptorSet(shadowmap_shader_);
                shadowmap_descriptor_sets_[i].SetArg(0, view_proj_buffer_.get());
                shadowmap_descriptor_sets_[i].SetArg(1, out.mesh_transforms[i].get());
                shadowmap_descriptor_sets_[i].CommitArgs();
            }
        }

        uint32_t cmd_buf_count = static_cast<uint32_t>(shadowmap_cmd_.size());

        while (cmd_buf_count < num_lights)
        {
            shadowmap_cmd_.push_back(vkw::CommandBuffer());

            BuildCommandBuffer(cmd_buf_count++, 0, out);
        }

        uint32_t light_idx = 0;
        uint32_t shadow_map_idx = 0;
        uint32_t view_proj_light_idx = 0;

        for (auto light_iter = scene.CreateLightIterator(); light_iter->IsValid(); light_iter->Next())
        {
            auto light = light_iter->ItemAs<Light>();
            auto light_type = GetLightType(*light);

            if (light_type == kArea || light_type == kIbl)
            {
                view_proj_light_idx++;
            }

            if (light_type == kSpot)
            {
                if (geometry_changed || lights_changed[light_idx])
                {
                    if (geometry_changed)
                    {
                        BuildCommandBuffer(shadow_map_idx, view_proj_light_idx, out);
                    }

                    UpdateShadowMap(shadow_map_idx, out);

                    view_proj_light_idx++;
                }
            }

            if (light_type == kDirectional && camera_changed)
            {
                BuildDirectionalLightCommandBuffer(shadow_map_idx, view_proj_light_idx, out);
                UpdateShadowMap(shadow_map_idx, out);
            }

            shadow_map_idx++;
            light_idx++;
        }
    }
}
