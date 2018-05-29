
/**********************************************************************
 Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 
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

#include "render_factory.h"

#include "SceneGraph/vkwscene.h"

#include <memory>
#include <string>

#include "VKW.h"

namespace Baikal
{
    /**
     \brief RenderFactory class is in charge of render entities creation.
     
     \details RenderFactory makes sure renderer objects are compatible between
     each other since many of them might use either CPU or GPU implementation.
     Entities create via the same factory are known to be compatible.
     */
    class VkwRenderFactory : public RenderFactory<VkwScene>
    {
    public:
        VkwRenderFactory(VkDevice device, VkPhysicalDevice physical_device, uint32_t queue_family_index);

        // Create a renderer of specified type
        std::unique_ptr<Renderer<VkwScene>> CreateRenderer(RendererType type) const override;
        
        // Create an output of specified type
        std::unique_ptr<Output> CreateOutput(std::uint32_t w, std::uint32_t h) const override;
        
        // Create post effect of specified type
        std::unique_ptr<PostEffect> CreatePostEffect(PostEffectType type) const override;

        std::unique_ptr<SceneController<VkwScene>> CreateSceneController() const override;

    private:
        std::unique_ptr<vkw::MemoryAllocator>       memory_allocator_;
        std::unique_ptr<vkw::MemoryManager>         memory_manager_;
        std::unique_ptr<vkw::RenderTargetManager>   render_target_manager_;

        int                                         queue_family_index_;
        VkDevice                                    device_;
        VkPhysicalDevice                            physical_device_;
    };
}
