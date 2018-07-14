#include "vkw_render_factory.h"

#include "Output/vkwoutput.h"
#include "Renderers/hybrid_renderer.h"
#include "PostEffects/bilateral_denoiser.h"
#include "PostEffects/wavelet_denoiser.h"

#include "Controllers/vkw_scene_controller.h"

#include <memory>

namespace Baikal
{
    VkwRenderFactory::VkwRenderFactory(VkDevice device
        , VkPhysicalDevice physical_device
        , uint32_t graphics_queue_family_index
        , uint32_t compute_queue_family_index
        , vkw::MemoryAllocator&       memory_allocator
        , vkw::MemoryManager&         memory_manager
        , vkw::RenderTargetManager&   render_target_manager
        , vkw::ShaderManager&         shader_manager
        , vkw::DescriptorManager&     descriptor_manager
        , vkw::PipelineManager&       pipeline_manager
        , vkw::Utils&                 utils
        , DirtyFlags                  update_flags)     : memory_allocator_(memory_allocator)
                                                        , memory_manager_(memory_manager)
                                                        , render_target_manager_(render_target_manager)
                                                        , shader_manager_(shader_manager)
                                                        , descriptor_manager_(descriptor_manager)
                                                        , pipeline_manager_(pipeline_manager)
                                                        , utils_(utils)
                                                        , graphics_queue_family_index_(graphics_queue_family_index)
                                                        , compute_queue_family_index_(compute_queue_family_index)
                                                        , device_(device)
                                                        , physical_device_(physical_device)
                                                        , update_flags_(update_flags)
    {
    }

    // Create a renderer of specified type
    std::unique_ptr<Renderer<VkwScene>> VkwRenderFactory::CreateRenderer(RendererType type) const
    {
        switch (type)
        {
            case RendererType::kHybrid:
                return std::unique_ptr<Renderer<VkwScene>>(new HybridRenderer(  device_,
                                                                                memory_manager_,
                                                                                shader_manager_,
                                                                                render_target_manager_,
                                                                                pipeline_manager_,
                                                                                graphics_queue_family_index_,
                                                                                compute_queue_family_index_));
            case RendererType::kUnidirectionalPathTracer:
                throw std::runtime_error("Renderer not supported");
            default:
                throw std::runtime_error("Renderer not supported");
        }
    }

    std::unique_ptr<Output> VkwRenderFactory::CreateOutput(std::uint32_t w, std::uint32_t h) const
    {
        return std::unique_ptr<Output>(new VkwOutput(render_target_manager_, utils_.CreateSemaphore(), w, h));
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
        return std::make_unique<VkwSceneController>(device_,
                                                    physical_device_,
                                                    memory_allocator_,
                                                    memory_manager_,
                                                    shader_manager_,
                                                    render_target_manager_,
                                                    pipeline_manager_,
                                                    graphics_queue_family_index_,
                                                    compute_queue_family_index_,
                                                    update_flags_);
    }
}