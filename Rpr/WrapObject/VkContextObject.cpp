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

#include "VkContextObject.h"

#include "WrapObject/SceneObject.h"
#include "WrapObject/MatSysObject.h"
#include "WrapObject/ShapeObject.h"
#include "WrapObject/CameraObject.h"
#include "WrapObject/LightObject.h"
#include "WrapObject/FramebufferObject.h"
#include "WrapObject/Materials/MaterialObject.h"
#include "WrapObject/Exception.h"

#include "SceneGraph/scene1.h"
#include "SceneGraph/iterator.h"
#include "SceneGraph/material.h"
#include "SceneGraph/light.h"

#include "RenderFactory/render_factory.h"
#include "Renderers/renderer.h"

#include "RadeonProRender_VK.h"

namespace
{

    std::map<uint32_t, Baikal::OutputType> kOutputTypeMap = { {RPR_AOV_COLOR, Baikal::OutputType::kColor},
                                                                        {RPR_AOV_GEOMETRIC_NORMAL, Baikal::OutputType::kWorldGeometricNormal},
                                                                        {RPR_AOV_SHADING_NORMAL, Baikal::OutputType::kWorldShadingNormal},
                                                                        {RPR_AOV_UV, Baikal::OutputType::kUv},
                                                                        {RPR_AOV_WORLD_COORDINATE, Baikal::OutputType::kWorldPosition},
                                                                        };

}// anonymous

VkContextObject::VkContextObject(rpr_creation_flags creation_flags, const rpr_context_properties *props)
    : ContextObject(creation_flags)
{
    rpr_int result = RPR_SUCCESS;

    bool interop = (creation_flags & RPR_CREATION_FLAGS_ENABLE_GL_INTEROP) != 0;
    if (interop)
    {
        throw Exception(RPR_ERROR_INTERNAL_ERROR, "ContextObject: OpenGL interop not supported");
    }
    if (creation_flags & RPR_CREATION_FLAGS_ENABLE_GPU0)
    {
        try
        {
            //TODO: check num_bounces 
            VkInstance instance;
            VkDevice device;
            VkPhysicalDevice physical_device;

            const rpr_context_properties *p = props;
            while (*p)
            {
                if ((*p) == RPR_VK_INSTANCE)
                {
                    instance = static_cast<VkInstance>(*(p + 1));
                    ++p;
                }
                else if((*p) == RPR_VK_DEVICE)
                {
                    device = static_cast<VkDevice>(*(p + 1));
                    ++p;
                }
                else if((*p) == RPR_VK_PHYSICAL_DEVICE)
                {
                    physical_device = static_cast<VkPhysicalDevice>(*(p + 1));
                    ++p;
                }
                ++p;
            }

            if (!instance || !device || !physical_device)
            {
                throw Exception(RPR_ERROR_INTERNAL_ERROR, "");
            }

            VkConfigManager::CreateConfig(m_cfg, instance, device, physical_device);
        }
        catch(Exception&)
        {
            throw;
        }
        catch (...)
        {
            // failed to create context with interop
            result = RPR_ERROR_UNSUPPORTED;
        }
    }
    else
    {
        result = RPR_ERROR_UNIMPLEMENTED;
    }    

    if (result != RPR_SUCCESS)
    {
        throw Exception(result, "");
    }
}

void VkContextObject::SetAOV(rpr_int in_aov, FramebufferObject* buffer)
{
    FramebufferObject* old_buf = GetAOV(in_aov);

    auto aov = kOutputTypeMap.find(in_aov);
    if (aov == kOutputTypeMap.end())
    {
        throw Exception(RPR_ERROR_UNIMPLEMENTED, "Context: requested AOV not implemented.");
    }
    
    m_cfg.renderer_->SetOutput(aov->second, buffer->GetOutput());

    //update registered output framebuffer
    m_output_framebuffers.erase(old_buf);
    m_output_framebuffers.insert(buffer);
}


FramebufferObject* VkContextObject::GetAOV(rpr_int in_aov)
{
    auto aov = kOutputTypeMap.find(in_aov);
    if (aov == kOutputTypeMap.end())
    {
        throw Exception(RPR_ERROR_UNIMPLEMENTED, "Context: requested AOV not implemented.");
    }

    Baikal::Output* out = m_cfg.renderer_->GetOutput(aov->second);
    if (!out)
    {
        return nullptr;
    }
    
    //find framebuffer
    auto it = find_if(m_output_framebuffers.begin(), m_output_framebuffers.end(), [out](FramebufferObject* buff)
    {
        return buff->GetOutput() == out;
    });
    if (it == m_output_framebuffers.end())
    {
        throw Exception(RPR_ERROR_INTERNAL_ERROR, "Context: unknown framebuffer.");
    }

    return *it;
}

void VkContextObject::Render()
{
    PrepareScene();

    //render
    auto& scene = m_cfg.controller_->GetCachedScene(m_current_scene->GetScene());
    m_cfg.renderer_->Render(scene);

    PostRender();
}

void VkContextObject::RenderTile(rpr_uint xmin, rpr_uint xmax, rpr_uint ymin, rpr_uint ymax)
{
    PrepareScene();

    const RadeonRays::int2 origin = { (int)xmin, (int)ymin };
    const RadeonRays::int2 size = { (int)xmax - (int)xmin, (int)ymax - (int)ymin };
    //render
    auto& scene = m_cfg.controller_->GetCachedScene(m_current_scene->GetScene());
    m_cfg.renderer_->RenderTile(scene, origin, size);
    PostRender();
}

FramebufferObject* VkContextObject::CreateFrameBuffer(rpr_framebuffer_format const in_format, rpr_framebuffer_desc const * in_fb_desc)
{
    //TODO: implement
    if (in_format.type != RPR_COMPONENT_TYPE_FLOAT32 || in_format.num_components != 4)
    {
        throw Exception(RPR_ERROR_UNIMPLEMENTED, "ContextObject: only 4 component RPR_COMPONENT_TYPE_FLOAT32 implemented now.");
    }

    Baikal::Output* out = m_cfg.factory_->CreateOutput(in_fb_desc->fb_width, in_fb_desc->fb_height).release();
    FramebufferObject* result = new FramebufferObject(out);
    return result;
}

FramebufferObject* VkContextObject::CreateFrameBufferFromGLTexture(rpr_GLenum target, rpr_GLint miplevel, rpr_GLuint texture)
{
    throw Exception(RPR_ERROR_INTERNAL_ERROR, "ContextObject: GL interop not supported.");

    return nullptr;
}

void VkContextObject::PrepareScene()
{
    m_current_scene->AddEmissive();

    m_cfg.controller_->CompileScene(m_current_scene->GetScene());
}
