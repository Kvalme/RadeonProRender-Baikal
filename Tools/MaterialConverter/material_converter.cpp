#include "material_converter.h"

#include "BaikalOld/SceneGraph/scene1.h"
#include "BaikalOld/SceneGraph/iterator.h"

#include "SceneGraph/iterator.h"
#include "SceneGraph/inputmaps.h"

#include <iostream>
#include <fstream>

#ifdef USE_LOGFILE
std::ofstream logfile("log.txt");
#define LOG(x) logfile << x << std::endl
#else
#define LOG(x)
#endif

Baikal::Texture::Format MaterialConverter::TranslateFormat(BaikalOld::Texture::Format old_format)
{
    switch (old_format)
    {
    case BaikalOld::Texture::Format::kRgba8:
        return Baikal::Texture::Format::kRgba8;
    case BaikalOld::Texture::Format::kRgba16:
        return Baikal::Texture::Format::kRgba16;
    case BaikalOld::Texture::Format::kRgba32:
        return Baikal::Texture::Format::kRgba32;
    }
    return Baikal::Texture::Format::kRgba8;
}

namespace
{
    static const Baikal::InputMap_ConstantFloat::Ptr kGammaPower = Baikal::InputMap_ConstantFloat::Create(2.2f);
}

Baikal::InputMap::Ptr MaterialConverter::TranslateInput(BaikalOld::Material::Ptr old_mtl, std::string const& input_name)
{
    Baikal::InputMap::Ptr input_map = nullptr;
    auto old_input_value = old_mtl->GetInputValue(input_name);
    LOG("Translating input " << input_name << " of material " << old_mtl->GetName());
    switch (old_input_value.type)
    {
    case BaikalOld::Material::InputType::kUint:
        throw std::runtime_error("Cannot translate uint to input map!");
        break;
    case BaikalOld::Material::InputType::kMaterial:
        throw std::runtime_error("Cannot translate material to input map!");
        break;
    case BaikalOld::Material::InputType::kFloat4:
        LOG("Float4 value: (" << old_input_value.float_value.x << ", " << old_input_value.float_value.y << ", " << old_input_value.float_value.z << ")");
        input_map = Baikal::InputMap_ConstantFloat3::Create(old_input_value.float_value);
        break;
    case BaikalOld::Material::InputType::kTexture:
        // Check braces
    {
        LOG("Texture value: " << old_input_value.tex_value->GetName());
        RadeonRays::int2 old_size = old_input_value.tex_value->GetSize();
        auto new_texture = Baikal::Texture::Create(const_cast<char*>(old_input_value.tex_value->GetData()),
            RadeonRays::int3(old_size.x, old_size.y, 0),
            TranslateFormat(old_input_value.tex_value->GetFormat()));
        new_texture->SetName(old_input_value.tex_value->GetName());
        input_map = Baikal::InputMap_Sampler::Create(new_texture);
        if (input_name == "albedo")
        {
            // Convert texture from gamma to linear space
            input_map = Baikal::InputMap_Pow::Create(input_map, kGammaPower);
        }
    }
        break;
    default:
        throw std::runtime_error("Translation of unsupported input");
    }
    return input_map;
}

Baikal::UberV2Material::Ptr MaterialConverter::TranslateSingleBxdfMaterial(BaikalOld::SingleBxdf::Ptr old_mtl)
{
    LOG("Translating SingleBxdf material: " << old_mtl->GetName());
    auto uber_mtl = Baikal::UberV2Material::Create();
    uber_mtl->SetName(old_mtl->GetName());
    switch (old_mtl->GetBxdfType())
    {
    case BaikalOld::SingleBxdf::BxdfType::kZero:
        LOG("BxdfType: Zero");
        uber_mtl->SetLayers(Baikal::UberV2Material::kDiffuseLayer);
        // Black diffuse
        uber_mtl->SetInputValue("uberv2.diffuse.color", Baikal::InputMap_ConstantFloat3::Create(RadeonRays::float3(0.0f, 0.0f, 0.0f)));
        break;
    case BaikalOld::SingleBxdf::BxdfType::kLambert:
        LOG("BxdfType: Lambert");
        uber_mtl->SetLayers(Baikal::UberV2Material::kDiffuseLayer);
        uber_mtl->SetInputValue("uberv2.diffuse.color", TranslateInput(old_mtl, "albedo"));
        if (old_mtl->GetInputValue("normal").tex_value != nullptr)
        {
            LOG("Found normal");
            uber_mtl->SetLayers(Baikal::UberV2Material::kDiffuseLayer | Baikal::UberV2Material::kShadingNormalLayer);
            uber_mtl->SetInputValue("uberv2.shading_normal", Baikal::InputMap_Remap::Create(
                Baikal::InputMap_ConstantFloat3::Create(RadeonRays::float3(0.0f, 1.0f, 0.0f)),
                Baikal::InputMap_ConstantFloat3::Create(RadeonRays::float3(-1.0f, 1.0f, 0.0f)),
                TranslateInput(old_mtl, "normal")));
        }
        break;
    case BaikalOld::SingleBxdf::BxdfType::kIdealReflect:
    case BaikalOld::SingleBxdf::BxdfType::kMicrofacetBeckmann:
    case BaikalOld::SingleBxdf::BxdfType::kMicrofacetGGX:
        LOG("BxdfType: Reflection");
        uber_mtl->SetLayers(Baikal::UberV2Material::kReflectionLayer);
        uber_mtl->SetInputValue("uberv2.reflection.color", TranslateInput(old_mtl, "albedo"));
        uber_mtl->SetInputValue("uberv2.reflection.roughness", TranslateInput(old_mtl, "roughness"));
        break;
    case BaikalOld::SingleBxdf::BxdfType::kEmissive:
        LOG("BxdfType: Emission");
        uber_mtl->SetLayers(Baikal::UberV2Material::kEmissionLayer);
        uber_mtl->SetInputValue("uberv2.emission.color", TranslateInput(old_mtl, "albedo"));
        break;
    case BaikalOld::SingleBxdf::BxdfType::kPassthrough:
        LOG("BxdfType: Passthrough");
        // Nothing?
        break;
    case BaikalOld::SingleBxdf::BxdfType::kTranslucent:
        LOG("BxdfType: Translucensy");
        uber_mtl->SetLayers(Baikal::UberV2Material::kTransparencyLayer);
        // CHECK: albedo or (1 - albedo)?
        uber_mtl->SetInputValue("uberv2.transparency", TranslateInput(old_mtl, "albedo"));
        break;
    case BaikalOld::SingleBxdf::BxdfType::kIdealRefract:
    case BaikalOld::SingleBxdf::BxdfType::kMicrofacetRefractionGGX:
    case BaikalOld::SingleBxdf::BxdfType::kMicrofacetRefractionBeckmann:
        LOG("BxdfType: Refraction");
        uber_mtl->SetLayers(Baikal::UberV2Material::kRefractionLayer);
        uber_mtl->SetInputValue("uberv2.refraction.color", TranslateInput(old_mtl, "albedo"));
        uber_mtl->SetInputValue("uberv2.refraction.roughness", TranslateInput(old_mtl, "roughness"));
        break;
    default:
        throw std::runtime_error("TranslateSingleBxdfMaterial: You shouldn't get here");

    }
    return uber_mtl;
}

bool HasLayer(std::uint32_t all_layers, std::uint32_t layer)
{
    return (all_layers & layer) == layer;
}

Baikal::UberV2Material::Ptr MaterialConverter::MergeMaterials(Baikal::UberV2Material::Ptr base, Baikal::UberV2Material::Ptr top, Baikal::InputMap::Ptr blend_factor)
{
    LOG("Merge materials " << base->GetName() << " and " << top->GetName() << " into one");
    auto result = Baikal::UberV2Material::Create();
    std::uint32_t base_layers = base->GetLayers();
    std::uint32_t top_layers = top->GetLayers();
    std::uint32_t result_layers = 0;

    // Emission
    {
        bool base_has_emission = HasLayer(base_layers, Baikal::UberV2Material::kEmissionLayer);
        bool top_has_emission = HasLayer(top_layers, Baikal::UberV2Material::kEmissionLayer);

        if (base_has_emission && top_has_emission)
        {
            LOG("Base and top materials both have emission: merging as result = base + top");
            result_layers |= Baikal::UberV2Material::kEmissionLayer;
            auto base_emission = base->GetInputValue("uberv2.emission.color").input_map_value;
            auto top_emission = top->GetInputValue("uberv2.emission.color").input_map_value;
            auto emission_result = Baikal::InputMap_Add::Create(base_emission, top_emission);
            result->SetInputValue("uberv2.emission.color", emission_result);
        }
        else if (base_has_emission || top_has_emission)
        {
            result_layers |= Baikal::UberV2Material::kEmissionLayer;
            Baikal::InputMap::Ptr input_color = nullptr;
            if (base_has_emission)
            {
                LOG("Base material: emission");
                input_color = base->GetInputValue("uberv2.emission.color").input_map_value;
            }
            if (top_has_emission)
            {
                LOG("Top material:  emission");
                input_color = top->GetInputValue("uberv2.emission.color").input_map_value;
            }
            result->SetInputValue("uberv2.emission.color", input_color);
        }
    }

    // Transparency
    {
        bool base_has_transparency = HasLayer(base_layers, Baikal::UberV2Material::kTransparencyLayer);
        bool top_has_transparency = HasLayer(top_layers, Baikal::UberV2Material::kTransparencyLayer);

        if (base_has_transparency && top_has_transparency)
        {
            LOG("Base and top materials both have transparency: merging as result = base * top");
            result_layers |= Baikal::UberV2Material::kTransparencyLayer;
            auto base_transparency = base->GetInputValue("uberv2.transparency").input_map_value;
            auto top_transparency = top->GetInputValue("uberv2.transparency").input_map_value;
            auto transparency_result = Baikal::InputMap_Mul::Create(base_transparency, top_transparency);
            result->SetInputValue("uberv2.transparency", transparency_result);
        }
        else if (base_has_transparency || top_has_transparency)
        {
            result_layers |= Baikal::UberV2Material::kTransparencyLayer;
            Baikal::InputMap::Ptr input_transparency = nullptr;
            if (base_has_transparency)
            {
                LOG("Base material: transparency");
                input_transparency = base->GetInputValue("uberv2.transparency").input_map_value;
            }
            if (top_has_transparency)
            {
                LOG("Top material:  transparency");
                input_transparency = top->GetInputValue("uberv2.transparency").input_map_value;
            }
            result->SetInputValue("uberv2.transparency", input_transparency);
        }
    }

    // Coating
    {
        bool base_has_coating = HasLayer(base_layers, Baikal::UberV2Material::kCoatingLayer);
        bool top_has_coating = HasLayer(top_layers, Baikal::UberV2Material::kCoatingLayer);

        if (base_has_coating && top_has_coating)
        {
            throw std::runtime_error("Base and top materials both have coating layer: merging is not supported");
        }
        else if (base_has_coating || top_has_coating)
        {
            result_layers |= Baikal::UberV2Material::kCoatingLayer;
            Baikal::InputMap::Ptr input_color = nullptr;
            Baikal::InputMap::Ptr input_ior = nullptr;
            if (base_has_coating)
            {
                LOG("Base material: coating");
                input_color = base->GetInputValue("uberv2.coating.color").input_map_value;
                input_ior = base->GetInputValue("uberv2.coating.ior").input_map_value;
            }
            if (top_has_coating)
            {
                LOG("Top material: coating");
                input_color = top->GetInputValue("uberv2.coating.color").input_map_value;
                input_ior = top->GetInputValue("uberv2.coating.ior").input_map_value;
            }
            result->SetInputValue("uberv2.coating.color", input_color);
            result->SetInputValue("uberv2.coating.ior", input_ior);
        }
    }

    // Reflection
    {
        bool base_has_reflection = HasLayer(base_layers, Baikal::UberV2Material::kReflectionLayer);
        bool top_has_reflection = HasLayer(top_layers, Baikal::UberV2Material::kReflectionLayer);

        if (base_has_reflection && top_has_reflection)
        {
            LOG("Base and top materials both have reflection: merging as base - reflection, top - coating");
            // Base is reflection
            result_layers |= Baikal::UberV2Material::kReflectionLayer;
            Baikal::InputMap::Ptr base_color = base->GetInputValue("uberv2.reflection.color").input_map_value;
            Baikal::InputMap::Ptr base_roughness = base->GetInputValue("uberv2.reflection.roughness").input_map_value;
            Baikal::InputMap::Ptr base_ior = base->GetInputValue("uberv2.reflection.ior").input_map_value;
            result->SetInputValue("uberv2.reflection.color", base_color);
            result->SetInputValue("uberv2.reflection.roughness", base_roughness);
            result->SetInputValue("uberv2.reflection.ior", base_ior);

            // Top is coating
            result_layers |= Baikal::UberV2Material::kCoatingLayer;
            Baikal::InputMap::Ptr coating_color = top->GetInputValue("uberv2.reflection.color").input_map_value;
            result->SetInputValue("uberv2.coating.color", coating_color);
            result->SetInputValue("uberv2.coating.ior", blend_factor);
        }
        else if (base_has_reflection || top_has_reflection)
        {
            result_layers |= Baikal::UberV2Material::kReflectionLayer;
            Baikal::InputMap::Ptr input_color = nullptr;
            Baikal::InputMap::Ptr input_roughness = nullptr;
            if (base_has_reflection)
            {
                LOG("Base material: reflection");
                input_color = base->GetInputValue("uberv2.reflection.color").input_map_value;
                input_roughness = base->GetInputValue("uberv2.reflection.roughness").input_map_value;
            }
            if (top_has_reflection)
            {
                LOG("Top material:  reflection");
                input_color = top->GetInputValue("uberv2.reflection.color").input_map_value;
                input_roughness = top->GetInputValue("uberv2.reflection.roughness").input_map_value;
            }
            result->SetInputValue("uberv2.reflection.color", input_color);
            result->SetInputValue("uberv2.reflection.roughness", input_roughness);
            result->SetInputValue("uberv2.reflection.ior", blend_factor);
        }
    }

    // Diffuse
    {
        bool base_has_diffuse = HasLayer(base_layers, Baikal::UberV2Material::kDiffuseLayer);
        bool top_has_diffuse = HasLayer(top_layers, Baikal::UberV2Material::kDiffuseLayer);

        if (base_has_diffuse && top_has_diffuse)
        {
            LOG("Base and top materials both have diffuse layer: merging as result = lerp(base, top, factor)");
            result_layers |= Baikal::UberV2Material::kDiffuseLayer;
            Baikal::InputMap::Ptr base_diffuse = base->GetInputValue("uberv2.diffuse.color").input_map_value;
            Baikal::InputMap::Ptr top_diffuse = top->GetInputValue("uberv2.diffuse.color").input_map_value;
            result->SetInputValue("uberv2.diffuse.color", Baikal::InputMap_Lerp::Create(base_diffuse, top_diffuse, blend_factor));
        }
        else if (base_has_diffuse || top_has_diffuse)
        {
            result_layers |= Baikal::UberV2Material::kDiffuseLayer;
            Baikal::InputMap::Ptr input_color = nullptr;
            if (base_has_diffuse)
            {
                LOG("Base material: diffuse");
                input_color = base->GetInputValue("uberv2.diffuse.color").input_map_value;
            }
            if (top_has_diffuse)
            {
                LOG("Top material:  diffuse");
                input_color = top->GetInputValue("uberv2.diffuse.color").input_map_value;
            }
            result->SetInputValue("uberv2.diffuse.color", input_color);
        }
    }

    // Refraction
    {
        bool base_has_refraction = HasLayer(base_layers, Baikal::UberV2Material::kRefractionLayer);
        bool top_has_refraction = HasLayer(top_layers, Baikal::UberV2Material::kRefractionLayer);

        if (base_has_refraction && top_has_refraction)
        {
            throw std::runtime_error("Base and top materials both have refraction layer: refraction merging is not supported");
        }
        else if (base_has_refraction || top_has_refraction)
        {
            result_layers |= Baikal::UberV2Material::kRefractionLayer;
            Baikal::InputMap::Ptr input_color = nullptr;
            Baikal::InputMap::Ptr input_roughness = nullptr;
            if (base_has_refraction)
            {
                LOG("Base material: refraction");
                input_color = base->GetInputValue("uberv2.refraction.color").input_map_value;
                input_roughness = base->GetInputValue("uberv2.refraction.roughness").input_map_value;
            }
            if (top_has_refraction)
            {
                LOG("Top material:  refraction");
                input_color = top->GetInputValue("uberv2.refraction.color").input_map_value;
                input_roughness = top->GetInputValue("uberv2.refraction.roughness").input_map_value;
            }
            result->SetInputValue("uberv2.refraction.color", input_color);
            result->SetInputValue("uberv2.refraction.roughness", input_roughness);
            result->SetInputValue("uberv2.refraction.ior", blend_factor);
        }
    }

    // SSS
    {
        bool base_has_sss = HasLayer(base_layers, Baikal::UberV2Material::kSSSLayer);
        bool top_has_sss = HasLayer(top_layers, Baikal::UberV2Material::kSSSLayer);

        if (base_has_sss && top_has_sss)
        {
            throw std::runtime_error("SSS merging is not supported");
        }
    }

    // Shading normal
    {
        bool base_has_shading_normal = HasLayer(base_layers, Baikal::UberV2Material::kShadingNormalLayer);
        bool top_has_shading_normal = HasLayer(top_layers, Baikal::UberV2Material::kShadingNormalLayer);

        if (base_has_shading_normal && top_has_shading_normal)
        {
            throw std::runtime_error("Base and top materials both have shading normal layer: refraction merging is not supported");
        }
        else if (base_has_shading_normal || top_has_shading_normal)
        {
            result_layers |= Baikal::UberV2Material::kShadingNormalLayer;
            Baikal::InputMap::Ptr input_normal = nullptr;
            if (base_has_shading_normal)
            {
                LOG("Base material: shading normal");
                input_normal = base->GetInputValue("uberv2.shading_normal").input_map_value;
            }
            if (top_has_shading_normal)
            {
                LOG("Top material:  shading normal");
                input_normal = top->GetInputValue("uberv2.shading_normal").input_map_value;
            }
            result->SetInputValue("uberv2.shading_normal", input_normal);
        }
    }

    // Don't forget to set layers
    result->SetLayers(result_layers);
    return result;
}

Baikal::UberV2Material::Ptr MaterialConverter::TranslateFresnelBlend(BaikalOld::MultiBxdf::Ptr mtl)
{
    auto base = mtl->GetInputValue("base_material").mat_value;
    auto top = mtl->GetInputValue("top_material").mat_value;
    LOG("Translating fresnel blend of " << base->GetName() << " and " << top->GetName());

    // Translate this materials and get two uber materials
    // Then 'Merge' them to get single uber material
    auto base_new = TranslateMaterial(base);
    auto top_new = TranslateMaterial(top);
    return MergeMaterials(base_new, top_new, TranslateInput(mtl, "ior"));
}

Baikal::UberV2Material::Ptr MaterialConverter::TranslateMix(BaikalOld::MultiBxdf::Ptr mtl)
{
    // CHECK: Might it be different from fresnel blend?
    auto base = mtl->GetInputValue("base_material").mat_value;
    auto top = mtl->GetInputValue("top_material").mat_value;
    LOG("Translating mix of " << base->GetName() << " and " << top->GetName());

    // Translate this materials and get two uber materials
    // Then 'Merge' them to get single uber material
    auto base_new = TranslateMaterial(base);
    auto top_new = TranslateMaterial(top);
    return MergeMaterials(base_new, top_new, TranslateInput(mtl, "weight"));
}

Baikal::UberV2Material::Ptr MaterialConverter::TranslateMultiBxdfMaterial(BaikalOld::MultiBxdf::Ptr mtl)
{
    LOG("Translating MultiBxdf material " << mtl->GetName());
    switch (mtl->GetType())
    {
    case BaikalOld::MultiBxdf::Type::kLayered:
        throw std::exception("kLayered translation is not supported");
    case BaikalOld::MultiBxdf::Type::kFresnelBlend:
        return TranslateFresnelBlend(mtl);
    case BaikalOld::MultiBxdf::Type::kMix:
        return TranslateMix(mtl);
    }
    return nullptr;
}

Baikal::UberV2Material::Ptr MaterialConverter::TranslateMaterial(BaikalOld::Material::Ptr mtl)
{
    if (auto single_bxdf = std::dynamic_pointer_cast<BaikalOld::SingleBxdf>(mtl))
    {
        return TranslateSingleBxdfMaterial(single_bxdf);
    }
    else if (auto multi_bxdf = std::dynamic_pointer_cast<BaikalOld::MultiBxdf>(mtl))
    {
        return TranslateMultiBxdfMaterial(multi_bxdf);
    }
    else if (auto disney_bxdf = std::dynamic_pointer_cast<BaikalOld::DisneyBxdf>(mtl))
    {
        throw std::exception("Translation of disney bxdf materials is not supported");
    }
    else if (auto volume_mtl = std::dynamic_pointer_cast<BaikalOld::VolumeMaterial>(mtl))
    {
        throw std::exception("Translation of volume materials is not supported");
    }
    throw std::exception("TranslateMaterial: unsupported material type");
}

std::set<Baikal::UberV2Material::Ptr> MaterialConverter::TranslateMaterials(std::set<BaikalOld::Material::Ptr> const& old_materials)
{
    std::set<Baikal::UberV2Material::Ptr> result;
    unsigned int i = 0;
    for (auto old_mtl : old_materials)
    {
        if (!old_mtl)
        {
            std::cout << "Empty material" << std::endl;
            continue;
        }
        LOG("******************************************************");
        LOG(++i << ". Processing scene material: " << old_mtl->GetName());
        LOG("******************************************************");
        try
        {
            auto new_mtl = TranslateMaterial(old_mtl);
            new_mtl->SetName(old_mtl->GetName());
            result.insert(new_mtl);
        }
        catch (std::exception const&
            #ifdef USE_LOG
            ex
            #endif
            )
        {
            LOG(">>> Caught exception: " << ex.what());
            throw;
        }
    }
    return result;
}