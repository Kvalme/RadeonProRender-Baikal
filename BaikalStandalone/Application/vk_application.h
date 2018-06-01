
/**********************************************************************
 Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.

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

#include "Application/application.h"
#include "Application/app_utils.h"
#include "Application/vk_render.h"

#include "image_io.h"

#include <future>
#include <memory>
#include <chrono>

namespace Baikal
{
    class VkApplication : public Application
    {
    public:
        VkApplication(int argc, char * argv[]);
        virtual ~VkApplication();
        
        void Run();
    private:
        void SaveToFile(std::chrono::high_resolution_clock::time_point time) const;
        void FindSuitableWindowSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceFormatKHR& window_surface_format);
        void FindPresentMode(VkPhysicalDevice physical_device, VkPresentModeKHR& present_mode);
        void ResizeSwapChain(VkDevice device, VkPhysicalDevice physical_device, int width, int height);

        void PrepareFrame(VkDevice device, VkPhysicalDevice physical_device);
        void EndFrame(VkDevice device, VkQueue queue);
        void PresentFrame(VkQueue queue);
    private:
        static const uint32_t       num_queued_frames_ = 16;
        static const uint32_t       num_max_back_buffers = 16;
        
        uint32_t                    frame_idx_;
        uint32_t                    back_buffer_indices_[num_queued_frames_];

        uint32_t                    framebuffer_width_;
        uint32_t                    framebuffer_height_;

        VkSurfaceKHR                window_surface_;
        VkSurfaceFormatKHR          window_surface_format_;
        VkPresentModeKHR            window_present_mode_;

        VkSwapchainKHR              swapchain_;
        VkRenderPass                render_pass_;

        VkImage                     back_buffer_images_[num_max_back_buffers];
        VkImageView                 back_buffer_views_[num_max_back_buffers];
        VkFramebuffer               framebuffers_[num_max_back_buffers];

        VkPresentModeKHR            default_present_mode_;

        uint32_t                    back_buffer_count_;

        VkCommandPool               command_pools_[num_queued_frames_];
        VkCommandBuffer             command_buffers_[num_queued_frames_];
        VkFence                     fences_[num_queued_frames_];
        VkSemaphore                 present_semaphores_[num_queued_frames_];
        VkSemaphore                 render_complete_semaphores_[num_queued_frames_];
    };
}
