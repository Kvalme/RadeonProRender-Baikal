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
#include "vk_config_manager.h"

#include "RenderFactory/render_factory.h"
#include "Controllers/vkw_scene_controller.h"

#ifndef NDEBUG
#include "Utils/vulkandebug.h"
#endif

#ifndef APP_BENCHMARK

vkw::VkScopedObject<VkInstance> VkConfigManager::CreateInstance(const std::vector<const char*> &requested_extensions)
{
    VkApplicationInfo app_info;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = "BaikalStandalone";
    app_info.applicationVersion = 1;
    app_info.pEngineName = "BaikalStandalone";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    std::vector<const char*> extensions(requested_extensions);
    std::vector<const char*> layers;
#ifndef NDEBUG
#ifndef __APPLE__

    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    layers.push_back(Baikal::VK_LAYER_LUNARG_parameter_validation_name);
    layers.push_back(Baikal::VK_LAYER_LUNARG_standard_validation_name);

#endif
#endif

    VkInstanceCreateInfo instance_info;
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = extensions.size();
    instance_info.ppEnabledExtensionNames = extensions.data();
    instance_info.enabledLayerCount = layers.size();
    instance_info.ppEnabledLayerNames = layers.data();
    
    VkInstance instance = nullptr;
    VkResult res = vkCreateInstance(&instance_info, nullptr, &instance);
    if (res == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        throw std::runtime_error("Cannot find a compatible Vulkan ICD\n");
    }
    else if (res)
    {
        throw std::runtime_error("vkCreateInstance: Unknown error\n");
    }
    
#ifndef NDEBUG
#ifndef __APPLE__

    VkDebugReportCallbackCreateInfoEXT debugReportCallback;
    debugReportCallback.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debugReportCallback.pfnCallback = &(Baikal::DebugReportCallback);
    debugReportCallback.flags =
    VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;

    VkDebugReportCallbackEXT debug_report_callback_;
    vkCreateDebugReportCallbackEXT(instance, &debugReportCallback, nullptr, &debug_report_callback_);

    return vkw::VkScopedObject<VkInstance>(instance,
                                           [debug_report_callback_](VkInstance instance)
                                           {
                                               vkDestroyInstance(instance, nullptr);
                                               vkDestroyDebugReportCallbackEXT(instance, debug_report_callback_, nullptr);
                                           });

#endif
#endif

    return vkw::VkScopedObject<VkInstance>(instance,
                                      [](VkInstance instance)
                                      {
                                          vkDestroyInstance(instance, nullptr);

                                      });
}

vkw::VkScopedObject<VkDevice> VkConfigManager::CreateDevice(VkInstance instance
                                            , std::uint32_t& compute_queue_family_index
                                            , std::uint32_t& graphics_queue_family_index
                                            , VkPhysicalDevice* opt_physical_device)
{
    // Enumerate devices
    auto gpu_count = 0u;
    auto res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
    
    if (gpu_count == 0)
    {
        throw std::runtime_error("No compatible devices found\n");
    }
    
    std::vector<VkPhysicalDevice> gpus(gpu_count);
    res = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
    
    auto queue_family_count = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_props(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queue_family_count, queue_props.data());
    
    // Look for a queue supporting
    uint32_t queue_family_idx[2] = {0xFFFFFFFF, 0xFFFFFFFF};

    for (unsigned int i = 0; i < queue_family_count; i++)
    {
        if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queue_family_idx[0] = i;
        }

        if (queue_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            queue_family_idx[1] = i;
        }
    }
    
    if (queue_family_idx[0] == 0xFFFFFFFF || queue_family_idx[1] == 0xFFFFFFFF)
    {
        throw std::runtime_error("No graphics/compute queues found\n");
    }

    graphics_queue_family_index = queue_family_idx[0];
    compute_queue_family_index = queue_family_idx[1];

    uint32_t queue_count = queue_family_idx[0] == queue_family_idx[1] ? 1 : 2;
    
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos(queue_count);
    
    float queue_priority = 0.f;
    for (size_t i = 0; i < queue_create_infos.size(); i++)
    {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].pNext = nullptr;
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].queueCount = 1u;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
        queue_create_infos[i].queueFamilyIndex = queue_family_idx[i];
    }
    
    int device_extension_count = 1;
    const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo device_create_info;
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = nullptr;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.enabledLayerCount = 0u;
    device_create_info.ppEnabledLayerNames = nullptr;
    device_create_info.enabledExtensionCount = device_extension_count;
    device_create_info.ppEnabledExtensionNames = device_extensions;
    device_create_info.pEnabledFeatures = nullptr;
    
    VkDevice device = nullptr;
    res = vkCreateDevice(gpus[0], &device_create_info, nullptr, &device);
    
    if (res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan device\n");
    }
    
    if (opt_physical_device)
    {
        *opt_physical_device = gpus[0];
    }
    
    return vkw::VkScopedObject<VkDevice>(device,
                                    [](VkDevice device)
                                    {
                                        vkDestroyDevice(device, nullptr);
                                    });
}

void VkConfigManager::CreateConfig(VkConfig& renderers, const std::vector<const char*> &requested_extensions)
{
    uint32_t compute_queue_family_index = -1;
    uint32_t graphics_queue_family_index = -1;

    VkPhysicalDevice physical_device;
        
    renderers.instance_ = CreateInstance(requested_extensions);
    renderers.device_   = CreateDevice(renderers.instance_.get(), compute_queue_family_index, graphics_queue_family_index, &physical_device);
    renderers.compute_queue_family_idx_ = compute_queue_family_index;
    renderers.graphics_queue_family_idx_ = graphics_queue_family_index;
    renderers.physical_device_ = physical_device;
    renderers.factory_ = std::make_unique<Baikal::VkwRenderFactory>(renderers.device_.get(), renderers.physical_device_, graphics_queue_family_index);
    renderers.controller_ = renderers.factory_->CreateSceneController();
    renderers.renderer_ = renderers.factory_->CreateRenderer(Baikal::VkwRenderFactory::RendererType::kHybrid);
}

#endif