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

#include <vulkan/vulkan.h>
#include "GLFW/glfw3.h"

#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_glfw_vulkan.h"

#include "Application/vk_application.h"
#include "Application/vk_render.h"

using namespace RadeonRays;

namespace Baikal
{
    void VkApplication::SaveToFile(std::chrono::high_resolution_clock::time_point time) const
    {
        // TODO: implement
        assert(0);
    }

    void VkApplication::FindSuitableWindowSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceFormatKHR& window_surface_format)
    {
        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface_, &format_count, NULL);

        if (format_count == 0)
        {
            throw std::runtime_error("No supported swapchain surface formats");
        }

        std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface_, &format_count, surface_formats.data());

        if (format_count == 1)
        {
            // VK_FORMAT_UNDEFINED means that any format is available
            if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
            {
                window_surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
                window_surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            }
            else
            {
                window_surface_format = surface_formats[0];
            }
        }
        else
        {
            std::vector<VkFormat> request_surface_format = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
            VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

            bool found = false;

            for (auto format : request_surface_format)
            {
                if (found)
                    break;

                for (uint32_t j = 0; j < format_count; j++)
                {
                    if (surface_formats[j].format == format && surface_formats[j].colorSpace == request_surface_color_space)
                    {
                        window_surface_format = surface_formats[j];
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                window_surface_format = surface_formats[0];
            }
        }
    }

    void VkApplication::FindPresentMode(VkPhysicalDevice physical_device, VkPresentModeKHR& present_mode)
    {
        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface_, &present_mode_count, nullptr);

        std::vector<VkPresentModeKHR> present_modes(present_mode_count);

        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface_, &present_mode_count, present_modes.data());

        bool found = false;

        present_mode = default_present_mode_;

        for (auto mode : present_modes)
        {
            if (mode == default_present_mode_)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            present_mode = VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    void VkApplication::ResizeSwapChain(VkDevice device, VkPhysicalDevice physical_device, int width, int height)
    {
        VkSwapchainKHR old_swapchain = swapchain_;

        vkDeviceWaitIdle(device);

        for (uint32_t i = 0; i < back_buffer_count_; i++)
        {
            vkDestroyImageView(device, back_buffer_views_[i], nullptr);
            vkDestroyFramebuffer(device, framebuffers_[i], nullptr);
        }

        if (render_pass_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device, render_pass_, nullptr);
        }

        VkSwapchainCreateInfoKHR swap_chain_create_info = {};
        swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swap_chain_create_info.surface = window_surface_;
        swap_chain_create_info.imageFormat = window_surface_format_.format;
        swap_chain_create_info.imageColorSpace = window_surface_format_.colorSpace;
        swap_chain_create_info.imageArrayLayers = 1;
        swap_chain_create_info.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swap_chain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swap_chain_create_info.presentMode = window_present_mode_;
        swap_chain_create_info.clipped = VK_TRUE;
        swap_chain_create_info.oldSwapchain = old_swapchain;

        VkSurfaceCapabilitiesKHR caps;

        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, window_surface_, &caps) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Can't get physical device surface caps");

        VkBool32 supported;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, 0, window_surface_, &supported);

        if (supported != VK_TRUE)
            throw std::runtime_error("VkApplication: Error no WSI support on physical device");

        if (caps.maxImageCount > 0)
            swap_chain_create_info.minImageCount = (caps.minImageCount + 2 < caps.maxImageCount) ? (caps.minImageCount + 2) : caps.maxImageCount;
        else
            swap_chain_create_info.minImageCount = caps.minImageCount + 2;

        if (caps.currentExtent.width == 0xffffffff)
        {
            framebuffer_width_ = width;
            framebuffer_height_ = height;

            swap_chain_create_info.imageExtent.width = framebuffer_width_;
            swap_chain_create_info.imageExtent.height = framebuffer_height_;
        }
        else
        {
            framebuffer_width_ = caps.currentExtent.width;
            framebuffer_height_ = caps.currentExtent.height;

            swap_chain_create_info.imageExtent.width = framebuffer_width_;
            swap_chain_create_info.imageExtent.height = framebuffer_height_;
        }

        if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swapchain_) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Can't create swapchain");

        if (vkGetSwapchainImagesKHR(device, swapchain_, &back_buffer_count_, NULL) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Can't retrieve swapchain image count");

        if (vkGetSwapchainImagesKHR(device, swapchain_, &back_buffer_count_, back_buffer_images_) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Can't retrieve swapchain images");

        if (old_swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, old_swapchain, nullptr);

        VkAttachmentDescription attachment = {};
        attachment.format = window_surface_format_.format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &attachment;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass_) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Can't create render pass");

        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = window_surface_format_.format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
        image_view_create_info.subresourceRange = image_range;

        for (uint32_t i = 0; i < back_buffer_count_; i++)
        {
            image_view_create_info.image = back_buffer_images_[i];

            if (vkCreateImageView(device, &image_view_create_info, nullptr, &back_buffer_views_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication: Can't create image view");
        }

        VkImageView view_attachment[1];

        VkFramebufferCreateInfo frame_buffer_create_info = {};
        frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_create_info.renderPass = render_pass_;
        frame_buffer_create_info.attachmentCount = 1;
        frame_buffer_create_info.pAttachments = view_attachment;
        frame_buffer_create_info.width = framebuffer_width_;
        frame_buffer_create_info.height = framebuffer_height_;
        frame_buffer_create_info.layers = 1;

        for (uint32_t i = 0; i < back_buffer_count_; i++)
        {
            view_attachment[0] = back_buffer_views_[i];

            if (vkCreateFramebuffer(device, &frame_buffer_create_info, nullptr, &framebuffers_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication: Can't create image view");
        }
    }

    void VkApplication::PrepareFrame(VkDevice device, VkPhysicalDevice physical_device)
    {
        for (;;)
        {
            VkResult r = vkWaitForFences(device, 1, &fences_[frame_idx_], VK_TRUE, std::numeric_limits<uint64_t>::max());

            if (r == VK_SUCCESS) break;
            if (r == VK_TIMEOUT) continue;
        }


        vkResetFences(device, 1, &fences_[frame_idx_]);

        VkResult r = vkAcquireNextImageKHR(device, swapchain_, UINT64_MAX, present_semaphores_[frame_idx_], VK_NULL_HANDLE, &back_buffer_indices_[frame_idx_]);

        if (r != VK_SUCCESS)
        {
            if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
            {
                ResizeSwapChain(device, physical_device, framebuffer_width_, framebuffer_height_);
            }
            else
            {
                throw std::runtime_error("VkApplication: Failed to acquire next KHR image");
            }
        }

        if (vkResetCommandPool(device, command_pools_[frame_idx_], 0))
            throw std::runtime_error("VkApplication: Failed to reset command pool");

        VkCommandBufferBeginInfo command_buffer_begin_info = {};
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(command_buffers_[frame_idx_], &command_buffer_begin_info) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Failed to begin command buffer");

        VkClearValue clear_value;
        clear_value.color.float32[0] = 1.0f;
        clear_value.color.float32[1] = 0.0f;
        clear_value.color.float32[2] = 0.0f;
        clear_value.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass_;
        render_pass_begin_info.framebuffer = framebuffers_[back_buffer_indices_[frame_idx_]];
        render_pass_begin_info.renderArea.extent.width = framebuffer_width_;
        render_pass_begin_info.renderArea.extent.height = framebuffer_height_;
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(command_buffers_[frame_idx_], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VkApplication::EndFrame(VkDevice device, VkQueue queue)
    {
        vkCmdEndRenderPass(command_buffers_[frame_idx_]);

        AppVkRender* vk_app_render = dynamic_cast<AppVkRender*>(app_render_.get());

        if (!vk_app_render)
            throw std::runtime_error("VkApplication: Internal error");

        VkwOutput* vk_output = dynamic_cast<VkwOutput*>(vk_app_render->GetRendererOutput());

        if (!vk_output)
            throw std::runtime_error("VkApplication: Internal error");

        VkSemaphore render_complete = dynamic_cast<VkwOutput*>(vk_app_render->GetRendererOutput())->GetSemaphore();
        VkPipelineStageFlags wait_stage[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSemaphore wait_semaphores[2] = { render_complete, present_semaphores_[frame_idx_] };

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 2;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers_[frame_idx_];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &render_complete_semaphores_[frame_idx_];

        if (vkEndCommandBuffer(command_buffers_[frame_idx_]) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Failed to end command buffer");

        if (vkQueueSubmit(queue, 1, &submit_info, fences_[frame_idx_]) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Failed to submit queue");
    }

    void VkApplication::PresentFrame(VkQueue queue)
    {
        uint32_t indices[1] = { back_buffer_indices_[frame_idx_] };

        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &render_complete_semaphores_[frame_idx_];
        info.swapchainCount = 1;
        info.pSwapchains = &swapchain_;
        info.pImageIndices = indices;

        if (vkQueuePresentKHR(queue, &info) != VK_SUCCESS)
            throw std::runtime_error("VkApplication: Failed PresentFrame");

        frame_idx_ = (frame_idx_ + 1) % num_queued_frames_;
    }

    VkApplication::VkApplication(int argc, char * argv[])
    : Application()
    , frame_idx_(0)
    , framebuffer_width_(0)
    , framebuffer_height_(0)
    , window_surface_(VK_NULL_HANDLE)
    , swapchain_(VK_NULL_HANDLE)
    , render_pass_(VK_NULL_HANDLE)
    , default_present_mode_(VK_PRESENT_MODE_MAILBOX_KHR)
    , back_buffer_count_(0)
    {
        if (glfwInit() != GL_TRUE)
        {
            std::cout << "GLFW initialization failed\n";
            exit(-1);
        }

        AppCliParser cli;
        m_settings = cli.Parse(argc, argv);

        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow(m_settings.width, m_settings.height, "Baikal standalone demo", NULL, NULL);

        if (!glfwVulkanSupported())
        {
            std::cout << "GLFW Vulkan is not supported\n";
            exit(-1);
        }

        try
        {
            std::uint32_t glfw_required_extensions_count;
            const char **glfw_required_extensions = glfwGetRequiredInstanceExtensions(&glfw_required_extensions_count);
            m_settings.vk_required_extensions.clear();
            for (auto a = 0U; a < glfw_required_extensions_count; ++a)
            {
                m_settings.vk_required_extensions.push_back(glfw_required_extensions[a]);
            }

            app_render_.reset(new AppVkRender(m_settings));

            glfwSetMouseButtonCallback(m_window, Application::OnMouseButton);
            glfwSetCursorPosCallback(m_window, Application::OnMouseMove);
            glfwSetKeyCallback(m_window, Application::OnKey);
            glfwSetScrollCallback(m_window, Application::OnMouseScroll);
        }
        catch (std::runtime_error& err)
        {
            glfwDestroyWindow(m_window);
            std::cout << err.what();
            exit(-1);
        }

        AppVkRender* vk_app_render = dynamic_cast<AppVkRender*>(app_render_.get());

        if (vk_app_render == nullptr)
        {
            throw std::runtime_error("VkApplication: Internal error");
        }

        if (glfwCreateWindowSurface(vk_app_render->GetInstance(), m_window, NULL, &window_surface_) != VK_SUCCESS)
        {
            throw std::runtime_error("VkApplication: Failed to create window surface");
        }

        FindSuitableWindowSurfaceFormat(vk_app_render->GetPhysicalDevice(), window_surface_format_);
        FindPresentMode(vk_app_render->GetPhysicalDevice(), window_present_mode_);

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);

        ResizeSwapChain(vk_app_render->GetDevice(), vk_app_render->GetPhysicalDevice(), framebuffer_width, framebuffer_height);

        vkw::ShaderManager& shader_manager = vk_app_render->GetShaderManager();
        vkw::PipelineManager& pipeline_manager = vk_app_render->GetPipelineManager();
        vkw::MemoryManager& memory_manager = vk_app_render->GetMemoryManager();
        vkw::Utils& utils = vk_app_render->GetUtils();

        sampler_ = utils.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

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

        fullscreen_quad_vb_ = memory_manager.CreateBuffer(4 * sizeof(Vertex),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertices);

        fullscreen_quad_ib_ = memory_manager.CreateBuffer(6 * sizeof(uint32_t),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indices);

        fsq_vert_shader_ = shader_manager.CreateShader(VK_SHADER_STAGE_VERTEX_BIT, "../Baikal/Kernels/VK/fullscreen_quad.vert.spv");
        output_frag_shader_ = shader_manager.CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../Baikal/Kernels/VK/output.frag.spv");
        output_pipeline_ = pipeline_manager.CreateGraphicsPipeline(fsq_vert_shader_, output_frag_shader_, render_pass_);

        Output* output = vk_app_render->GetRendererOutput();

        VkwOutput* vkw_output = dynamic_cast<VkwOutput*>(output);

        output_frag_shader_.SetArg(1, vkw_output->GetRenderTarget().attachments[0].view.get(), sampler_.get());
        output_frag_shader_.CommitArgs();

        for (uint32_t i = 0; i < num_queued_frames_; i++)
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            if (vkCreateSemaphore(vk_app_render->GetDevice(), &info, nullptr, &present_semaphores_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication:Failed to create semaphore");

            if (vkCreateSemaphore(vk_app_render->GetDevice(), &info, nullptr, &render_complete_semaphores_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication:Failed to create semaphore");

            VkFenceCreateInfo fence_info = {};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            if (vkCreateFence(vk_app_render->GetDevice(), &fence_info, nullptr, &fences_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication:Failed to create fence");

            VkCommandPoolCreateInfo command_pool_create_info = {};
            command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            command_pool_create_info.queueFamilyIndex = vk_app_render->GetGraphicsQueueFamilyIndex();
            command_pool_create_info.flags = 0;

            if (vkCreateCommandPool(vk_app_render->GetDevice(), &command_pool_create_info, nullptr, &command_pools_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication: Failed to create command pool");

            VkCommandBufferAllocateInfo command_buffers_create_info = {};
            command_buffers_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_buffers_create_info.commandPool = command_pools_[i];
            command_buffers_create_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_buffers_create_info.commandBufferCount = 1;

            if (vkAllocateCommandBuffers(vk_app_render->GetDevice(), &command_buffers_create_info, &command_buffers_[i]) != VK_SUCCESS)
                throw std::runtime_error("VkApplication: Failed to allocate command buffers");
        }
    }

    VkApplication::~VkApplication()
    {
        AppVkRender* vk_app_render = dynamic_cast<AppVkRender*>(app_render_.get());

        if (vk_app_render == nullptr)
        {
            std::cout << "VkApplication: Can't deallocate resources";
            exit(-1);
        }

        VkDevice    device = vk_app_render->GetDevice();
        VkInstance  instance = vk_app_render->GetInstance();

        for (uint32_t i = 0; i < back_buffer_count_; i++)
        {
            vkDestroyImageView(device, back_buffer_views_[i], nullptr);
            vkDestroyFramebuffer(device, framebuffers_[i], nullptr);
        }

        for (uint32_t i = 0; i < num_queued_frames_; i++)
        {
            vkDestroySemaphore(device, present_semaphores_[i], nullptr);
            vkDestroySemaphore(device, render_complete_semaphores_[i], nullptr);
            vkDestroyCommandPool(device, command_pools_[i], nullptr);
            vkDestroyFence(device, fences_[i], nullptr);
        }

        vkDestroyRenderPass(device, render_pass_, nullptr);
        vkDestroySwapchainKHR(device, swapchain_, nullptr);

        vkDestroySurfaceKHR(instance, window_surface_, nullptr);
    }

    void VkApplication::Run()
    {
        CollectSceneStats();

        try
        {
            AppVkRender* vk_app_render = dynamic_cast<AppVkRender*>(app_render_.get());

            if (vk_app_render == nullptr)
            {
                throw std::runtime_error("VkApplication: Internal error");
            }

            vk_app_render->StartRenderThreads();

            static bool update = true;

            VkQueue queue;
            vkGetDeviceQueue(vk_app_render->GetDevice(), vk_app_render->GetGraphicsQueueFamilyIndex(), 0, &queue);

            while (!glfwWindowShouldClose(m_window))
            {
                Update(update);

                PrepareFrame(vk_app_render->GetDevice(), vk_app_render->GetPhysicalDevice());

                vkCmdBindPipeline(command_buffers_[frame_idx_], VK_PIPELINE_BIND_POINT_GRAPHICS, output_pipeline_.pipeline.get());

                VkDeviceSize offsets[1] = { 0 };
                VkBuffer vb = fullscreen_quad_vb_.get();

                VkViewport viewport = vkw::Utils::CreateViewport(
                    static_cast<float>(framebuffer_width_),
                    static_cast<float>(framebuffer_height_),
                    0.f, 1.f);

                vkCmdSetViewport(command_buffers_[frame_idx_], 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.extent.width = framebuffer_width_;
                scissor.extent.height = framebuffer_height_;
                scissor.offset.x = 0;
                scissor.offset.y = 0;

                vkCmdSetScissor(command_buffers_[frame_idx_], 0, 1, &scissor);

                VkDescriptorSet desc_set = output_frag_shader_.descriptor_set.descriptor_set.get();
                vkCmdBindDescriptorSets(command_buffers_[frame_idx_], VK_PIPELINE_BIND_POINT_GRAPHICS, output_pipeline_.layout.get(), 0, 1, &desc_set, 0, NULL);

                vkCmdBindVertexBuffers(command_buffers_[frame_idx_], 0, 1, &vb, offsets);
                vkCmdBindIndexBuffer(command_buffers_[frame_idx_], fullscreen_quad_ib_.get(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(command_buffers_[frame_idx_], 6, 1, 0, 0, 0);

                EndFrame(vk_app_render->GetDevice(), queue);

                PresentFrame(queue);

                glfwPollEvents();
            }

            vk_app_render->StopRenderThreads();

            vkDeviceWaitIdle(vk_app_render->GetDevice());

            glfwDestroyWindow(m_window);
        }
        catch (std::runtime_error& err)
        {
            glfwDestroyWindow(m_window);
            std::cout << err.what();
            exit(-1);
        }
    }
}

