#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "SceneGraph/Collector/collector.h"

#include "math/matrix.h"
#include "math/bbox.h"

#include "vk_scoped_object.h"
#include "vk_texture.h"
#include "vk_render_target_manager.h"

namespace Baikal
{
    using namespace RadeonRays;

    #include "Kernels/VK/common_structures.glsl"

    struct VkwScene
    {
        typedef std::vector<vkw::VkScopedObject<VkSemaphore>> SemaphoreArray;
        typedef std::vector<vkw::RenderTarget> RenderTargetArray;

        constexpr static uint32_t invalid_idx = static_cast<uint32_t>(-1);

        VkwScene()
            : point_lights(VK_NULL_HANDLE)
            , spot_lights(VK_NULL_HANDLE)
            , directional_lights(VK_NULL_HANDLE)
            , point_lights_transforms(VK_NULL_HANDLE)
            , spot_lights_transforms(VK_NULL_HANDLE)
            , directional_lights_transforms(VK_NULL_HANDLE)
            , camera(VK_NULL_HANDLE)
            , mesh_transforms(VK_NULL_HANDLE)
            , mesh_vertex_buffer(VK_NULL_HANDLE)
            , mesh_index_buffer(VK_NULL_HANDLE)
            , ibl_skylight_diffuse(VK_NULL_HANDLE)
            , raytrace_shape_buffer(VK_NULL_HANDLE)
            , raytrace_material_buffer(VK_NULL_HANDLE)
            , raytrace_lights_buffer(VK_NULL_HANDLE)
            , raytrace_RNG_buffer(VK_NULL_HANDLE)
            , sh_grid(VK_NULL_HANDLE)
            , vertex_count(0)
            , index_count(0)
            , shapes_count(0)
            , light_count(0)
            , num_point_lights(0)
            , num_spot_lights(0)
            , num_directional_lights(0)
            , sh_count(0)
            , env_map_idx(invalid_idx)
            , rebuild_deferred_pass(true)
            , rebuild_mrt_pass(true)
        {}

        typedef matrix mat4;

        struct Material
        {
            uint32_t layers; // Values from UberV2Material::Layers

            struct Value
            {
                bool isTexture = false;
                float3 color;
                uint32_t texture_id;
            };

            Value diffuse_color;

            Value reflection_roughness;
            Value reflection_metalness;
            Value reflection_ior;

            Value transparency;

            Value shading_normal;
        };

        struct VkwMesh
        {
            uint32_t index_base;
            uint32_t index_count;
            uint32_t vertex_count;
            uint32_t material_id;
        };

        vkw::VkScopedObject<VkBuffer>   point_lights;
        vkw::VkScopedObject<VkBuffer>   spot_lights;
        vkw::VkScopedObject<VkBuffer>   directional_lights;

        vkw::VkScopedObject<VkBuffer>   point_lights_transforms;
        vkw::VkScopedObject<VkBuffer>   spot_lights_transforms;
        vkw::VkScopedObject<VkBuffer>   directional_lights_transforms;

        vkw::VkScopedObject<VkBuffer>   camera;

        vkw::VkScopedObject<VkBuffer>                   mesh_bound_volumes;
        std::vector<vkw::VkScopedObject<VkBuffer>>      mesh_transforms;
        std::vector<vkw::VkScopedObject<VkBuffer>>      prev_mesh_transforms;
        vkw::VkScopedObject<VkBuffer>                   mesh_vertex_buffer;
        vkw::VkScopedObject<VkBuffer>                   mesh_index_buffer;

        vkw::Texture                    ibl_skylight_reflections;
        vkw::Texture                    ibl_brdf_lut;
        vkw::VkScopedObject<VkBuffer>   ibl_skylight_diffuse;

        vkw::VkScopedObject<VkBuffer>   raytrace_shape_buffer;
        vkw::VkScopedObject<VkBuffer>   raytrace_material_buffer;
        vkw::VkScopedObject<VkBuffer>   raytrace_lights_buffer;
        vkw::VkScopedObject<VkBuffer>   raytrace_RNG_buffer;

        vkw::VkScopedObject<VkBuffer>   env_map_irradiance_sh9;
        vkw::VkScopedObject<VkBuffer>   sh_grid;

        uint32_t                        vertex_count;
        uint32_t                        index_count;
        uint32_t                        shapes_count;
        uint32_t                        light_count;
        uint32_t                        num_point_lights;
        uint32_t                        num_spot_lights;
        uint32_t                        num_directional_lights;
        uint32_t                        sh_count;
        uint32_t                        env_map_idx;

        std::vector<vkw::Texture>       textures;
        std::vector<VkwMesh>            meshes;
        std::vector<Material>           materials;

        mutable SemaphoreArray          shadows_finished_signal;
        RenderTargetArray               shadows;

        std::unique_ptr<Bundle>         material_bundle;
        std::unique_ptr<Bundle>         volume_bundle;
        std::unique_ptr<Bundle>         texture_bundle;
        std::unique_ptr<Bundle>         input_map_leafs_bundle;
        std::unique_ptr<Bundle>         input_map_bundle;

        RadeonRays::float4              cascade_splits_dist;
        RadeonRays::bbox                scene_aabb;

        mutable bool                    rebuild_deferred_pass;
        mutable bool                    rebuild_mrt_pass;
    };
}
