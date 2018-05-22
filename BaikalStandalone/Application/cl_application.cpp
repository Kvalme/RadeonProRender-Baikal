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

#ifdef __APPLE__
#include <OpenCL/OpenCL.h>
#define GLFW_INCLUDE_GLCOREARB
#define GLFW_NO_GLU
#include "GLFW/glfw3.h"
#elif WIN32
#define NOMINMAX
#include <Windows.h>
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#else
#include <CL/cl.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include "GLFW/glfw3.h"
#endif

#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_glfw_gl3.h"

#include <memory>
#include <chrono>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <mutex>
#include <fstream>
#include <functional>
#include <queue>

#include <OpenImageIO/imageio.h>

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef RR_EMBED_KERNELS
#include "./CL/cache/kernels.h"
#endif

#include "CLW.h"

#include "math/mathutils.h"
#include "Application/cl_application.h"
#include "SceneGraph/IO/material_io.h"

using namespace RadeonRays;

namespace Baikal
{
    void ClApplication::SaveToFile(std::chrono::high_resolution_clock::time_point time) const
    {
        using namespace OIIO;
        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        assert(glGetError() == 0);
        const auto channels = 3;
        auto *data = new GLubyte[channels * w * h];
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

        //opengl coordinates to oiio coordinates
        for (auto i = 0; i <= h / 2; ++i)
        {
            std::swap_ranges(data + channels * w * i, data + channels * w * (i + 1) - 1, data + channels * w * (h - (i + 1)));
        }
        
        const auto filename = m_settings.path + "/" + m_settings.base_image_file_name + "-" + std::to_string(time.time_since_epoch().count()) + "." + m_settings.image_file_format;
        auto out = ImageOutput::create(filename);
        if (out)
        {
            ImageSpec spec{ w, h, channels, TypeDesc::UINT8 };
            out->open(filename, spec);
            out->write_image(TypeDesc::UINT8, data);
            out->close();
            delete out; // ImageOutput::destroy not found
        }
        else
        {
            std::cout << "Wrong file format\n";
        }
        
        delete[] data;
    }


    void OnError(int error, const char* description)
    {
        std::cout << description << "\n";
    }

    ClApplication::ClApplication(int argc, char * argv[])
    : Application()
    {
        // Command line parsing
        AppCliParser cli;
        m_settings = cli.Parse(argc, argv);
        if (!m_settings.cmd_line_mode)
        {
            // Initialize GLFW
            {
                auto err = glfwInit();
                if (err != GL_TRUE)
                {
                    std::cout << "GLFW initialization failed\n";
                    exit(-1);
                }
            }
            // Setup window
            glfwSetErrorCallback(OnError);
            glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #if __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

            // GLUT Window Initialization:
            m_window = glfwCreateWindow(m_settings.width, m_settings.height, "Baikal standalone demo", nullptr, nullptr);
            glfwMakeContextCurrent(m_window);

    #ifndef __APPLE__
            {
                glewExperimental = GL_TRUE;
                GLenum err = glewInit();
                if (err != GLEW_OK)
                {
                    std::cout << "GLEW initialization failed\n";
                    exit(-1);
                }
            }
    #endif

            ImGui_ImplGlfwGL3_Init(m_window, true);

            try
            {
                m_gl.reset(new AppGlRender(m_settings));
                app_render_.reset(new AppClRender(m_settings, m_gl->GetTexture()));

                //set callbacks
                using namespace std::placeholders;
                glfwSetWindowUserPointer(m_window, this);
                glfwSetMouseButtonCallback(m_window, Application::OnMouseButton);
                glfwSetCursorPosCallback(m_window, Application::OnMouseMove);
                glfwSetKeyCallback(m_window, Application::OnKey);
                glfwSetScrollCallback(m_window, Application::OnMouseScroll);
            }
            catch (std::runtime_error& err)
            {
                glfwDestroyWindow(m_window);
                std::cout << err.what();
                exit(-1);
            }
        }
        else
        {
            m_settings.interop = false;
            app_render_.reset(new AppClRender(m_settings, -1));
        }
    }

    void ClApplication::Run()
    {
        CollectSceneStats();

        if (!m_settings.cmd_line_mode)
        {
            try
            {
                app_render_->StartRenderThreads();
                static bool update = true;
                while (!glfwWindowShouldClose(m_window))
                {

                    ImGui_ImplGlfwGL3_NewFrame();
                    Update(update);
                    m_gl->Render(m_window);
                    update = UpdateGui();

                    glfwSwapBuffers(m_window);
                    glfwPollEvents();
                }

                app_render_->StopRenderThreads();

                glfwDestroyWindow(m_window);
            }
            catch (std::runtime_error& err)
            {
                glfwDestroyWindow(m_window);
                std::cout << err.what();
                exit(-1);
            }
        }
        else
        {
            app_render_.reset(new AppClRender(m_settings, -1));
                        
            std::cout << "Number of triangles: " << m_num_triangles << "\n";
            std::cout << "Number of instances: " << m_num_instances << "\n";

            //compile scene
            app_render_->UpdateScene();
            app_render_->RunBenchmark(m_settings);

            auto minutes = (int)(m_settings.time_benchmark_time / 60.f);
            auto seconds = (int)(m_settings.time_benchmark_time - minutes * 60);

            std::cout << "General benchmark results:\n";
            std::cout << "\tRendering time: " << minutes << "min:" << seconds << "s\n";
            std::string rating;
            RadeonRays::float3 color;
            if (GradeTimeBenchmarkResults(m_settings.modelname, minutes * 60 + seconds, rating, color))
            {
                std::cout << "\tRating: " << rating.c_str() << "\n";
            }
            else
            {
                std::cout << "\tRating: N/A\n";
            }

            std::cout << "RT benchmark results:\n";
            std::cout << "\tPrimary: " << m_settings.stats.primary_throughput * 1e-6f << " Mrays/s\n";
            std::cout << "\tSecondary: " << m_settings.stats.secondary_throughput * 1e-6f << " Mrays/s\n";
            std::cout << "\tShadow: " << m_settings.stats.shadow_throughput * 1e-6f << " Mrays/s\n";
        }
    }
}