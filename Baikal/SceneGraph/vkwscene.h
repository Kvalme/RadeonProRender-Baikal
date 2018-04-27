#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "SceneGraph/Collector/collector.h"

#include "math/float3.h"

namespace Baikal
{
    using namespace RadeonRays;

    struct VkwScene
    {
        typedef matrix mat4;

        #include "Kernels/VK/common.glsl"

        struct VkwMesh
        {
            uint32_t                index_base;
            uint32_t                index_count;

            VkDescriptorSet         descriptor_set;
            MaterialConstants       material_constants;
        };

        VkBuffer                    lights;
        VkBuffer                    camera;

        VkBuffer                    mesh_transforms;
        VkBuffer                    vertex_buffer;
        VkBuffer                    index_buffer;

        //vkw::Texture                ibl_skylight_reflections;
        VkBuffer                    ibl_skylight_diffuse;

        VkBuffer                    texture_data;
        VkBuffer                    texture_desc;

        VkBuffer                    raytrace_shape_buffer;
        VkBuffer                    raytrace_material_buffer;
        VkBuffer                    raytrace_lights_buffer;
        VkBuffer                    raytrace_RNG_buffer;

        VkBuffer                    sh_grid;

        uint32_t                    light_count;
        uint32_t                    sh_count;

        //std::vector<vkw::Texture>   textures;
        std::vector<VkwMesh>        meshes;

        std::unique_ptr<Bundle>     material_bundle;
        std::unique_ptr<Bundle>     volume_bundle;
        std::unique_ptr<Bundle>     texture_bundle;
    };
}