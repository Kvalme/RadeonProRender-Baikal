#include "Controllers/vkw_scene_controller.h"

#include "Controllers/vkw_scene_helpers.h"

#include "SceneGraph/scene1.h"
#include "SceneGraph/camera.h"
#include "SceneGraph/light.h"
#include "SceneGraph/shape.h"
#include "SceneGraph/material.h"
#include "SceneGraph/texture.h"
#include "SceneGraph/Collector/collector.h"
#include "SceneGraph/iterator.h"
#include "SceneGraph/uberv2material.h"
#include "SceneGraph/inputmaps.h"
#include "Utils/distribution1d.h"
#include "Utils/log.h"

#include "math/bbox.h"

#include <chrono>
#include <memory>
#include <stack>
#include <vector>
#include <array>

using namespace RadeonRays;

namespace Baikal
{
    VkwSceneController::VkwSceneController(VkDevice device, VkPhysicalDevice physical_device, vkw::MemoryAllocator& memory_allocator,
        vkw::MemoryManager& memory_manager,
        vkw::ShaderManager& shader_manager,
        vkw::RenderTargetManager& render_target_manager,
        vkw::PipelineManager& pipeline_manager,
        vkw::ExecutionManager& execution_manager,
        uint32_t graphics_queue_index,
        uint32_t compute_queue_index)
        : memory_allocator_(memory_allocator)
        , memory_manager_(memory_manager)
        , render_target_manager_(render_target_manager)
        , shader_manager_(shader_manager)
        , pipeline_manager_(pipeline_manager)
        , execution_manager_(execution_manager)
        , device_(device)
        , physical_device_(physical_device)
        , shapes_changed_(false)
        , camera_changed_(false)
    {
        shadow_controller_.reset(new VkwShadowController(device, memory_manager, shader_manager, render_target_manager, pipeline_manager, graphics_queue_index, compute_queue_index));
        probe_controller_.reset(new VkwProbeController(device, memory_manager, shader_manager, render_target_manager, pipeline_manager, execution_manager, graphics_queue_index, compute_queue_index));
    }

    Material::Ptr VkwSceneController::GetDefaultMaterial() const
    {
        return default_material_;
    }

    VkwSceneController::~VkwSceneController()
    {
    }

    void VkwSceneController::UpdateCamera(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const
    {
        PerspectiveCamera* camera = dynamic_cast<PerspectiveCamera*>(scene.GetCamera().get());

        if (camera == nullptr)
        {
            throw std::runtime_error("VkwSceneController supports only perspective camera");
        }
        
        const matrix proj = MakeProjectionMatrix(*camera);
        const matrix view = MakeViewMatrix(*camera);
        const matrix view_proj = proj * view;

        const float focal_length = camera->GetFocalLength();
        const float2 sensor_size = camera->GetSensorSize();
        const float fovy = atan(sensor_size.y / (2.0f * focal_length));

        VkCamera camera_internal;
        camera_internal.position = camera->GetPosition();
        camera_internal.view_proj = view_proj;
        camera_internal.prev_view_proj = prev_view_proj_;
        camera_internal.view = view;
        camera_internal.inv_view = inverse(view);
        camera_internal.inv_proj = inverse(proj);
        camera_internal.inv_view_proj = inverse(view_proj);
        camera_internal.params = RadeonRays::float4(camera->GetAspectRatio(), fovy);

        prev_view_proj_ = view_proj;

        if (out.camera == VK_NULL_HANDLE)
        {
            out.camera.reset();

            out.camera = memory_manager_.CreateBuffer(sizeof(VkCamera),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &camera_internal);
        }

        memory_manager_.WriteBuffer(out.camera.get(), 0u, sizeof(VkCamera), &camera_internal);

        camera_changed_ = true;
    }

    void VkwSceneController::UpdateShapes(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const
    {
        shapes_changed_ = true;

        uint32_t num_shapes = static_cast<uint32_t>(scene.GetNumShapes());
        uint32_t num_vertices = 0;
        uint32_t num_indices = 0;

        vertex_buffer_.clear();
        index_buffer_.clear();
        mesh_transforms_.clear();
        mesh_bound_volumes_.clear();

        out.meshes.clear();

        RadeonRays::bbox scene_bounds;

        std::unique_ptr<Iterator> mesh_iter(scene.CreateShapeIterator());

        uint32_t num_transform_buffers = static_cast<uint32_t>(scene.GetNumShapes() / kMaxTransforms) + 1;

        out.mesh_transforms.clear();
        out.mesh_transforms.resize(num_transform_buffers);

        uint32_t mesh_idx = 0;

        for (; mesh_iter->IsValid(); mesh_iter->Next())
        {
            Baikal::Shape::Ptr shape = mesh_iter->ItemAs<Baikal::Shape>();
            Baikal::Mesh const* mesh = dynamic_cast<Baikal::Mesh*>(shape.get());

            if (mesh == nullptr)
                continue;

            auto material = (shape->GetMaterial() == nullptr) ? GetDefaultMaterial() : shape->GetMaterial();

            VkwScene::VkwMesh vkw_mesh = { static_cast<uint32_t>(num_indices), static_cast<uint32_t>(mesh->GetNumIndices()), static_cast<uint32_t>(mesh->GetNumVertices()), mat_collector.GetItemIndex(material) };
            out.meshes.push_back(vkw_mesh);

            RadeonRays::bbox mesh_bb;

            for (std::size_t v = 0; v < mesh->GetNumVertices(); v++)
            {
                RadeonRays::float3 pos = mesh->GetTransform() * mesh->GetVertices()[v];

                mesh_bb.grow(pos);
                scene_bounds.grow(pos);

                Vertex vertex = { pos, mesh->GetNormals()[v], mesh->GetUVs()[v] };
                vertex_buffer_.push_back(vertex);
            }

            mesh_bound_volumes_.push_back(mesh_bb);

            for (std::size_t idx = 0; idx < mesh->GetNumIndices(); idx++)
            {
                index_buffer_.push_back(mesh->GetIndices()[idx] + num_vertices);
            }

            mesh_transforms_.push_back(mesh->GetTransform());

            if (mesh_transforms_.size() == kMaxTransforms)
            {
                uint32_t buffer_idx = mesh_idx / (kMaxTransforms + 1);

                if (out.mesh_transforms[buffer_idx] == VK_NULL_HANDLE)
                {
                    out.mesh_transforms[buffer_idx].reset();

                    out.mesh_transforms[buffer_idx] = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix) * mesh_transforms_.size(),
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);
                }

                memory_manager_.WriteBuffer(out.mesh_transforms[buffer_idx].get(), 0u, sizeof(RadeonRays::matrix) * mesh_transforms_.size(), mesh_transforms_.data());
                mesh_transforms_.clear();
            }

            num_vertices += static_cast<uint32_t>(mesh->GetNumVertices());
            num_indices += static_cast<uint32_t>(mesh->GetNumIndices());

            assert(mesh->GetNumVertices() == mesh->GetNumNormals());
            assert(mesh->GetNumVertices() == mesh->GetNumUVs());

            mesh_idx++;
        }

        if (!mesh_transforms_.empty())
        {
            uint32_t buffer_idx = mesh_idx / kMaxTransforms;

            if (out.mesh_transforms[buffer_idx] == VK_NULL_HANDLE)
            {
                out.mesh_transforms[buffer_idx].reset();

                out.mesh_transforms[buffer_idx] = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix) * mesh_transforms_.size(),
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);
            }

            memory_manager_.WriteBuffer(out.mesh_transforms[buffer_idx].get(), 0u, sizeof(RadeonRays::matrix) * mesh_transforms_.size(), mesh_transforms_.data());
            mesh_transforms_.clear();
        }

        if (out.vertex_count < num_vertices)
        {
            out.mesh_vertex_buffer.reset();

            out.mesh_vertex_buffer = memory_manager_.CreateBuffer(sizeof(Vertex) * num_vertices,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr);

            out.vertex_count = num_vertices;
        }

        if (out.index_count < num_indices)
        {
            out.mesh_index_buffer.reset();

            out.mesh_index_buffer = memory_manager_.CreateBuffer(sizeof(uint32_t) * num_indices,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nullptr);

            out.index_count = num_indices;
        }

        if (out.shapes_count < num_shapes)
        {
            out.mesh_bound_volumes.reset();

            out.mesh_bound_volumes = memory_manager_.CreateBuffer(sizeof(RadeonRays::bbox) * num_shapes,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);

            out.shapes_count = num_shapes;
        }

        memory_manager_.WriteBuffer(out.mesh_vertex_buffer.get(), 0u, sizeof(Vertex) * num_vertices, vertex_buffer_.data());
        memory_manager_.WriteBuffer(out.mesh_index_buffer.get(), 0u, sizeof(uint32_t) * num_indices, index_buffer_.data());
        memory_manager_.WriteBuffer(out.mesh_bound_volumes.get(), 0u, sizeof(RadeonRays::bbox) * num_shapes, mesh_bound_volumes_.data());

        out.scene_aabb = scene_bounds;
        out.rebuild_mrt_pass = true;
    }

    void VkwSceneController::UpdateShapeProperties(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& volume_collector, VkwScene& out) const
    {
        shapes_changed_ = true;

        mesh_transforms_.clear();

        uint32_t mesh_idx = 0;

        std::unique_ptr<Iterator> mesh_iter(scene.CreateShapeIterator());
        for (; mesh_iter->IsValid(); mesh_iter->Next())
        {
            Baikal::Shape::Ptr shape = mesh_iter->ItemAs<Baikal::Shape>();
            Baikal::Mesh const* mesh = dynamic_cast<Baikal::Mesh*>(shape.get());

            if (mesh == nullptr)
                continue;

            mesh_transforms_.push_back(mesh->GetTransform());

            if (mesh_transforms_.size() == kMaxTransforms)
            {
                uint32_t buffer_idx = mesh_idx / (kMaxTransforms + 1);

                if (out.mesh_transforms[buffer_idx] == VK_NULL_HANDLE)
                {
                    out.mesh_transforms[buffer_idx].reset();

                    out.mesh_transforms[buffer_idx] = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix) * mesh_transforms_.size(),
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);
                }

                memory_manager_.WriteBuffer(out.mesh_transforms[buffer_idx].get(), 0u, sizeof(RadeonRays::matrix) * mesh_transforms_.size(), mesh_transforms_.data());
                mesh_transforms_.clear();
            }

            mesh_transforms_.push_back(mesh->GetTransform());
            mesh_idx++;
        }

        if (!mesh_transforms_.empty())
        {
            uint32_t buffer_idx = mesh_idx / kMaxTransforms;

            if (out.mesh_transforms[buffer_idx] == VK_NULL_HANDLE)
            {
                out.mesh_transforms[buffer_idx].reset();

                out.mesh_transforms[buffer_idx] = memory_manager_.CreateBuffer(sizeof(RadeonRays::matrix) * mesh_transforms_.size(),
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);
            }

            memory_manager_.WriteBuffer(out.mesh_transforms[buffer_idx].get(), 0u, sizeof(RadeonRays::matrix) * mesh_transforms_.size(), mesh_transforms_.data());
            mesh_transforms_.clear();
        }

        out.rebuild_mrt_pass = true;
    }

    void VkwSceneController::UpdateCurrentScene(Scene1 const& scene, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateMaterials(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
        // Get new buffer size
        std::size_t material_buffer_size = mat_collector.GetNumItems();
        if (material_buffer_size == 0)
        {
            return;
        }

        // Resize texture cache if needed
        if (out.materials.size() < material_buffer_size)
        {
            out.materials.resize(material_buffer_size);
        }

        // Update material bundle first to be able to track differences
        out.material_bundle.reset(mat_collector.CreateBundle());

        // Create texture iterator
        std::unique_ptr<Iterator> mat_iter(mat_collector.CreateIterator());

        // Iterate and serialize
        std::size_t num_materials_written = 0;
        for (; mat_iter->IsValid(); mat_iter->Next())
        {
            auto mat = mat_iter->ItemAs<Baikal::Material>();

            WriteMaterial(*mat, mat_collector, tex_collector, out.materials.data() + num_materials_written);

            num_materials_written++;
        }
    }

    void VkwSceneController::UpdateVolumes(Scene1 const& scene, Collector& volume_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateTextures(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
        // Get new buffer size
        std::size_t tex_buffer_size = tex_collector.GetNumItems();
        if (tex_buffer_size == 0)
        {
            return;
        }

        // Resize texture cache if needed
        if (out.textures.size() < tex_buffer_size)
        {
            out.textures.resize(tex_buffer_size);
        }

        // Update material bundle first to be able to track differences
        out.texture_bundle.reset(tex_collector.CreateBundle());

        // Create texture iterator
        std::unique_ptr<Iterator> tex_iter(tex_collector.CreateIterator());

        // Iterate and serialize
        std::size_t num_textures_written = 0;
        for (; tex_iter->IsValid(); tex_iter->Next())
        {
            auto tex = tex_iter->ItemAs<Texture>();

            WriteTexture(*tex, out.textures[num_textures_written]);

            ++num_textures_written;
        }
    }

    void VkwSceneController::WriteMaterial(Material const& material, Collector& mat_collector, Collector& tex_collector, void* data) const
    {
        VkwScene::Material *mat = static_cast<VkwScene::Material*>(data);
        const UberV2Material &ubermaterial = static_cast<const UberV2Material&>(material);
        mat->layers = ubermaterial.GetLayers();

        auto input_to_material_value = [&](Material::InputValue input_value) -> VkwScene::Material::Value
        {
            assert(input_value.type == Material::InputType::kInputMap);
            // We only support leaf nodes atm, so proceed them and assert if we have any other node
            switch (input_value.input_map_value->m_type)
            {
            case InputMap::InputMapType::kConstantFloat:
            {
                InputMap_ConstantFloat *i = static_cast<InputMap_ConstantFloat*>(input_value.input_map_value.get());
                VkwScene::Material::Value value;
                value.isTexture = false;
                value.color.x = i->GetValue();
                return value;
            }
            case InputMap::InputMapType::kConstantFloat3:
            {
                InputMap_ConstantFloat3 *i = static_cast<InputMap_ConstantFloat3*>(input_value.input_map_value.get());
                VkwScene::Material::Value value;
                value.isTexture = false;
                value.color = i->GetValue();
                return value;
            }
            case InputMap::InputMapType::kSampler:
            {
                InputMap_Sampler *i = static_cast<InputMap_Sampler*>(input_value.input_map_value.get());
                VkwScene::Material::Value value;
                value.isTexture = true;
                value.texture_id = tex_collector.GetItemIndex(i->GetTexture());
                return value;
            }
            case InputMap::InputMapType::kSamplerBumpmap:
            {
                InputMap_SamplerBumpMap *i = static_cast<InputMap_SamplerBumpMap*>(input_value.input_map_value.get());
                VkwScene::Material::Value value;
                value.isTexture = true;
                value.texture_id = tex_collector.GetItemIndex(i->GetTexture());
                return value;
            };
            default:
                assert(!"Unsupported input map type");
            }
            return  VkwScene::Material::Value();
        };

        if ((mat->layers & UberV2Material::Layers::kDiffuseLayer) == UberV2Material::Layers::kDiffuseLayer)
        {
            mat->diffuse_color = input_to_material_value(material.GetInputValue("uberv2.diffuse.color"));
        }

        if ((mat->layers & UberV2Material::Layers::kReflectionLayer) == UberV2Material::Layers::kReflectionLayer)
        {
            mat->reflection_color = input_to_material_value(material.GetInputValue("uberv2.reflection.color"));
            mat->reflection_ior = input_to_material_value(material.GetInputValue("uberv2.reflection.roughness"));
            mat->reflection_roughness = input_to_material_value(material.GetInputValue("uberv2.reflection.ior"));
        }

        if ((mat->layers & UberV2Material::Layers::kTransparencyLayer) == UberV2Material::Layers::kTransparencyLayer)
        {
            mat->transparency = input_to_material_value(material.GetInputValue("uberv2.transparency"));
        }

        if ((mat->layers & UberV2Material::Layers::kShadingNormalLayer) == UberV2Material::Layers::kShadingNormalLayer)
        {
            mat->shading_normal = input_to_material_value(material.GetInputValue("uberv2.shading_normal"));
        }
    }

    void VkwSceneController::WriteLight(Scene1 const& scene, Light const& light, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::UpdateLights(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
        uint32_t num_lights = static_cast<uint32_t>(scene.GetNumLights());

        lights_changed_.resize(num_lights);

        uint32_t num_spot_lights = 0;
        uint32_t num_point_lights = 0;
        uint32_t num_directional_lights = 0;

        std::vector<VkSpotLight> spot_lights;
        std::vector<VkPointLight> point_lights;
        std::vector<VkDirectionalLight> directional_lights;

        spot_lights.reserve(static_cast<size_t>(num_lights));
        point_lights.reserve(static_cast<size_t>(num_lights));
        directional_lights.reserve(static_cast<size_t>(num_lights));

        std::unique_ptr<Iterator> light_iter(scene.CreateLightIterator());

        uint32_t global_light_idx = 0;

        for (; light_iter->IsValid(); light_iter->Next())
        {
            auto light = light_iter->ItemAs<Light>();

            auto light_type = GetLightType(*light);

            switch (light_type)
            {
            case kPoint:
            {
                lights_changed_[global_light_idx] = light->IsDirty();

                float3 p = light->GetPosition();
                float3 r = light->GetEmittedRadiance();

                VkPointLight point_light = { p, float4(r.x, r.y, r.z, static_cast<float>(global_light_idx++)) };
                point_lights.push_back(point_light);

                num_point_lights++;

                break;
            };

            case kDirectional:
            {
                lights_changed_[global_light_idx] = light->IsDirty();

                float3 r = light->GetEmittedRadiance();

                VkDirectionalLight directional_light = { light->GetDirection(), float4(r.x, r.y, r.z, static_cast<float>(global_light_idx++)) };
                directional_lights.push_back(directional_light);

                num_directional_lights++;

                break;
            };

            case kSpot:
            {
                lights_changed_[global_light_idx] = light->IsDirty();

                float3 p = light->GetPosition();
                float3 d = light->GetDirection();
                float3 r = light->GetEmittedRadiance();

                auto cone_shape = static_cast<SpotLight const&>(*light).GetConeShape();

                VkSpotLight spot_light =
                {
                    float4(p.x, p.y, p.z, cone_shape.x),
                    float4(d.x, d.y, d.z, cone_shape.y),
                    float4(r.x, r.y, r.z, static_cast<float>(global_light_idx++))
                };

                spot_lights.push_back(spot_light);

                num_spot_lights++;

                break;
            };

            case kIbl:
            {
                lights_changed_[global_light_idx++] = light->IsDirty();

                if (light->IsDirty())
                {
                    Baikal::ImageBasedLight *ibl = static_cast<Baikal::ImageBasedLight *>(light.get());
                    out.env_map_idx = tex_collector.GetItemIndex(ibl->GetTexture());
                    probe_controller_->PrefilterEnvMap(out);

                    out.rebuild_deferred_pass = true;
                }

                break;
            };

            case kArea:
            {
                lights_changed_[global_light_idx++] = light->IsDirty();

                break;
            };

            default:
            {
                break;
            };
            }
        }

        if (num_spot_lights > 0 && (out.spot_lights == VK_NULL_HANDLE || out.num_spot_lights < num_spot_lights))
        {
            out.spot_lights.reset();

            out.spot_lights = memory_manager_.CreateBuffer(sizeof(VkSpotLight) * num_spot_lights,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, spot_lights.data());
        }

        if (num_point_lights > 0 && (out.point_lights == VK_NULL_HANDLE || out.num_point_lights < num_point_lights))
        {
            out.point_lights.reset();

            out.point_lights = memory_manager_.CreateBuffer(sizeof(VkPointLight) * num_point_lights,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, point_lights.data());
        }

        if (num_directional_lights > 0 && (out.directional_lights == VK_NULL_HANDLE || out.num_directional_lights < num_directional_lights))
        {
            out.directional_lights.reset();

            out.directional_lights = memory_manager_.CreateBuffer(sizeof(VkDirectionalLight) * num_directional_lights,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, directional_lights.data());
        }

        if (num_spot_lights > 0)
            memory_manager_.WriteBuffer(out.spot_lights.get(), 0u, sizeof(VkSpotLight) * num_spot_lights, spot_lights.data());

        if (num_point_lights > 0)
            memory_manager_.WriteBuffer(out.point_lights.get(), 0u, sizeof(VkPointLight) * num_point_lights, point_lights.data());

        if (num_directional_lights > 0)
            memory_manager_.WriteBuffer(out.directional_lights.get(), 0u, sizeof(VkDirectionalLight) * num_directional_lights, directional_lights.data());

        out.light_count = num_lights;
        out.num_point_lights = num_point_lights;
        out.num_spot_lights = num_spot_lights;
        out.num_directional_lights = num_directional_lights;
    }

    VkFormat GetTextureFormat(const Texture &texture)
    {
        switch (texture.GetFormat())
        {
        case Texture::Format::kRgba8: return VK_FORMAT_R8G8B8A8_UNORM;
        case Texture::Format::kRgba16: return VK_FORMAT_R16G16B16A16_UNORM;
        case Texture::Format::kRgba32: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }

    void VkwSceneController::WriteTexture(Texture const& texture, vkw::Texture &vk_texture) const
    {
        auto sz = texture.GetSize();
        VkExtent3D size;
        size.width = sz.x;
        size.height = sz.y;
        size.depth = sz.z;

        VkFormat fmt = GetTextureFormat(texture);

        vk_texture.SetTexture(&memory_manager_, size, fmt, false, texture.GetSizeInBytes(), texture.GetData());
    }

    void VkwSceneController::WriteVolume(VolumeMaterial const& volume, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::UpdateSceneAttributes(Scene1 const& scene, Collector& tex_collector, VkwScene& out) const
    {
    }

    void Baikal::VkwSceneController::UpdateInputMaps(const Baikal::Scene1& scene, Baikal::Collector& input_map_collector, Collector& input_map_leafs_collector, VkwScene& out) const
    {
    }

    void Baikal::VkwSceneController::UpdateLeafsData(Scene1 const& scene, Collector& input_map_leafs_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void Baikal::VkwSceneController::WriteInputMapLeaf(InputMap const& leaf, Collector& tex_collector, void* data) const
    {
    }

    void Baikal::VkwSceneController::PostUpdate(Scene1 const& scene, VkwScene& out) const
    {
        shadow_controller_->UpdateShadows(shapes_changed_, camera_changed_, lights_changed_, scene, out);

        shapes_changed_ = false;
        camera_changed_ = false;

        //Range-for don't work for std::vector<bool> on gcc-7.3
        for (std::size_t a = 0; a < lights_changed_.size(); ++a)
        {
            lights_changed_[a] = false;
        }
    }
}