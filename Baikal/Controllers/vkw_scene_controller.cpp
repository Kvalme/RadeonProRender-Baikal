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

    VkwSceneController::VkwSceneController(VkDevice device)
    : m_default_material(SingleBxdf::Create(SingleBxdf::BxdfType::kLambert))
    {

    }

    Material::Ptr VkwSceneController::GetDefaultMaterial() const
    {
        return m_default_material;
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
