#include "vulkan/vulkan.h"

namespace Baikal
{
	VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
		VkDebugReportFlagsEXT       flags,
		VkDebugReportObjectTypeEXT  objectType,
		uint64_t                    object,
		size_t                      location,
		int32_t                     messageCode,
		const char*                 pLayerPrefix,
		const char*                 pMessage,
		void*                       pUserData);

	extern const char *VK_LAYER_LUNARG_standard_validation_name;
	extern const char *VK_LAYER_LUNARG_parameter_validation_name;
}