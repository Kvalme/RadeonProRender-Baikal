#include "vkw_probe_controller.h"

#include "vkw_scene_helpers.h"

#include "SceneGraph/vkwscene.h"
#include "SceneGraph/scene1.h"
#include "SceneGraph/camera.h"
#include "SceneGraph/light.h"
#include "SceneGraph/Collector/collector.h"

#include <algorithm>

namespace Baikal
{
    VkwProbeController::VkwProbeController(VkDevice device, vkw::MemoryManager& memory_manager,
        vkw::ShaderManager& shader_manager,
        vkw::RenderTargetManager& render_target_manager,
        vkw::PipelineManager& pipeline_manager,
        vkw::ExecutionManager& execution_manager,
        uint32_t graphics_queue_index,
        uint32_t compute_queue_index)
        : device_(device)
        , memory_manager_(memory_manager)
        , render_target_manager_(render_target_manager)
        , shader_manager_(shader_manager)
        , pipeline_manager_(pipeline_manager)
        , execution_manager_(execution_manager)
        , utils_(device_)
        , graphics_queue_index_(graphics_queue_index)
        , compute_queue_index_(compute_queue_index)

    {
        command_buffer_builder_.reset(new vkw::CommandBufferBuilder(device, compute_queue_index_));
        
        const uint32_t num_mips = static_cast<uint32_t>(floor(log2(env_map_size))) + 1;

        nearest_sampler_ = utils_.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        linear_sampler_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        linear_sampler_cube_map_ = utils_.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.f, static_cast<float>(num_mips));

        vkGetDeviceQueue(device_, graphics_queue_index, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, compute_queue_index, 0, &compute_queue_);

        uint32_t group_size = 8;
        uint32_t group_count_x = (env_map_size + group_size - 1) / group_size;
        uint32_t group_count_y = (env_map_size + group_size - 1) / group_size;

        convert_to_cubemap_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/convert_to_cubemap.comp.spv");
        convert_to_cubemap_pipeline_ = pipeline_manager_.CreateComputePipeline(convert_to_cubemap_shader_, group_count_x, group_count_y, 1);

        generate_cubemap_mips_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/generate_cube_mips.comp.spv");
        generate_cubemap_mips_pipeline_ = pipeline_manager_.CreateComputePipeline(generate_cubemap_mips_shader_, group_count_x, group_count_y, 1);

        project_cubemap_to_sh9_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/cubemap_sh9_project.comp.spv");
        project_cubemap_to_sh9_pipeline_ = pipeline_manager_.CreateComputePipeline(project_cubemap_to_sh9_shader_, group_count_x, group_count_y, 1);

        downsample_sh9_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/cubemap_sh9_downsample.comp.spv");
        downsample_sh9_pipeline_ = pipeline_manager_.CreateComputePipeline(downsample_sh9_shader_, group_count_x, group_count_y, 1);

        final_sh9_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/cubemap_sh9_final.comp.spv");
        final_sh9_pipeline_ = pipeline_manager_.CreateComputePipeline(final_sh9_shader_, 1, 1, 1);

        prefilter_reflections_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/prefilter_reflections.comp.spv");
        prefilter_reflections_pipeline_ = pipeline_manager_.CreateComputePipeline(prefilter_reflections_shader_, group_count_x, group_count_y, 1);

        VkExtent3D extent = { env_map_size, env_map_size, 1 };
        env_cube_map_.SetCubeTexture(&memory_manager_, extent, VK_FORMAT_R16G16B16A16_SFLOAT, true);

        convert_to_cubemap_shader_.SetArg(1, env_cube_map_.GetImageView(), nearest_sampler_.get());
        convert_to_cubemap_shader_.CommitArgs();

        const uint32_t num_cube_faces = 6;

        sh9_buffers_.resize(num_mips);
        sh9_semaphores_.resize(num_mips);

        for (uint32_t i = 0; i < num_mips; i++)
        {
            uint32_t width = env_map_size >> i;
            uint32_t height = env_map_size >> i;

            uint32_t buffer_size = num_cube_faces * width * height * sizeof(VkSH9Color);

            sh9_buffers_[i] = memory_manager_.CreateBuffer(buffer_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            sh9_semaphores_[i] = utils_.CreateSemaphore();
        }
    }

    void VkwProbeController::PrefilterEnvMap(VkwScene& out)
    {
        convert_to_cubemap_shader_.SetArg(0, out.textures[out.env_map_idx].GetImageView(), nearest_sampler_.get());
        convert_to_cubemap_shader_.CommitArgs();
               
        const uint32_t num_mips = static_cast<uint32_t>(floor(log2(env_map_size))) + 1;
        
        uint32_t group_size = 8;
        uint32_t group_count_x = (env_map_size + group_size - 1) / group_size;
        uint32_t group_count_y = (env_map_size + group_size - 1) / group_size;
        
        command_buffer_builder_->BeginCommandBuffer();
        command_buffer_builder_->Dispatch(convert_to_cubemap_pipeline_, convert_to_cubemap_shader_, group_count_x, group_count_y, 6);
        vkw::CommandBuffer command_buffer = command_buffer_builder_->EndCommandBuffer();
        
        execution_manager_.Submit(command_buffer.get());
        execution_manager_.WaitIdle();
        
        for (uint32_t i = 0; i < num_mips - 1; i++)
        {
            uint32_t next_mip_size = env_map_size >> (i + 1);
            uint32_t group_count_x = (next_mip_size + group_size - 1) / group_size;
            uint32_t group_count_y = (next_mip_size + group_size - 1) / group_size;

            generate_cubemap_mips_shader_.SetArg(0, env_cube_map_.GetImageView(i), linear_sampler_.get());
            generate_cubemap_mips_shader_.SetArg(1, env_cube_map_.GetImageView(i + 1), linear_sampler_.get());
            generate_cubemap_mips_shader_.CommitArgs();

            command_buffer_builder_->BeginCommandBuffer();
            command_buffer_builder_->Dispatch(generate_cubemap_mips_pipeline_, generate_cubemap_mips_shader_, group_count_x, group_count_y, 6);
            vkw::CommandBuffer command_buffer = command_buffer_builder_->EndCommandBuffer();

            execution_manager_.Submit(command_buffer.get());
            execution_manager_.WaitIdle();
        }

        project_cubemap_to_sh9_shader_.SetArg(0, env_cube_map_.GetImageView(), nearest_sampler_.get());
        project_cubemap_to_sh9_shader_.SetArg(1, sh9_buffers_[0].get());
        project_cubemap_to_sh9_shader_.CommitArgs();
        
        command_buffer_builder_->BeginCommandBuffer();
        command_buffer_builder_->Dispatch(project_cubemap_to_sh9_pipeline_, project_cubemap_to_sh9_shader_, group_count_x, group_count_y, 6);
        command_buffer = command_buffer_builder_->EndCommandBuffer();
        
        execution_manager_.Submit(command_buffer.get());
        execution_manager_.WaitIdle();
        
        for (uint32_t i = 0; i < num_mips - 1; i++)
        {
            const uint32_t src_tex_width = env_map_size >> i;
            const uint32_t src_tex_height = env_map_size >> i;
            const uint32_t dst_tex_width = env_map_size >> (i + 1);
            const uint32_t dst_tex_height = env_map_size >> (i + 1);
        
            downsample_sh9_shader_.SetArg(0, sh9_buffers_[i].get());
            downsample_sh9_shader_.SetArg(1, sh9_buffers_[i + 1].get());
            downsample_sh9_shader_.CommitArgs();
        
            uint32_t group_size = 2;
            uint32_t group_count_x = (dst_tex_width + group_size - 1) / group_size;
            uint32_t group_count_y = (dst_tex_height + group_size - 1) / group_size;
        
            uint32_t push_constants[4] = { src_tex_width, src_tex_height, dst_tex_width, dst_tex_height };
        
            command_buffer_builder_->BeginCommandBuffer();
            command_buffer_builder_->Dispatch(downsample_sh9_pipeline_, downsample_sh9_shader_, group_count_x, group_count_y, 6, sizeof(push_constants), push_constants);
            command_buffer = command_buffer_builder_->EndCommandBuffer();
        
            execution_manager_.Submit(command_buffer.get());
            execution_manager_.WaitIdle();
        }
        
        if (out.env_map_irradiance_sh9 == VK_NULL_HANDLE)
        {
            out.env_map_irradiance_sh9.reset();
            out.env_map_irradiance_sh9 = memory_manager_.CreateBuffer(sizeof(VkSH9Color), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        }
        
        final_sh9_shader_.SetArg(0, sh9_buffers_[num_mips - 1].get());
        final_sh9_shader_.SetArg(1, out.env_map_irradiance_sh9.get());
        final_sh9_shader_.CommitArgs();
        
        uint32_t probe_idx = 0;
        uint32_t push_constants = probe_idx;
        
        command_buffer_builder_->BeginCommandBuffer();
        command_buffer_builder_->Dispatch(final_sh9_pipeline_, final_sh9_shader_, 1, 1, 1, sizeof(push_constants), &push_constants);
        command_buffer = command_buffer_builder_->EndCommandBuffer();
        
        execution_manager_.Submit(command_buffer.get());
        execution_manager_.WaitIdle();

        if (out.ibl_skylight_reflections.GetImageView() == VK_NULL_HANDLE)
        {
            VkExtent3D extent = { env_map_size, env_map_size, 1 };
            out.ibl_skylight_reflections.SetCubeTexture(&memory_manager_, extent, VK_FORMAT_R16G16B16A16_SFLOAT, true);
        }

        std::vector<VkImageView> env_map_mips;
        env_map_mips.resize(num_mips);

        for (uint32_t i = 0; i < num_mips; i++)
        {
            env_map_mips[i] = env_cube_map_.GetImageView(i);
        }

        struct PushConstsStruct
        {
            uint32_t mip;
            uint32_t max_mips;
            float roughness;
        };

        prefilter_reflections_shader_.SetArgArray(0, env_map_mips, linear_sampler_.get());
        prefilter_reflections_shader_.SetArg(1, env_cube_map_.GetImageView(), linear_sampler_cube_map_.get());

        for (uint32_t mip = 0; mip < num_mips; mip++)
        {
            const uint32_t width = env_map_size >> mip;
            const uint32_t height = env_map_size >> mip;

            const uint32_t group_size = 8;
            const uint32_t group_count_x = (width + group_size - 1) / group_size;
            const uint32_t group_count_y = (height + group_size - 1) / group_size;

            prefilter_reflections_shader_.SetArg(2, out.ibl_skylight_reflections.GetImageView(mip), linear_sampler_.get());
            prefilter_reflections_shader_.CommitArgs();

            float roughness = (float)mip / (float)(num_mips - 1);
            PushConstsStruct push_constants = { mip, num_mips, roughness };

            command_buffer_builder_->BeginCommandBuffer();
            command_buffer_builder_->Dispatch(prefilter_reflections_pipeline_, prefilter_reflections_shader_, group_count_x, group_count_y, 6, sizeof(push_constants), &push_constants);
            command_buffer = command_buffer_builder_->EndCommandBuffer();

            execution_manager_.Submit(command_buffer.get());
            execution_manager_.WaitIdle();
        }

        // Generate brdf lut
        if (out.ibl_brdf_lut.GetImageView() == VK_NULL_HANDLE)
        {
            generate_brdf_lut_shader_ = shader_manager_.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT, "../Baikal/Kernels/VK/generate_brdf_lut.comp.spv");
            generate_brdf_lut_pipeline_ = pipeline_manager_.CreateComputePipeline(generate_brdf_lut_shader_, group_count_x, group_count_y, 1);

            uint32_t brdf_lut_size = 512;
            VkExtent3D brdf_lut_extent = { brdf_lut_size, brdf_lut_size, 1 };
            out.ibl_brdf_lut.SetTexture(&memory_manager_, brdf_lut_extent, VK_FORMAT_R16G16B16A16_SFLOAT);

            generate_brdf_lut_shader_.SetArg(0, out.ibl_brdf_lut.GetImageView(), nearest_sampler_.get());
            generate_brdf_lut_shader_.CommitArgs();

            uint32_t group_count_x = (brdf_lut_size + group_size - 1) / group_size;
            uint32_t group_count_y = (brdf_lut_size + group_size - 1) / group_size;

            command_buffer_builder_->BeginCommandBuffer();
            command_buffer_builder_->Dispatch(generate_brdf_lut_pipeline_, generate_brdf_lut_shader_, group_count_x, group_count_y, 1);
            vkw::CommandBuffer command_buffer = command_buffer_builder_->EndCommandBuffer();

            execution_manager_.Submit(command_buffer.get());
            execution_manager_.WaitIdle();
        }
    }
}