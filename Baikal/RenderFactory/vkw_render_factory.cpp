#include "vkw_render_factory.h"

#include "Output/vkwoutput.h"
#include "Renderers/hybrid_renderer.h"
#include "PostEffects/bilateral_denoiser.h"
#include "PostEffects/wavelet_denoiser.h"

#include "Controllers/vkw_scene_controller.h"

#include <memory>

namespace Baikal
{
    VkwRenderFactory::VkwRenderFactory(VkDevice device, VkPhysicalDevice physical_device, uint32_t queue_family_index)
    : device_(device)
    , physical_device_(physical_device)
    , queue_family_index_(queue_family_index)
    {
        memory_allocator_ = std::unique_ptr<vkw::MemoryAllocator>(new vkw::MemoryAllocator(device, physical_device));
        memory_manager_ = std::unique_ptr<vkw::MemoryManager>(new vkw::MemoryManager(device, queue_family_index, *memory_allocator_));
        render_target_manager_ = std::unique_ptr<vkw::RenderTargetManager>(new vkw::RenderTargetManager(device, *memory_manager_));
    }

    // Create a renderer of specified type
    std::unique_ptr<Renderer<VkwScene>> VkwRenderFactory::CreateRenderer(RendererType type) const
    {
        switch (type)
        {
            case RendererType::kHybrid:
                return std::unique_ptr<Renderer<VkwScene>>(new HybridRenderer(device_, *memory_manager_, *render_target_manager_));
            case RendererType::kUnidirectionalPathTracer:
                throw std::runtime_error("Renderer not supported");
            default:
                throw std::runtime_error("Renderer not supported");
        }
    }

    std::unique_ptr<Output> VkwRenderFactory::CreateOutput(std::uint32_t w, std::uint32_t h) const
    {
        return std::unique_ptr<Output>(new VkwOutput(*render_target_manager_, w, h));
    }

    std::unique_ptr<PostEffect> VkwRenderFactory::CreatePostEffect(PostEffectType type) const
    {
        // TODO: Implement
        assert(0);

		std::unique_ptr<PostEffect> post_effect;
		return post_effect;
    }

    std::unique_ptr<SceneController<VkwScene>> VkwRenderFactory::CreateSceneController() const
    {
        return std::make_unique<VkwSceneController>(*memory_allocator_, *memory_manager_, device_, physical_device_, queue_family_index_);
    }
}
