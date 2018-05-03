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
#include "config_manager.h"

#if HYBRID_RENDERER
    #include "VKW.h"
#else
    #include "CLW.h"
#endif

#include "RenderFactory/render_factory.h"
#include "Controllers/vkw_scene_controller.h"

#ifndef APP_BENCHMARK

#ifdef __APPLE__
#include <OpenCL/OpenCL.h>
#include <OpenGL/OpenGL.h>
#elif WIN32
#define NOMINMAX
#include <Windows.h>
#include "GL/glew.h"
#else
#include <CL/cl.h>
#include <GL/glew.h>
#include <GL/glx.h>
#endif

vkw::VkScopedObject<VkInstance> instance;

vkw::VkScopedObject<VkInstance> create_instance()
{
    VkApplicationInfo app_info;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = "BaikalStandalone";
    app_info.applicationVersion = 1;
    app_info.pEngineName = "BaikalStandalone";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo instance_info;
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = 0;
    instance_info.ppEnabledExtensionNames = NULL;
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = NULL;
    
    VkInstance instance = nullptr;
    VkResult res = vkCreateInstance(&instance_info, nullptr, &instance);
    if (res == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        throw std::runtime_error("Cannot find a compatible Vulkan ICD\n");
    }
    else if (res)
    {
        throw std::runtime_error("Unknown error\n");
    }
    
    return vkw::VkScopedObject<VkInstance>(instance,
                                      [](VkInstance instance)
                                      {
                                          vkDestroyInstance(instance, nullptr);
                                      });
}

vkw::VkScopedObject<VkDevice> create_device(VkInstance instance,
                                       std::uint32_t& queue_family_index,
                                       VkPhysicalDevice* opt_physical_device = nullptr)
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
    
    float queue_priority = 0.f;
    VkDeviceQueueCreateInfo queue_create_info;
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.pNext = nullptr;
    queue_create_info.flags = 0;
    queue_create_info.queueCount = 1u;
    queue_create_info.pQueuePriorities = &queue_priority;
    
    auto queue_family_count = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_props(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queue_family_count, queue_props.data());
    
    // Look for a queue supporing both compute and transfer
    bool found = false;
    for (unsigned int i = 0; i < queue_family_count; i++)
    {
        if (queue_props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT))
        {
            queue_create_info.queueFamilyIndex = i;
            queue_family_index = i;
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        throw std::runtime_error("No compute/transfer queues found\n");
    }
    
    VkDeviceCreateInfo device_create_info;
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = nullptr;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = 1u;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledLayerCount = 0u;
    device_create_info.ppEnabledLayerNames = nullptr;
    device_create_info.enabledExtensionCount = 0u;
    device_create_info.ppEnabledExtensionNames = nullptr;
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

// $tmp
void ConfigManager::CreateConfigs(
        Mode mode,
        bool interop,
        std::vector<VkwConfig>& renderers,
        int initial_num_bounces,
        int req_platform_index,
        int req_device_index)
{
    renderers.resize(1);

    instance = create_instance();

    for (int i = 0; i < renderers.size(); ++i)
    {
        uint32_t queue_family_index = -1;
        VkPhysicalDevice physical_device;

        renderers[i].device = create_device(instance, queue_family_index, &physical_device);
        renderers[i].physical_device = physical_device;
        renderers[i].factory = std::make_unique<Baikal::VkwRenderFactory>(renderers[i].device, renderers[i].physical_device, queue_family_index);
        renderers[i].controller = renderers[i].factory->CreateSceneController();
        renderers[i].renderer = renderers[i].factory->CreateRenderer(Baikal::VkwRenderFactory::RendererType::kHybrid);
    }
}

void ConfigManager::CreateConfigs(
    Mode mode,
    bool interop,
    std::vector<Config>& configs,
    int initial_num_bounces,
    int req_platform_index,
    int req_device_index)
{
    std::vector<CLWPlatform> platforms;

    CLWPlatform::CreateAllPlatforms(platforms);

    if (platforms.size() == 0)
    {
        throw std::runtime_error("No OpenCL platforms installed.");
    }

    configs.clear();

    if (req_platform_index >= (int)platforms.size())
        throw std::runtime_error("There is no such platform index");
    else if ((req_platform_index > 0) &&
        (req_device_index >= (int)platforms[req_platform_index].GetDeviceCount()))
        throw std::runtime_error("There is no such device index");

    bool hasprimary = false;

    int i = (req_platform_index >= 0) ? (req_platform_index) : 0;
    int d = (req_device_index >= 0) ? (req_device_index) : 0;

    int platforms_end = (req_platform_index >= 0) ?
        (req_platform_index + 1) : ((int)platforms.size());

    for (; i < platforms_end; ++i)
    {
        int device_end = 0;

        if (req_platform_index < 0 || req_device_index < 0)
            device_end = (int)platforms[i].GetDeviceCount();
        else
            device_end = req_device_index + 1;

        for (; d < device_end; ++d)
        {
            if (req_platform_index < 0)
            {
                if ((mode == kUseGpus || mode == kUseSingleGpu) && platforms[i].GetDevice(d).GetType() != CL_DEVICE_TYPE_GPU)
                    continue;

                if ((mode == kUseCpus || mode == kUseSingleCpu) && platforms[i].GetDevice(d).GetType() != CL_DEVICE_TYPE_CPU)
                    continue;
            }

            Config cfg;
            cfg.caninterop = false;

            if (platforms[i].GetDevice(d).HasGlInterop() && !hasprimary && interop)
            {
#ifdef WIN32
                cl_context_properties props[] =
                {
                    //OpenCL platform
                    CL_CONTEXT_PLATFORM, (cl_context_properties)((cl_platform_id)platforms[i]),
                    //OpenGL context
                    CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
                    //HDC used to create the OpenGL context
                    CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
                    0
                };
#elif __linux__
                cl_context_properties props[] =
                {
                    //OpenCL platform
                    CL_CONTEXT_PLATFORM, (cl_context_properties)((cl_platform_id)platforms[i]),
                    //OpenGL context
                    CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
                    //HDC used to create the OpenGL context
                    CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
                    0
                };
#elif __APPLE__
                CGLContextObj kCGLContext = CGLGetCurrentContext();
                CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
                // Create CL context properties, add handle & share-group enum !
                cl_context_properties props[] = {
                    CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                    (cl_context_properties)kCGLShareGroup, 0
                };
#endif
                cfg.context = CLWContext::Create(platforms[i].GetDevice(d), props);
                cfg.type = kPrimary;
                cfg.caninterop = true;
                hasprimary = true;
            }
            else
            {
                cfg.context = CLWContext::Create(platforms[i].GetDevice(d));
                cfg.type = kSecondary;
            }

            configs.push_back(std::move(cfg));

            if (mode == kUseSingleGpu || mode == kUseSingleCpu)
                break;
        }

        if (configs.size() == 1 && (mode == kUseSingleGpu || mode == kUseSingleCpu))
            break;
    }

    if (configs.size() == 0)
    {
        throw std::runtime_error(
            "No devices was selected (probably device index type does not correspond with real device type).");
    }

    if (!hasprimary)
    {
        configs[0].type = kPrimary;
    }

    for (int i = 0; i < configs.size(); ++i)
    {
        configs[i].factory = std::make_unique<Baikal::ClwRenderFactory>(configs[i].context, "cache");
        configs[i].controller = configs[i].factory->CreateSceneController();
        configs[i].renderer = configs[i].factory->CreateRenderer(Baikal::ClwRenderFactory::RendererType::kUnidirectionalPathTracer);
    }
}

#else
void ConfigManager::CreateConfigs(
    Mode mode,
    bool interop,
    std::vector<Config>& configs,
    int initial_num_bounces,
    int req_platform_index,
    int req_device_index)
{
    std::vector<CLWPlatform> platforms;

    CLWPlatform::CreateAllPlatforms(platforms);

    if (platforms.size() == 0)
    {
        throw std::runtime_error("No OpenCL platforms installed.");
    }

    configs.clear();

    bool hasprimary = false;
    for (int i = 0; i < (int)platforms.size(); ++i)
    {
        for (int d = 0; d < (int)platforms[i].GetDeviceCount(); ++d)
        {
            if ((mode == kUseGpus || mode == kUseSingleGpu) && platforms[i].GetDevice(d).GetType() != CL_DEVICE_TYPE_GPU)
                continue;

            if ((mode == kUseCpus || mode == kUseSingleCpu) && platforms[i].GetDevice(d).GetType() != CL_DEVICE_TYPE_CPU)
                continue;

            Config cfg;
            cfg.caninterop = false;
            cfg.context = CLWContext::Create(platforms[i].GetDevice(d));
            cfg.type = kSecondary;

            configs.push_back(std::move(cfg));

            if (mode == kUseSingleGpu || mode == kUseSingleCpu)
                break;
        }

        if (configs.size() == 1 && (mode == kUseSingleGpu || mode == kUseSingleCpu))
            break;
    }

    if (!hasprimary)
    {
        configs[0].type = kPrimary;
    }

    for (int i = 0; i < configs.size(); ++i)
    {
        configs[i].factory = std::make_unique<Baikal::ClwRenderFactory>(configs[i].context);
        configs[i].controller = configs[i].factory->CreateSceneController();
        configs[i].renderer = configs[i].factory->CreateRenderer(Baikal::ClwRenderFactory::RendererType::kUnidirectionalPathTracer);
    }
}
#endif //APP_BENCHMARK
