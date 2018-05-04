#include "Controllers/vkw_scene_controller.h"
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


#include <chrono>
#include <memory>
#include <stack>
#include <vector>
#include <array>

using namespace RadeonRays;

namespace Baikal
{
    static std::size_t align16(std::size_t value)
    {
        return (value + 0xF) / 0x10 * 0x10;
    }

    VkwSceneController::VkwSceneController(VkDevice device, VkPhysicalDevice physical_device, uint32_t queue_family_index)
    : default_material_(SingleBxdf::Create(SingleBxdf::BxdfType::kLambert))
    , device_(device)
    , physical_device_(physical_device)
    {
        memory_allocator_ = std::unique_ptr<vkw::MemoryAllocator>(new vkw::MemoryAllocator(device, physical_device));
        memory_manager_ = std::unique_ptr<vkw::MemoryManager>(new vkw::MemoryManager(device, queue_family_index, *memory_allocator_.get()));
    }

    Material::Ptr VkwSceneController::GetDefaultMaterial() const
    {
        return default_material_;
    }

    VkwSceneController::~VkwSceneController()
    {
    }

    
    void VkwSceneController::UpdateIntersector(Scene1 const& scene, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateIntersectorTransforms(Scene1 const& scene, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateCamera(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const
    {
        PerspectiveCamera* camera = dynamic_cast<PerspectiveCamera*>(scene.GetCamera().get());

        if (camera == nullptr)
        {
            throw std::runtime_error("VkwSceneController supports only perspective cameras");
        }

        const float focal_length = camera->GetFocalLength();
        const float2 sensor_size = camera->GetSensorSize();

        float2 z_range = camera->GetDepthRange();

        // Nan-avoidance in perspective matrix
        z_range.x = std::max(z_range.x, std::numeric_limits<float>::epsilon());

        const float fovy = atan(sensor_size.y / (2.0f * focal_length));

        const float3 up = camera->GetUpVector();
        const float3 right = -camera->GetRightVector();
        const float3 forward = camera->GetForwardVector();
        const float3 pos = camera->GetPosition();

        const matrix proj = perspective_proj_fovy_rh_gl(fovy, camera->GetAspectRatio(), z_range.x, z_range.y);
        const float3 ip = float3(-dot(right, pos), -dot(up, pos), -dot(forward, pos));

        const matrix view = matrix( right.x, right.y, right.z, ip.x,
                                    up.x, up.y, up.z, ip.y,
                                    forward.x, forward.y, forward.z, ip.z,
                                    0.0f, 0.0f, 0.0f, 1.0f);

        const matrix view_proj = proj * view;

        VkwScene::Camera camera_internal;
        camera_internal.camera_position = camera->GetPosition();
        camera_internal.view_projection = view_proj;
        camera_internal.inv_view        = inverse(view);
        camera_internal.inv_projection  = inverse(proj);

        if (out.camera == VK_NULL_HANDLE)
        {
            out.camera = memory_manager_->CreateBuffer( sizeof(VkwScene::Camera),
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &camera_internal);
        }
        
        memory_manager_->WriteBuffer(out.camera, 0u, sizeof(VkwScene::Camera), &camera_internal);
    }

    void VkwSceneController::UpdateShapes(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateShapeProperties(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& volume_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateCurrentScene(Scene1 const& scene, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateMaterials(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateVolumes(Scene1 const& scene, Collector& volume_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateTextures(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::WriteMaterial(Material const& material, Collector& mat_collector, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::WriteLight(Scene1 const& scene, Light const& light, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::UpdateLights(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::WriteTexture(Texture const& texture, std::size_t data_offset, void* data) const
    {
    }

    void VkwSceneController::WriteTextureData(Texture const& texture, void* data) const
    {
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
    
}
