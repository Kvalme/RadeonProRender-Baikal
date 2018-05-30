#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "SceneGraph/Collector/collector.h"

#include "math/matrix.h"
#include "vk_scoped_object.h"
#include "vk_texture.h"

namespace Baikal
{
    using namespace RadeonRays;

    struct VkwScene
    {
        VkwScene() 
        : lights(VK_NULL_HANDLE)
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
        , sh_count(0)
        {}

        typedef matrix mat4;

        #include "Kernels/VK/common.glsl"

        struct VkwMesh
        {
            uint32_t                index_base;
            uint32_t                index_count;

            VkDescriptorSet         descriptor_set;
            MaterialConstants       material_constants;
        };

        struct VkwTexture
        {
            vkw::VkScopedObject<VkImage> image;
            vkw::VkScopedObject<VkImageView> image_view;
        };

        vkw::VkScopedObject<VkBuffer>   lights;
        vkw::VkScopedObject<VkBuffer>   camera;

        vkw::VkScopedObject<VkBuffer>   mesh_transforms;
        vkw::VkScopedObject<VkBuffer>   mesh_vertex_buffer;
        vkw::VkScopedObject<VkBuffer>   mesh_index_buffer;

        //vkw::Texture                  ibl_skylight_reflections;
        vkw::VkScopedObject<VkBuffer>   ibl_skylight_diffuse;

        vkw::VkScopedObject<VkBuffer>   raytrace_shape_buffer;
        vkw::VkScopedObject<VkBuffer>   raytrace_material_buffer;
        vkw::VkScopedObject<VkBuffer>   raytrace_lights_buffer;
        vkw::VkScopedObject<VkBuffer>   raytrace_RNG_buffer;

        vkw::VkScopedObject<VkBuffer>   sh_grid;

        uint32_t                        vertex_count;
        uint32_t                        index_count;
        uint32_t                        shapes_count;
        uint32_t                        light_count;
        uint32_t                        sh_count;

        std::vector<vkw::Texture>       textures;
        std::vector<VkwMesh>            meshes;

        std::unique_ptr<Bundle>         material_bundle;
        std::unique_ptr<Bundle>         volume_bundle;
        std::unique_ptr<Bundle>         texture_bundle;
        std::unique_ptr<Bundle>         input_map_leafs_bundle;
        std::unique_ptr<Bundle>         input_map_bundle;
    };
}
