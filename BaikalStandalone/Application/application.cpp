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

#include "Application/application.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

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

#include "math/mathutils.h"
#include "SceneGraph/IO/material_io.h"

using namespace RadeonRays;

namespace Baikal
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
    static float2   g_mouse_pos = float2(0, 0);
    static float2   g_mouse_delta = float2(0, 0);
    static float2   g_scroll_delta = float2(0, 0);

    auto start = std::chrono::high_resolution_clock::now();

    Application::MaterialSelector::MaterialSelector(Material::Ptr root) :
        m_root(root), m_current(m_root)
    {  }

    bool Application::MaterialSelector::IsRoot() const
    { return (m_root == m_current); }

    Material::Ptr Application::MaterialSelector::Get()
    {
        return m_current;
    }

    void Application::MaterialSelector::SelectMaterial(Material::Ptr material)
    {
        m_current = material;
    }

    void Application::MaterialSelector::GetParent()
    {
        if (m_root == m_current)
        {
            return;
        }

        std::queue<Material::Ptr> queue;
        queue.push(m_root);

        while (!queue.empty())
        {
            auto parent = queue.front();
            for (size_t i = 0; i < queue.front()->GetNumInputs(); i++)
            {
                auto input = queue.front()->GetInput(i);

                if (input.value.type == Material::InputType::kMaterial)
                {
                    if (input.value.mat_value == m_current)
                    {
                        m_current = parent;
                        return;
                    }
                    queue.push(input.value.mat_value);
                }
            }
            queue.pop();
        }

        return;
    }

    bool Application::InputSettings::HasMultiplier() const
    { return m_multiplier.first; }

    float Application::InputSettings::GetMultiplier() const
    { return m_multiplier.second; }

    void Application::InputSettings::SetMultiplier(float multiplier)
    {
        m_multiplier.first = true;
        m_multiplier.second = multiplier;
    }

    RadeonRays::float3  Application::InputSettings::GetColor() const
    { return m_color.second; }

    void Application::InputSettings::SetColor(RadeonRays::float3  color)
    {
        m_color.first = true;
        m_color.second = color;
    }

    std::uint32_t Application::InputSettings::GetInteger() const
    { return m_integer_input.second; }

    void Application::InputSettings::SetInteger(std::uint32_t integer)
    {
        m_integer_input.first= true;
        m_integer_input.second = integer;
    }

    std::string Application::InputSettings::GetTexturePath() const
    { return m_texture_path.second; }

    void Application::InputSettings::SetTexturePath(std::string texture_path)
    {
        m_texture_path.first = true;
        m_texture_path.second = texture_path;
    }


    void Application::MaterialSettings::Clear()
    {
        inputs_info.clear();
    }

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
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            if (action == GLFW_PRESS)
            {
                g_is_mouse_tracking = true;

                double x, y;
                glfwGetCursorPos(window, &x, &y);
                g_mouse_pos = float2((float)x, (float)y);
                g_mouse_delta = float2(0, 0);
            }
            else if (action == GLFW_RELEASE && g_is_mouse_tracking)
            {
                g_is_mouse_tracking = false;
                g_mouse_delta = float2(0, 0);
            }
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (action == GLFW_PRESS)
            {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>
                    (std::chrono::high_resolution_clock::now() - start);

                if (duration.count() < 200)
                {
                    double x, y;
                    glfwGetCursorPos(window, &x, &y);
                    g_mouse_pos = float2((float)x, (float)y);
                    g_mouse_delta = float2(0, 0);
                    g_is_double_click = true;
                }
                start = std::chrono::high_resolution_clock::now();
            }
            else if (action == GLFW_RELEASE)
                g_is_double_click = false;
        }
    }

    void Application::OnKey(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        ImGuiIO& io = ImGui::GetIO();
        Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        auto map = io.KeyMap;
        const bool press_or_repeat = action == GLFW_PRESS || action == GLFW_REPEAT;

        if (action == GLFW_PRESS)
            io.KeysDown[key] = true;
        if (action == GLFW_RELEASE)
            io.KeysDown[key] = false;

        (void)mods; // Modifiers are not reliable across systems
        io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
        io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

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
    , m_image_io(ImageIo::CreateImageIo())
    {
    }

    void Application::Update(bool update_required)
    {
        static auto prevtime = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(time - prevtime);
        prevtime = time;

        bool update = update_required;
        float camrotx = 0.f;
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
        }

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

    bool Application::GradeTimeBenchmarkResults(std::string const& scene, int time_in_sec, std::string& rating, RadeonRays::float3& color)
    {
        if (scene == "classroom.obj")
        {
            if (time_in_sec < 70)
            {
                rating = "Excellent";
                color = RadeonRays::float3(0.1f, 0.7f, 0.1f, 1.f);
            }
            else if (time_in_sec < 100)
            {
                rating = "Good";
                color = RadeonRays::float3(0.1f, 0.7f, 0.1f, 1.f);
            }
            else if (time_in_sec < 120)
            {
                rating = "Average";
                color = RadeonRays::float3(0.7f, 0.7f, 0.1f, 1.f);
            }
            else
            {
                rating = "Poor";
                color = RadeonRays::float3(0.7f, 0.1f, 0.1f, 1.f);
            }

            return true;
        }

        return false;
    }

    bool Application::ReadFloatInput(Material::Ptr material, MaterialSettings& settings, std::uint32_t input_idx, std::string id_suffix)
    {
        auto input = material->GetInput(input_idx);
        auto name = input.info.name;
        auto input_info = settings.inputs_info[input_idx];
        
        if (!settings.inputs_info[input_idx].HasMultiplier())
        {
            auto mult = std::max(
                std::max(input.value.float_value.x, input.value.float_value.y),
                input.value.float_value.z);

            mult = (mult > 1) ? (mult) : (1.f);

            input_info.SetMultiplier(mult);

            input_info.SetColor(RadeonRays::float3(
                input.value.float_value.x / mult,
                input.value.float_value.y / mult,
                input.value.float_value.z / mult));

            material->SetInputValue(input.info.name, input_info.GetColor());
        }

        auto mult = input_info.GetMultiplier();
        float color[3] = { 0 };
        auto input_color = input_info.GetColor();

        color[0] = input_color.x;
        color[1] = input_color.y;
        color[2] = input_color.z;

        if (!id_suffix.empty())
            ImGui::PushID(id_suffix.c_str());

        ImGui::InputFloat(name.c_str(), &mult, .0f, .0f, -1, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::ColorEdit3(name.c_str(), color);

        if (!id_suffix.empty())
            ImGui::PopID();

        if ((input.value.tex_value == nullptr) &&
            ((input_color.x != color[0]) ||
             (input_color.y != color[1]) ||
             (input_color.z != color[2]) ||
             (input_info.GetMultiplier() != mult)))
        {
            RadeonRays::float4 value(
                mult * color[0],
                mult * color[1],
                mult * color[2],
                0);
            input_info.SetColor(RadeonRays::float3(color[0], color[1], color[2]));
            input_info.SetMultiplier(mult);
            settings.inputs_info[input_idx] = input_info;
            material->SetInputValue(input.info.name, value);
            return true;
        }
        return false;
    }

    bool Application::ReadTextruePath(Material::Ptr material, MaterialSettings& settings, std::uint32_t input_idx)
    {
        const size_t buffer_size = 2048;
        char text_buffer[buffer_size] = { 0 };
        auto input = material->GetInput(input_idx);
        auto name = input.info.name;

        auto input_info = settings.inputs_info[input_idx];

        if (input_info.GetTexturePath().empty() && 
           (input.value.tex_value != nullptr))
        {
            strcpy(text_buffer, input.info.name.c_str());
        }
        else
        {
            strcpy(text_buffer, input_info.GetTexturePath().c_str());
        }

        if (ImGui::InputText((name + std::string("_texture")).c_str(), text_buffer, buffer_size, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            Texture::Ptr texture = nullptr;
            if (strlen(text_buffer) != 0)
            {
                try
                {
                    texture = m_image_io->LoadImage(text_buffer);
                    material->SetInputValue(input.info.name, texture);
                }
                catch (std::exception&)
                {
                    printf("WARNING: Can not load texture by specified path\n");
                }
            }
            else
            {
                material->SetInputValue(name, texture);
            }
            input_info.SetTexturePath(text_buffer);
            settings.inputs_info[input_idx] = input_info;

            return true;
        }
        return false;
    }

    bool Application::UpdateGui()
    {
        static float aperture = 0.0f;
        static float focal_length = 35.f;
        static float focus_distance = 1.f;
        static int num_bounces = 5;
        static char const* outputs =
            "Color\0"
            "World position\0"
            "Shading normal\0"
            "Geometric normal\0"
            "Texture coords\0"
            "Wire\0"
            "Albedo\0"
            "Tangent\0"
            "Bitangent\0"
            "Gloss\0"
            "Depth\0\0"
            ;

        static int output = 0;
        bool update = false;
        if (m_settings.gui_visible)
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(380, 580), ImVec2(380, 580));
            ImGui::Begin("Baikal settings");
            ImGui::Text("Use wsad keys to move");
            ImGui::Text("Q/E to climb/descent");
            ImGui::Text("Mouse+RMB to look around");
            ImGui::Text("F1 to hide/show GUI");
            ImGui::Separator();
            //TODO: implement
            //ImGui::Text("Device vendor: %s", m_cl->GetDevice(0).GetVendor().c_str());
            //ImGui::Text("Device name: %s", m_cl->GetDevice(0).GetName().c_str());
            //ImGui::Text("OpenCL: %s", m_cl->GetDevice(0).GetVersion().c_str());
            ImGui::Separator();
            ImGui::Text("Resolution: %dx%d ", m_settings.width, m_settings.height);
            ImGui::Text("Scene: %s", m_settings.modelname.c_str());
            ImGui::Text("Unique triangles: %d", m_num_triangles);
            ImGui::Text("Number of instances: %d", m_num_instances);
            ImGui::Separator();
            ImGui::SliderInt("GI bounces", &num_bounces, 1, 10);

            auto camera = app_render_->GetCamera();

            if (m_settings.camera_type == CameraType::kPerspective)
            {
                auto perspective_camera = std::dynamic_pointer_cast<PerspectiveCamera>(camera);

                if (!perspective_camera)
                {
                    throw std::runtime_error("Application::UpdateGui(...): can not cast to perspective camera");
                }

                if (aperture != m_settings.camera_aperture * 1000.f)
                {
                    m_settings.camera_aperture = aperture / 1000.f;
                    perspective_camera->SetAperture(m_settings.camera_aperture);
                    update = true;
                }

                if (focus_distance != m_settings.camera_focus_distance)
                {
                    m_settings.camera_focus_distance = focus_distance;
                    perspective_camera->SetFocusDistance(m_settings.camera_focus_distance);
                    update = true;
                }

                if (focal_length != m_settings.camera_focal_length * 1000.f)
                {
                    m_settings.camera_focal_length = focal_length / 1000.f;
                    perspective_camera->SetFocalLength(m_settings.camera_focal_length);
                    update = true;
                }

                ImGui::SliderFloat("Aperture(mm)", &aperture, 0.0f, 100.0f);
                ImGui::SliderFloat("Focal length(mm)", &focal_length, 5.f, 200.0f);
                ImGui::SliderFloat("Focus distance(m)", &focus_distance, 0.05f, 20.f);
            }

            if (num_bounces != m_settings.num_bounces)
            {
                m_settings.num_bounces = num_bounces;
                app_render_->SetNumBounces(num_bounces);
                update = true;
            }

            auto gui_out_type = static_cast<Baikal::OutputType>(output);

            if (gui_out_type != app_render_->GetOutputType())
            {
                app_render_->SetOutputType(gui_out_type);
                update = true;
            }

            RadeonRays::float3 eye, at;
            eye = camera->GetPosition();
            at = eye + camera->GetForwardVector();

            ImGui::Combo("Output", &output, outputs);
            ImGui::Text(" ");
            ImGui::Text("Number of samples: %d", m_settings.samplecount);
            ImGui::Text("Frame time %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Renderer performance %.3f Msamples/s", (ImGui::GetIO().Framerate *m_settings.width * m_settings.height) / 1000000.f);
            ImGui::Text("Eye: x = %.3f y = %.3f z = %.3f", eye.x, eye.y, eye.z);
            ImGui::Text("At: x = %.3f y = %.3f z = %.3f", at.x, at.y, at.z);
            ImGui::Separator();

            if (m_settings.time_benchmark)
            {
                ImGui::ProgressBar(m_settings.samplecount / 512.f);
            }

            static decltype(std::chrono::high_resolution_clock::now()) time_bench_start_time;
            if (!m_settings.time_benchmark && !m_settings.benchmark)
            {
                if (ImGui::Button("Start benchmark") && m_settings.num_samples == -1)
                {
                    time_bench_start_time = std::chrono::high_resolution_clock::now();
                    m_settings.time_benchmark = true;
                    update = true;
                }

                if (!m_settings.time_benchmark && ImGui::Button("Start RT benchmark"))
                {
                    m_settings.benchmark = true;
                }
            }

            if (m_settings.time_benchmark && m_settings.samplecount > 511)
            {
                m_settings.time_benchmark = false;
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>
                    (std::chrono::high_resolution_clock::now() - time_bench_start_time).count();
                m_settings.time_benchmark_time = delta / 1000.f;
                m_settings.time_benchmarked = true;
            }

            if (m_settings.time_benchmarked)
            {
                auto minutes = (int)(m_settings.time_benchmark_time / 60.f);
                auto seconds = (int)(m_settings.time_benchmark_time - minutes * 60);
                ImGui::Separator();

                RadeonRays::float3 color;
                std::string rating;
                ImGui::Text("Rendering time: %2dmin:%ds", minutes, seconds);
                if (GradeTimeBenchmarkResults(m_settings.modelname, minutes * 60 + seconds, rating, color))
                {
                    ImGui::TextColored(ImVec4(color.x, color.y, color.z, color.w), "Rating: %s", rating.c_str());
                }
                else
                {
                    ImGui::Text("Rating: N/A");
                }
            }

            if (m_settings.rt_benchmarked)
            {
                auto& stats = m_settings.stats;

                ImGui::Separator();
                ImGui::Text("Primary rays: %f Mrays/s", stats.primary_throughput * 1e-6f);
                ImGui::Text("Secondary rays: %f Mrays/s", stats.secondary_throughput * 1e-6f);
                ImGui::Text("Shadow rays: %f Mrays/s", stats.shadow_throughput * 1e-6f);
            }

#ifdef ENABLE_DENOISER
            ImGui::Separator();

            static float sigmaPosition = app_render_->GetDenoiserFloatParam("position_sensitivity").x;
            static float sigmaNormal = app_render_->GetDenoiserFloatParam("normal_sensitivity").x;
            static float sigmaColor = app_render_->GetDenoiserFloatParam("color_sensitivity").x;

            ImGui::Text("Denoiser settings");
            ImGui::SliderFloat("Position sigma", &sigmaPosition, 0.f, 0.3f);
            ImGui::SliderFloat("Normal sigma", &sigmaNormal, 0.f, 5.f);
            ImGui::SliderFloat("Color sigma", &sigmaColor, 0.f, 5.f);       

            if (app_render_->GetDenoiserFloatParam("position_sensitivity").x != sigmaPosition ||
                app_render_->GetDenoiserFloatParam("normal_sensitivity").x != sigmaNormal ||
                app_render_->GetDenoiserFloatParam("color_sensitivity").x != sigmaColor)
            {
                app_render_->SetDenoiserFloatParam("position_sensitivity", sigmaPosition);
                app_render_->SetDenoiserFloatParam("normal_sensitivity", sigmaNormal);
                app_render_->SetDenoiserFloatParam("color_sensitivity", sigmaColor);
            }
#endif
            ImGui::End();

            // Get shape/material info from renderer
            if (m_shape_id_future.valid())
            {
                m_current_shape_id = m_shape_id_future.get();
                auto shape = app_render_->GetShapeById(m_current_shape_id);
                m_material_selector = nullptr;

                if (shape)
                {
                    // set basic material settings
                    MaterialSettings material_settings;
                    material_settings.id = m_current_shape_id;
                    m_material_settings.push_back(material_settings);
                    // set volume material settings
                    if (shape->GetVolumeMaterial())
                    {
                        MaterialSettings volume_settings;
                        volume_settings.id = m_current_shape_id;
                        m_volume_settings.push_back(volume_settings);
                    }

                    m_material_selector = std::make_unique<MaterialSelector>(MaterialSelector(shape->GetMaterial()));
                    m_object_name = shape->GetName();
                }
            }

            // Process double click event if it occured
            if (g_is_double_click)
            {
                m_shape_id_future = app_render_->GetShapeId((std::uint32_t)g_mouse_pos.x, (std::uint32_t)g_mouse_pos.y);
                g_is_double_click = false;
            }

            // draw material props
            if (m_material_selector)
            {
                ImGui::SetNextWindowSizeConstraints(ImVec2(380, 290), ImVec2(380, 290));
                ImGui::Begin("Material info", 0, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                if (!m_object_name.empty())
                {
                    ImGui::Text(m_object_name.c_str());
                }

                ImGui::Separator();
                ImGui::Text("Material:");

                bool is_scene_changed = false;

                auto settings = std::find_if(m_material_settings.begin(), m_material_settings.end(),
                    [&](const MaterialSettings& settings)
                    {
                        return (settings.id == m_current_shape_id);
                    });

                if (settings == m_material_settings.end())
                    throw std::runtime_error(
                        "Application::UpdateGui(...): there is no such shape id in material settings");

                auto material = m_material_selector->Get();
                for (size_t i = 0; i < material->GetNumInputs(); i++)
                {
                    ImGui::Separator();
                    auto input = material->GetInput(i);

                    if (settings->inputs_info.size() <= i)
                    {
                        settings->inputs_info.push_back(InputSettings());
                    }

                    auto input_info = settings->inputs_info[i];
                    for (const auto& supported_type : input.info.supported_types)
                    {
                        auto name = input.info.name;
                        switch (supported_type)
                        {
                            case Material::InputType::kFloat4:
                            {
                                auto result = ReadFloatInput(material, *settings, i);
                                is_scene_changed = is_scene_changed ? is_scene_changed : result;
                                break;
                            }
                            case Material::InputType::kTexture:
                            {
                                auto result = ReadTextruePath(material, *settings, i);
                                is_scene_changed = is_scene_changed ? is_scene_changed : result;
                                break;
                            }
                            case Material::InputType::kUint:
                            {
                                std::uint32_t input_value = input.value.uint_value;
                                ImGui::InputInt(name.c_str(), (int*)(&input_value));

                                if ((input.value.uint_value != input_value) &&
                                    (input.value.tex_value == nullptr))
                                {
                                    settings->inputs_info[i].SetInteger(input_value);
                                    material->SetInputValue(input.info.name, input_value);
                                    is_scene_changed = true;
                                }
                                break;
                            }
                            case Material::InputType::kMaterial:
                            {
                                if (ImGui::Button((std::string("Into: ") + name).c_str()))
                                {
                                    m_material_selector->SelectMaterial(input.value.mat_value);
                                }
                                break;
                            }
                        }
                    }
                }

                ImGui::Separator();
                if (!m_material_selector->IsRoot())
                {
                    if (ImGui::Button("Back"))
                    {
                        m_material_selector->GetParent();
                    }
                }

                // Get material type settings
                std::string material_info;
                MaterialAccessor material_accessor(m_material_selector->Get());
                for (const auto& iter : material_accessor.GetTypeInfo())
                {
                    material_info += iter;
                    material_info.push_back('\0');
                }

                int material_type_output = material_accessor.GetType();
                ImGui::Separator();
                ImGui::Combo("Material type", &material_type_output, material_info.c_str());

                if (material_accessor.GetType() != material_type_output)
                {
                    material_accessor.SetType(material_type_output);
                    is_scene_changed = true;
                }

                // process volume materials
                auto volume = app_render_->GetShapeById(m_current_shape_id)->GetVolumeMaterial();

                auto volume_settings = std::find_if(m_volume_settings.begin(), m_volume_settings.end(),
                    [&](const MaterialSettings& settings)
                {
                    return (settings.id == m_current_shape_id);
                });

                if (volume && volume_settings == m_volume_settings.end())
                    throw std::runtime_error(
                        "Application::UpdateGui(...): there is no volume settings");

                if ((volume == nullptr) && (ImGui::Button("Create volume")))
                {
                    auto new_volume = VolumeMaterial::Create();

                    new_volume->SetInputValue("absorption", RadeonRays::float4(.0f, .0f, .0f, .0f));
                    new_volume->SetInputValue("scattering", RadeonRays::float4(.0f, .0f, .0f, .0f));
                    new_volume->SetInputValue("emission", RadeonRays::float4(.0f, .0f, .0f, .0f));
                    new_volume->SetInputValue("g", RadeonRays::float4(.0f, .0f, .0f, .0f));

                    app_render_->GetShapeById(m_current_shape_id)->SetVolumeMaterial(new_volume);

                    MaterialSettings volume_settings;
                    volume_settings.id = m_current_shape_id;
                    m_volume_settings.push_back(volume_settings);

                    is_scene_changed = true;
                }

                ImGui::Separator();
                if (ImGui::Button("Save materials"))
                {
                    auto material_io{ Baikal::MaterialIo::CreateMaterialIoXML() };
                    material_io->SaveMaterialsFromScene(m_settings.path + "/materials.xml", *app_render_->GetScene());
                    material_io->SaveIdentityMapping(m_settings.path + "/mapping.xml", *app_render_->GetScene());
                }

                if (volume != nullptr)
                {
                    ImGui::Separator();
                    ImGui::Text("Volumes:");

                    for (auto i = 0u; i < volume->GetNumInputs(); i++)
                    {
                        if (volume_settings->inputs_info.size() <= i)
                        {
                            volume_settings->inputs_info.push_back(InputSettings());
                        }

                        auto supported_types = volume->GetInput(i).info.supported_types;
                        if (supported_types.find(Material::InputType::kFloat4) != supported_types.end())
                        {
                            auto result = ReadFloatInput(volume, *volume_settings, i, "volume");
                            is_scene_changed = is_scene_changed ? is_scene_changed : result;
                        }
                    }

                    if (ImGui::Button("Make world volume"))
                    {
                        app_render_->GetCamera()->SetVolume(volume);
                        is_scene_changed = true;
                    }
                }

                ImGui::End();

                if (is_scene_changed)
                {
                    app_render_->UpdateScene();
                }
            }


            ImGui::Render();
        }

        return update;
    }

    void Application::CollectSceneStats()
    {
        // Collect some scene statistics
        m_num_triangles = 0U;
        m_num_instances = 0U;
        {
            auto scene = app_render_->GetScene();
            auto shape_iter = scene->CreateShapeIterator();

            for (; shape_iter->IsValid(); shape_iter->Next())
            {
                auto shape = shape_iter->ItemAs<Baikal::Shape>();
                auto mesh = std::dynamic_pointer_cast<Baikal::Mesh>(shape);

                if (mesh)
                {
                    m_num_triangles += (int)(mesh->GetNumIndices() / 3);
                }
                else
                {
                    ++m_num_instances;
                }
            }
        }
    }
}