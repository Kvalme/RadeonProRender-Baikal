#pragma once

#include "output.h"

#include "VKW.h"

namespace Baikal
{
    class VkwOutput : public Output
    {
    public:
        VkwOutput(VkDevice device, std::uint32_t w, std::uint32_t h)
        : Output(w, h)
        , m_device(device)
        {
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
        VkDevice m_device;
    };
}
