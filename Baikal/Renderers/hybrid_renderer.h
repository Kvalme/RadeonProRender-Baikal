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
#pragma once

#include "math/int2.h"
#include "renderer.h"

#include "SceneGraph/vkwscene.h"
#include "Controllers/vkw_scene_controller.h"

#include "VKW.h"

#include <memory>


namespace Baikal
{
    ///< Renderer implementation
    class HybridRenderer : public Renderer<VkwScene>
    {
    public:

        HybridRenderer(
            VkDevice device
        );

        ~HybridRenderer() = default;

        // Renderer overrides
        void Clear(RadeonRays::float3 const& val,
                   Output& output) const override;

        // Render the scene into the output
        void Render(VkwScene const& scene) override;

        // Render single tile
        void RenderTile(VkwScene const& scene,
                        RadeonRays::int2 const& tile_origin,
                        RadeonRays::int2 const& tile_size) override;

        // Set output
        void SetOutput(OutputType type, Output* output) override;

        void SetRandomSeed(std::uint32_t seed) override;

        // Set max number of light bounces
        void SetMaxBounces(std::uint32_t max_bounces);
    };

}
