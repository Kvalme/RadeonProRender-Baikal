#include "vkw_render_factory.h"

#include "Output/vkwoutput.h"
#include "Renderers/hybrid_renderer.h"
#include "PostEffects/bilateral_denoiser.h"
#include "PostEffects/wavelet_denoiser.h"

#include "Controllers/vkw_scene_controller.h"

#include <memory>

namespace Baikal
{
    VkwRenderFactory::VkwRenderFactory(VkDevice device, VkPhysicalDevice physical_device, int queue_family_index)
    : m_device(device)
    , m_physical_device(physical_device)
    , m_queue_family_index(queue_family_index)
    {
    }

    // Create a renderer of specified type
    std::unique_ptr<Renderer<VkwScene>> VkwRenderFactory::CreateRenderer(
                                                    RendererType type) const
    {
        switch (type)
        {
            case RendererType::kHybrid:
                return std::unique_ptr<Renderer<VkwScene>>(new HybridRenderer(m_device));
            case RendererType::kUnidirectionalPathTracer:
                throw std::runtime_error("Renderer not supported");
            default:
                throw std::runtime_error("Renderer not supported");
        }
    }

    std::unique_ptr<Output> VkwRenderFactory::CreateOutput(std::uint32_t w,
                                                           std::uint32_t h)
                                                           const
    {
        return std::unique_ptr<Output>(new VkwOutput(m_device, w, h));
    }

    std::unique_ptr<PostEffect> VkwRenderFactory::CreatePostEffect(
                                                    PostEffectType type) const
    {
        // TODO: Implement
        assert(0);
    }

    std::unique_ptr<SceneController<VkwScene>> VkwRenderFactory::CreateSceneController() const
    {
        return std::make_unique<VkwSceneController>(m_device, m_physical_device, m_queue_family_index);
    }
}
