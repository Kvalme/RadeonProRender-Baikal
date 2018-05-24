#pragma once

#include "output.h"

#include "VKW.h"

namespace Baikal
{
    class VkwOutput : public Output
    {
    public:
        VkwOutput(vkw::RenderTargetManager& render_target_manager, std::uint32_t w, std::uint32_t h)
        : Output(w, h)
        {
            std::vector<vkw::RenderTargetCreateInfo> attachments = {
                {w, h, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT},
            };
    
            render_target_ = render_target_manager.CreateRenderTarget(attachments);
        }

        void GetData(RadeonRays::float3* data) const
        {
        }

        void GetData(RadeonRays::float3* data, /* offset in elems */ size_t offset, /* read elems */size_t elems_count) const
        {
        }

        void Clear(RadeonRays::float3 const& val)
        {
        }
        
    private:
        vkw::RenderTarget   render_target_;
    };
}
