#include "hybrid_renderer.h"

namespace Baikal
{
    HybridRenderer::HybridRenderer(VkDevice device)
    {

    }

    void HybridRenderer::Clear(RadeonRays::float3 const& val,
               Output& output) const
    {

    }

    void HybridRenderer::Render(VkwScene const& scene)
    {

    }
    
    void HybridRenderer::RenderTile(VkwScene const& scene,
                    RadeonRays::int2 const& tile_origin,
                    RadeonRays::int2 const& tile_size)
    {

    }

    void HybridRenderer::SetOutput(OutputType type, Output* output)
    {

    }

    void HybridRenderer::SetRandomSeed(std::uint32_t seed)
    {

    }

    void HybridRenderer::SetMaxBounces(std::uint32_t max_bounces)
    {

    }
}