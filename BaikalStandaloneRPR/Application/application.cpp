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

#include <OpenImageIO/imageio.h>
#include "Application/application.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

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

#define _USE_MATH_DEFINES
#include <math.h>

#include "math/mathutils.h"
#include "material_io.h"

using namespace RadeonRays;

namespace BaikalRPR
{
    static bool     g_is_left_pressed = false;
    static bool     g_is_right_pressed = false;
    static bool     g_is_fwd_pressed = false;
    static bool     g_is_back_pressed = false;
    static bool     g_is_climb_pressed = false;
    static bool     g_is_descent_pressed = false;
    static bool     g_is_mouse_tracking = false;
    static bool     g_is_double_click = false;
    static bool     g_is_f10_pressed = false;
    static bool     g_is_middle_pressed = false; // middle mouse button
    static float2   g_mouse_pos = float2(0, 0);
    static float2   g_mouse_delta = float2(0, 0);
    static float2   g_scroll_delta = float2(0, 0);

    auto start = std::chrono::high_resolution_clock::now();

    void Application::OnMouseMove(GLFWwindow* window, double x, double y)
    {
        if (g_is_mouse_tracking)
        {
            g_mouse_delta = float2((float)x, (float)y) - g_mouse_pos;
            g_mouse_pos = float2((float)x, (float)y);
        }
    }

    void Application::OnMouseScroll(GLFWwindow* window, double x, double y)
    {
        g_scroll_delta = float2((float)x, (float)y);
    }

    void Application::OnMouseButton(GLFWwindow* window, int button, int action, int mods)
    {
        if ((button == GLFW_MOUSE_BUTTON_LEFT) ||
            (button == GLFW_MOUSE_BUTTON_RIGHT) ||
            (button == GLFW_MOUSE_BUTTON_MIDDLE))
        {
            if (action == GLFW_PRESS)
            {
                double x, y;
                glfwGetCursorPos(window, &x, &y);
                g_mouse_pos = float2((float)x, (float)y);
                g_mouse_delta = float2(0, 0);
            }
            else if (action == GLFW_RELEASE && g_is_mouse_tracking)
            {
                g_is_mouse_tracking = false;
                g_is_middle_pressed = false;
                g_mouse_delta = float2(0, 0);
            }
        }

        if ((button == GLFW_MOUSE_BUTTON_RIGHT) &&  (action == GLFW_PRESS))
        {
            g_is_mouse_tracking = true;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (action == GLFW_PRESS)
            {
                double x, y;
                glfwGetCursorPos(window, &x, &y);
                g_mouse_pos = float2((float)x, (float)y);

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>
                    (std::chrono::high_resolution_clock::now() - start);
                g_is_double_click = (duration.count() < 200) ? (true) : (false);
                start = std::chrono::high_resolution_clock::now();
            }
            else if (action == GLFW_RELEASE  && g_is_mouse_tracking)
            {
                g_is_double_click = false;
            }
        }

        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        {
            if (action == GLFW_PRESS)
            {
                g_is_middle_pressed = true;
                g_is_mouse_tracking = true;
            }
        }
    }

    void Application::OnKey(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        
        const bool press_or_repeat = action == GLFW_PRESS || action == GLFW_REPEAT;


        (void)mods; // Modifiers are not reliable across systems

        switch (key)
        {
        case GLFW_KEY_W:
            g_is_fwd_pressed = press_or_repeat;
            break;
        case GLFW_KEY_S:
            g_is_back_pressed = press_or_repeat;
            break;
        case GLFW_KEY_A:
            g_is_left_pressed = press_or_repeat;
            break;
        case GLFW_KEY_D:
            g_is_right_pressed = press_or_repeat;
            break;
        case GLFW_KEY_Q:
            g_is_climb_pressed = press_or_repeat;
            break;
        case GLFW_KEY_E:
            g_is_descent_pressed = press_or_repeat;
            break;
        case GLFW_KEY_F1:
            app->m_settings.gui_visible = action == GLFW_PRESS ? !app->m_settings.gui_visible : app->m_settings.gui_visible;
            break;
        case GLFW_KEY_F3:
            app->m_settings.benchmark = action == GLFW_PRESS ? true : app->m_settings.benchmark;
            break;
        case GLFW_KEY_F10:
            g_is_f10_pressed = action == GLFW_PRESS;
            break;
        default:
            break;
        }
    }

    Application::Application()
    : m_window(nullptr)
    , m_num_triangles(0)
    , m_num_instances(0)
    {
    }

    void Application::Update(bool update_required)
    {
        static auto prevtime = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(time - prevtime);
        prevtime = time;

        bool update = update_required;
        /*float camrotx = 0.f;
        float camroty = 0.f;

        const float kMouseSensitivity = 0.001125f;
        const float kScrollSensitivity = 0.05f;
        auto camera = app_render_->GetCamera();
        if (!m_settings.benchmark && !m_settings.time_benchmark)
        {
            float2 delta = g_mouse_delta * float2(kMouseSensitivity, kMouseSensitivity);
            float2 scroll_delta = g_scroll_delta * float2(kScrollSensitivity, kScrollSensitivity);
            camrotx = -delta.x;
            camroty = -delta.y;

            if (!g_is_middle_pressed)
            {
                if (std::abs(camroty) > 0.001f)
                {
                    camera->Tilt(camroty);
                    update = true;
                }

                if (std::abs(camrotx) > 0.001f)
                {

                    camera->Rotate(camrotx);
                    update = true;
                }
            }

            const float kMovementSpeed = m_settings.cspeed;
            if (std::abs(scroll_delta.y) > 0.001f)
            {
                camera->Zoom(scroll_delta.y * kMovementSpeed);
                g_scroll_delta = float2();
                update = true;
            }
            if (g_is_fwd_pressed)
            {
                camera->MoveForward((float)dt.count() * kMovementSpeed);
                update = true;
            }

            if (g_is_back_pressed)
            {
                camera->MoveForward(-(float)dt.count() * kMovementSpeed);
                update = true;
            }

            if (g_is_right_pressed)
            {
                camera->MoveRight((float)dt.count() * kMovementSpeed);
                update = true;
            }

            if (g_is_left_pressed)
            {
                camera->MoveRight(-(float)dt.count() * kMovementSpeed);
                update = true;
            }

            if (g_is_climb_pressed)
            {
                camera->MoveUp((float)dt.count() * kMovementSpeed);
                update = true;
            }

            if (g_is_descent_pressed)
            {
                camera->MoveUp(-(float)dt.count() * kMovementSpeed);
                update = true;
            }

            if (g_is_f10_pressed)
            {
                g_is_f10_pressed = false; //one time execution
                SaveToFile(time);
            }

            if (g_is_middle_pressed)
            {
                float distance = (float)dt.count() * kMovementSpeed / 2.f;
                float right_shift, up_shift;
                right_shift = (delta.x) ? distance * delta.x / std::abs(delta.x) : 0;
                up_shift = (delta.y) ? distance * delta.y / std::abs(delta.y) : 0;
                camera->MoveRight(-right_shift);
                camera->MoveUp(up_shift);
                update = true;
                g_mouse_delta = RadeonRays::float2(0, 0);
            }
        }*/

        if (update)
        {
            //if (g_num_samples > -1)
            {
                m_settings.samplecount = 0;
            }

            app_render_->UpdateScene();
        }

        if (m_settings.num_samples == -1 || m_settings.samplecount <  m_settings.num_samples)
        {
            app_render_->Render(m_settings.samplecount);
            ++m_settings.samplecount;
        }
        else if (m_settings.samplecount == m_settings.num_samples)
        {
            app_render_->SaveFrameBuffer(m_settings);
            std::cout << "Target sample count reached\n";
            ++m_settings.samplecount;
            //exit(0);
        }

        app_render_->Update(m_settings);
    }

}
