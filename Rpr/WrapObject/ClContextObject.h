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

#include "WrapObject/ContextObject.h"

#include "WrapObject/WrapObject.h"
#include "WrapObject/LightObject.h"

#include "cl_config_manager.h"
#include "Renderers/monte_carlo_renderer.h"

#include <vector>
#include "RadeonProRender.h"
#include "RadeonProRender_GL.h"

class TextureObject;
class FramebufferObject;
class SceneObject;
class MatSysObject;
class ShapeObject;
class CameraObject;
class MaterialObject;

//this class represent rpr_context
class ClContextObject
    : public ContextObject
{
public:
    ClContextObject(rpr_creation_flags creation_flags);
    virtual ~ClContextObject() = default;
    
    //AOV
    virtual void SetAOV(rpr_int in_aov, FramebufferObject* buffer) override;
    virtual FramebufferObject* GetAOV(rpr_int in_aov) override;

    //render
    virtual void Render() override;
    virtual void RenderTile(rpr_uint xmin, rpr_uint xmax, rpr_uint ymin, rpr_uint ymax) override;

    //create methods
    virtual FramebufferObject* CreateFrameBuffer(rpr_framebuffer_format const in_format, rpr_framebuffer_desc const * in_fb_desc) override;
    virtual FramebufferObject* CreateFrameBufferFromGLTexture(rpr_GLenum target, rpr_GLint miplevel, rpr_GLuint texture) override;
private:
    void PrepareScene();

    //render configs
    std::vector<ClConfigManager::ClConfig> m_cfgs;
};
