
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

#include "Output/output.h"
#include "Output/vkwoutput.h"
#include "SceneGraph/vkwscene.h"
#include "Utils/vk_config_manager.h"
#include "math/float3.h"
#include "app_utils.h"

#include <memory>
#include <vector>
#include <future>

namespace Baikal
{
    class AppRender
    {
    public:
        // update render
        virtual void Update(AppSettings& settings) = 0;

        //compile scene
        virtual void UpdateScene() = 0;
        
        //render
        virtual void Render(int sample_cnt) = 0;
        virtual void StartRenderThreads() = 0;
        virtual void StopRenderThreads() = 0;
        virtual void RunBenchmark(AppSettings& settings) = 0;

        //save frame buffer to file
        virtual void SaveFrameBuffer(AppSettings& settings) = 0;
        virtual void SaveImage(const std::string& name, int width, int height, const RadeonRays::float3* data) = 0;

        inline Baikal::Camera::Ptr GetCamera() { return m_camera; };
        inline Baikal::Scene1::Ptr GetScene() { return m_scene; };

        inline OutputType GetOutputType() { return m_output_type; };

        virtual void SetNumBounces(int num_bounces) = 0;
        virtual void SetOutputType(OutputType type) = 0;

        virtual std::future<int> GetShapeId(std::uint32_t x, std::uint32_t y) = 0;
        virtual Baikal::Shape::Ptr GetShapeById(int shape_id) = 0;

    protected:
        void LoadScene(AppSettings& settings);
    protected:
        Baikal::Scene1::Ptr m_scene;
        Baikal::Camera::Ptr m_camera;

        OutputType m_output_type;
    };
}
