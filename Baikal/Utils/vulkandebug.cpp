#include "vulkandebug.h"

#include <vulkan/vulkan.h>
#include <iostream>
#include <map>

const char *Baikal::VK_LAYER_LUNARG_standard_validation_name = "VK_LAYER_LUNARG_standard_validation";
const char *Baikal::VK_LAYER_LUNARG_parameter_validation_name = "VK_LAYER_LUNARG_parameter_validation";


VKAPI_ATTR VkBool32 VKAPI_CALL Baikal::DebugReportCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData)
{
    static const std::map<VkDebugReportFlagsEXT, std::string> vkDebugPrefix =
    {
        {VK_DEBUG_REPORT_INFORMATION_BIT_EXT, "Info: "},
        {VK_DEBUG_REPORT_WARNING_BIT_EXT, "Warn: "},
        {VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT, "Perf: "},
        {VK_DEBUG_REPORT_DEBUG_BIT_EXT, "Debug: "},
        {VK_DEBUG_REPORT_ERROR_BIT_EXT, "Error: "}
    };

    auto prefix_it = vkDebugPrefix.find(flags);
    if (prefix_it == vkDebugPrefix.end())
    {
        std::cerr << "VK: " <<pLayerPrefix <<" : "<< pMessage << std::endl;
    }
    else
    {
        std::cerr << prefix_it->second.c_str() << pLayerPrefix << " : "<< pMessage << std::endl;
    }

    return VK_FALSE;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugReportCallbackEXT(
    VkInstance                                  instance,
    const VkDebugReportCallbackCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugReportCallbackEXT*                   pCallback)
{
    static PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));

    return vkCreateDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, pCallback);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugReportCallbackEXT(
     VkInstance                                  instance,
     VkDebugReportCallbackEXT                    callback,
     const VkAllocationCallbacks*                pAllocator)
{
    static PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));

    vkDestroyDebugReportCallbackEXT(instance, callback, pAllocator);
}