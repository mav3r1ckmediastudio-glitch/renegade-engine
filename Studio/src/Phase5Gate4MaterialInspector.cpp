#include "StudioApplication.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <sstream>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::MaterialTextureSlot;

    constexpr std::array<MaterialTextureSlot, 5> Gate4TextureSlots = {
        MaterialTextureSlot::BaseColor,
        MaterialTextureSlot::Normal,
        MaterialTextureSlot::Surface,
        MaterialTextureSlot::Emissive,
        MaterialTextureSlot::Occlusion,
    };

    const char* TextureSlotUiName(const MaterialTextureSlot slot) noexcept
    {
        switch (slot)
        {
        case MaterialTextureSlot::BaseColor: return "BASE COLOUR";
        case MaterialTextureSlot::Normal: return "NORMAL";
        case MaterialTextureSlot::Surface: return "PACKED SURFACE";
        case MaterialTextureSlot::Emissive: return "EMISSIVE";
        case MaterialTextureSlot::Occlusion: return "OCCLUSION / AO";
        }
        return "TEXTURE";
    }

    std::string MaterialDisplayName(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const std::size_t index)
    {
        const auto* name = scene.names.GetComponent(entity);
        if (name != nullptr && !name->name.empty())
            return name->name;
        return "MATERIAL " + std::to_string(index + 1);
    }

    std::string BoundTextureDisplay(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const MaterialTextureSlot slot,
        const renegade::bridge::AssetRegistry* registry,
        std::string& detail)
    {
        detail.clear();
        const auto* material = scene.materials.GetComponent(materialEntity);
        if (material == nullptr)
            return "MISSING MATERIAL";

        const auto* metadata = scene.metadatas.GetComponent(materialEntity);
        if (metadata != nullptr &&
            metadata->string_values.has(
                renegade::bridge::MaterialTextureSlotMetadataKey(slot)))
        {
            const auto assetId = metadata->string_values.get(
                renegade::bridge::MaterialTextureSlotMetadataKey(slot));
            detail = "Governed asset ID: " + assetId;
            if (registry != nullptr)
            {
                const auto found = std::find_if(
                    registry->records.begin(), registry->records.end(),
                    [&assetId](const renegade::bridge::AssetRecord& record)
                    { return record.assetId == assetId; });
                if (found != registry->records.end())
                {
                    detail += "\nProject product: " + found->projectRelativePath;
                    const auto file = fs::u8path(found->projectRelativePath)
                        .filename().generic_u8string();
                    if (!file.empty())
                        return file;
                }
            }
            return assetId.size() > 12 ? assetId.substr(0, 12) + "..." : assetId;
        }

        const auto& texture = material->textures[
            renegade::bridge::WickedTextureSlot(slot)];
        if (!texture.name.empty())
        {
            detail = "Native Wicked texture: " + texture.name;
            return fs::u8path(texture.name).filename().generic_u8string();
        }
        if (texture.resource.IsValid())
        {
            detail = "Live embedded/native Wicked texture resource";
            return "LIVE / EMBEDDED";
        }
        return "EMPTY";
    }
}

namespace renegade::studio
{
    void StudioRenderPath::CreateMaterialInspector()
    {
        const auto createSection = [this](
            wi::gui::Label& label,
            const char* name,
            const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = wi::Color(178, 178, 176, 255);
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            inspectorPanel_.AddWidget(&label);
        };

        createSection(materialLabel_, "Gate 4 Material Section",
            "MATERIAL // NATIVE WICKED");

        materialSelector_.Create("Material Target");
        materialSelector_.SetTooltip(
            "Choose one material referenced by this object/reusable asset. Mesh subset bindings and imported hierarchy remain unchanged.");
        materialSelector_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            materialInspectorEntity_ = static_cast<wi::ecs::Entity>(args.userdata);
            if (session_ != nullptr)
            {
                RefreshMaterialInspector(
                    true, session_->Selection().SelectedEntity());
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&materialSelector_);

        materialShaderType_.Create("Material Shader Type");
        using Material = wi::scene::MaterialComponent;
        materialShaderType_.AddItem("PBR", Material::SHADERTYPE_PBR);
        materialShaderType_.AddItem("PBR // PLANAR REFLECTION", Material::SHADERTYPE_PBR_PLANARREFLECTION);
        materialShaderType_.AddItem("PBR // PARALLAX OCCLUSION", Material::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING);
        materialShaderType_.AddItem("PBR // ANISOTROPIC", Material::SHADERTYPE_PBR_ANISOTROPIC);
        materialShaderType_.AddItem("WATER", Material::SHADERTYPE_WATER);
        materialShaderType_.AddItem("CARTOON", Material::SHADERTYPE_CARTOON);
        materialShaderType_.AddItem("UNLIT", Material::SHADERTYPE_UNLIT);
        materialShaderType_.AddItem("PBR // CLOTH", Material::SHADERTYPE_PBR_CLOTH);
        materialShaderType_.AddItem("PBR // CLEARCOAT", Material::SHADERTYPE_PBR_CLEARCOAT);
        materialShaderType_.AddItem("PBR // CLOTH + CLEARCOAT", Material::SHADERTYPE_PBR_CLOTH_CLEARCOAT);
        materialShaderType_.AddItem("PBR // TERRAIN BLENDED", Material::SHADERTYPE_PBR_TERRAINBLENDED);
        materialShaderType_.AddItem("INTERIOR MAPPING", Material::SHADERTYPE_INTERIORMAPPING);
        materialShaderType_.SetTooltip(
            "Native Wicked material shader. Custom SDK shaders remain SDK-owned and are not replaced by Gate 4.");
        materialShaderType_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedMaterialShader(
                static_cast<Material::SHADERTYPE>(args.userdata));
        });
        inspectorPanel_.AddWidget(&materialShaderType_);

        materialBlendMode_.Create("Material Blend Mode");
        materialBlendMode_.AddItem("OPAQUE", wi::enums::BLENDMODE_OPAQUE);
        materialBlendMode_.AddItem("ALPHA", wi::enums::BLENDMODE_ALPHA);
        materialBlendMode_.AddItem("PREMULTIPLIED", wi::enums::BLENDMODE_PREMULTIPLIED);
        materialBlendMode_.AddItem("ADDITIVE", wi::enums::BLENDMODE_ADDITIVE);
        materialBlendMode_.AddItem("MULTIPLY", wi::enums::BLENDMODE_MULTIPLY);
        materialBlendMode_.AddItem("INVERSE", wi::enums::BLENDMODE_INVERSE);
        materialBlendMode_.SetTooltip("Native Wicked material blend mode.");
        materialBlendMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedMaterialBlend(
                static_cast<wi::enums::BLENDMODE>(args.userdata));
        });
        inspectorPanel_.AddWidget(&materialBlendMode_);

        createSection(materialCoreLabel_, "Gate 4 Material Core",
            "PBR // CORE PROPERTIES");

        const auto createSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const char* tooltip,
            const MaterialField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            slider.Create(minimum, maximum, 0.0f, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnDragStarted([this, field](const float)
            { BeginMaterialSlider(field); });
            slider.OnValuePreview([this, field](const float value)
            { PreviewMaterialSlider(field, value); });
            slider.OnValueCommitted([this, field](const float value)
            { CommitMaterialSlider(field, value); });
            inspectorPanel_.AddWidget(&slider);
        };

        createSlider(materialBaseColorRed_, "Material Base Red", "BASE COLOUR // R",
            "Native base-colour red channel.", MaterialField::BaseColorRed, 0, 1, 255);
        createSlider(materialBaseColorGreen_, "Material Base Green", "BASE COLOUR // G",
            "Native base-colour green channel.", MaterialField::BaseColorGreen, 0, 1, 255);
        createSlider(materialBaseColorBlue_, "Material Base Blue", "BASE COLOUR // B",
            "Native base-colour blue channel.", MaterialField::BaseColorBlue, 0, 1, 255);
        createSlider(materialOpacity_, "Material Opacity", "OPACITY",
            "Base-colour alpha. Use an appropriate blend mode for translucent rendering.",
            MaterialField::Opacity, 0, 1, 255);
        createSlider(materialMetalness_, "Material Metalness", "METALNESS",
            "Metallic response of the native PBR material.", MaterialField::Metalness, 0, 1, 255);
        createSlider(materialRoughness_, "Material Roughness", "ROUGHNESS",
            "Surface microsurface roughness.", MaterialField::Roughness, 0, 1, 255);
        createSlider(materialReflectance_, "Material Reflectance", "REFLECTANCE",
            "Native dielectric reflectance amount.", MaterialField::Reflectance, 0, 1, 255);
        createSlider(materialNormalStrength_, "Material Normal Strength", "NORMAL MAP // STRENGTH",
            "How strongly the native normal map perturbs the surface normal.",
            MaterialField::NormalMapStrength, 0, 4, 1000);
        createSlider(materialAlphaRef_, "Material Alpha Cutoff", "ALPHA CUTOFF",
            "Native alpha-test reference. Values below 1 enable alpha testing.",
            MaterialField::AlphaRef, 0, 1, 255);
        createSlider(materialEmissiveRed_, "Material Emissive Red", "EMISSIVE // R",
            "Emissive colour red channel.", MaterialField::EmissiveRed, 0, 1, 255);
        createSlider(materialEmissiveGreen_, "Material Emissive Green", "EMISSIVE // G",
            "Emissive colour green channel.", MaterialField::EmissiveGreen, 0, 1, 255);
        createSlider(materialEmissiveBlue_, "Material Emissive Blue", "EMISSIVE // B",
            "Emissive colour blue channel.", MaterialField::EmissiveBlue, 0, 1, 255);
        createSlider(materialEmissiveStrength_, "Material Emissive Strength", "EMISSIVE // STRENGTH",
            "Native emissive intensity.", MaterialField::EmissiveStrength, 0, 100, 2000);

        const auto createToggle = [this](
            SceneInspectorCheckBox& checkbox,
            const char* name,
            const char* tooltip,
            const MaterialToggle toggle)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, toggle](const wi::gui::EventArgs& args)
            { ApplySelectedMaterialToggle(toggle, args.bValue); });
            inspectorPanel_.AddWidget(&checkbox);
        };
        createToggle(materialReceiveShadow_, "Receive shadow: ",
            "Allow native Wicked lighting shadows to fall on this material.",
            MaterialToggle::ReceiveShadow);
        createToggle(materialCastShadow_, "Cast shadow: ",
            "Allow geometry using this material to cast native Wicked shadows.",
            MaterialToggle::CastShadow);
        createToggle(materialUseVertexColors_, "Vertex colours: ",
            "Multiply/use mesh vertex colour data in this material.",
            MaterialToggle::UseVertexColors);
        createToggle(materialDoubleSided_, "Double sided: ",
            "Render both sides of polygons using this material.",
            MaterialToggle::DoubleSided);

        createSection(materialUvLabel_, "Gate 4 Material UV", "UV // TEXTURE TRANSFORM");
        createSlider(materialTilingU_, "Material Tiling U", "TILING // U",
            "Native Wicked texture-coordinate multiplier U.", MaterialField::TilingU, 0.01f, 10, 1000);
        createSlider(materialTilingV_, "Material Tiling V", "TILING // V",
            "Native Wicked texture-coordinate multiplier V.", MaterialField::TilingV, 0.01f, 10, 1000);
        createSlider(materialOffsetU_, "Material Offset U", "OFFSET // U",
            "Native Wicked texture-coordinate offset U.", MaterialField::OffsetU, -10, 10, 2000);
        createSlider(materialOffsetV_, "Material Offset V", "OFFSET // V",
            "Native Wicked texture-coordinate offset V.", MaterialField::OffsetV, -10, 10, 2000);

        createSection(materialTexturesLabel_, "Gate 4 Material Textures",
            "TEXTURES // GOVERNED PROJECT ASSETS");
        for (std::size_t index = 0; index < Gate4TextureSlots.size(); ++index)
        {
            const auto slot = Gate4TextureSlots[index];
            auto& assign = materialTextureAssign_[index];
            auto& clear = materialTextureClear_[index];
            assign.Create(std::string("Material Texture ") + TextureSlotUiName(slot));
            assign.SetText(std::string(TextureSlotUiName(slot)) + " // SELECT...");
            assign.SetTooltip(slot == MaterialTextureSlot::Surface
                ? "Wicked's native packed SURFACEMAP. This is not a separate roughness/metalness file slot. Select/import a governed local texture."
                : "Select/import a local image through Renegade's governed project texture pipeline.");
            assign.OnClick([this, slot](const wi::gui::EventArgs&)
            { ChooseSelectedMaterialTexture(slot); });
            inspectorPanel_.AddWidget(&assign);

            clear.Create(std::string("Clear Material Texture ") + TextureSlotUiName(slot));
            clear.SetText("CLEAR");
            clear.SetTooltip("Remove this material texture binding. Undo restores the exact prior native texture and governed stable-ID metadata.");
            clear.OnClick([this, slot](const wi::gui::EventArgs&)
            { ClearSelectedMaterialTexture(slot); });
            inspectorPanel_.AddWidget(&clear);
        }

        createSection(materialShaderSpecificLabel_, "Gate 4 Shader Specific",
            "SHADER // NATIVE PARAMETERS");
        createSlider(materialPomStrength_, "Material POM", "POM // STRENGTH",
            "Parallax Occlusion Mapping amount. Used by PBR // PARALLAX OCCLUSION.",
            MaterialField::ParallaxOcclusion, 0, 1, 1000);
        createSlider(materialAnisotropyStrength_, "Material Anisotropy Strength", "ANISOTROPY // STRENGTH",
            "Anisotropic specular strength.", MaterialField::AnisotropyStrength, 0, 1, 1000);
        createSlider(materialAnisotropyRotation_, "Material Anisotropy Rotation", "ANISOTROPY // ROTATION",
            "Anisotropic direction in degrees.", MaterialField::AnisotropyRotation, 0, 360, 360);
        createSlider(materialSheenRed_, "Material Sheen Red", "SHEEN // R",
            "Cloth sheen colour red.", MaterialField::SheenRed, 0, 1, 255);
        createSlider(materialSheenGreen_, "Material Sheen Green", "SHEEN // G",
            "Cloth sheen colour green.", MaterialField::SheenGreen, 0, 1, 255);
        createSlider(materialSheenBlue_, "Material Sheen Blue", "SHEEN // B",
            "Cloth sheen colour blue.", MaterialField::SheenBlue, 0, 1, 255);
        createSlider(materialSheenRoughness_, "Material Sheen Roughness", "SHEEN // ROUGHNESS",
            "Cloth sheen roughness.", MaterialField::SheenRoughness, 0, 1, 1000);
        createSlider(materialClearcoat_, "Material Clearcoat", "CLEARCOAT // STRENGTH",
            "Clearcoat layer factor.", MaterialField::Clearcoat, 0, 1, 1000);
        createSlider(materialClearcoatRoughness_, "Material Clearcoat Roughness", "CLEARCOAT // ROUGHNESS",
            "Clearcoat layer roughness.", MaterialField::ClearcoatRoughness, 0, 1, 1000);
        createSlider(materialTransmission_, "Material Transmission", "TRANSMISSION",
            "Diffuse light transmission amount for the native water-capable material.",
            MaterialField::Transmission, 0, 1, 1000);
        createSlider(materialRefraction_, "Material Refraction", "REFRACTION",
            "Native refraction amount for the water-capable material.",
            MaterialField::Refraction, 0, 1, 1000);
        createSlider(materialTerrainBlendHeight_, "Material Terrain Blend Height", "TERRAIN BLEND // HEIGHT",
            "Blend geometry with terrain height.", MaterialField::BlendWithTerrainHeight, 0, 2, 1000);
        createSlider(materialMeshBlend_, "Material Mesh Blend", "MESH BLEND",
            "Native screen-space mesh blend falloff.", MaterialField::MeshBlend, 0, 2, 1000);
        createSlider(materialInteriorScaleX_, "Interior Scale X", "INTERIOR SCALE // X",
            "Interior mapping cell scale X.", MaterialField::InteriorScaleX, 0.001f, 100, 2000);
        createSlider(materialInteriorScaleY_, "Interior Scale Y", "INTERIOR SCALE // Y",
            "Interior mapping cell scale Y.", MaterialField::InteriorScaleY, 0.001f, 100, 2000);
        createSlider(materialInteriorScaleZ_, "Interior Scale Z", "INTERIOR SCALE // Z",
            "Interior mapping cell scale Z.", MaterialField::InteriorScaleZ, 0.001f, 100, 2000);
        createSlider(materialInteriorOffsetX_, "Interior Offset X", "INTERIOR OFFSET // X",
            "Interior mapping offset X.", MaterialField::InteriorOffsetX, -10, 10, 2000);
        createSlider(materialInteriorOffsetY_, "Interior Offset Y", "INTERIOR OFFSET // Y",
            "Interior mapping offset Y.", MaterialField::InteriorOffsetY, -10, 10, 2000);
        createSlider(materialInteriorOffsetZ_, "Interior Offset Z", "INTERIOR OFFSET // Z",
            "Interior mapping offset Z.", MaterialField::InteriorOffsetZ, -10, 10, 2000);
        createSlider(materialInteriorRotation_, "Interior Rotation", "INTERIOR // ROTATION",
            "Interior mapping horizontal rotation in degrees.", MaterialField::InteriorRotation, 0, 360, 360);
    }

    void StudioRenderPath::LayoutMaterialInspector(const float fieldWidth)
    {
        if (!materialInspectorVisible_)
        {
            materialInspectorBottom_ = 630.0f;
            return;
        }

        float y = 630.0f;
        const auto full = [fieldWidth, &y](wi::gui::Widget& widget, const float height = 28.0f)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, height));
            y += height + 6.0f;
        };
        const auto section = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 20.0f));
            y += 24.0f;
        };
        const auto two = [fieldWidth, &y](wi::gui::Widget& a, wi::gui::Widget& b)
        {
            const float width = (fieldWidth - 8.0f) * 0.5f;
            a.SetPos(XMFLOAT2(12.0f, y));
            b.SetPos(XMFLOAT2(20.0f + width, y));
            a.SetSize(XMFLOAT2(width, 28.0f));
            b.SetSize(XMFLOAT2(width, 28.0f));
            y += 34.0f;
        };

        section(materialLabel_);
        if (materialInspectorMaterialCount_ > 1)
            full(materialSelector_);
        full(materialShaderType_);
        full(materialBlendMode_);
        section(materialCoreLabel_);
        full(materialBaseColorRed_);
        full(materialBaseColorGreen_);
        full(materialBaseColorBlue_);
        full(materialOpacity_);
        full(materialMetalness_);
        full(materialRoughness_);
        full(materialReflectance_);
        full(materialNormalStrength_);
        full(materialAlphaRef_);
        full(materialEmissiveRed_);
        full(materialEmissiveGreen_);
        full(materialEmissiveBlue_);
        full(materialEmissiveStrength_);
        two(materialReceiveShadow_, materialCastShadow_);
        two(materialUseVertexColors_, materialDoubleSided_);

        section(materialUvLabel_);
        full(materialTilingU_);
        full(materialTilingV_);
        full(materialOffsetU_);
        full(materialOffsetV_);

        section(materialTexturesLabel_);
        for (std::size_t index = 0; index < materialTextureAssign_.size(); ++index)
        {
            const float clearWidth = std::min(84.0f, fieldWidth * 0.26f);
            const float assignWidth = fieldWidth - clearWidth - 8.0f;
            materialTextureAssign_[index].SetPos(XMFLOAT2(12.0f, y));
            materialTextureAssign_[index].SetSize(XMFLOAT2(assignWidth, 28.0f));
            materialTextureClear_[index].SetPos(XMFLOAT2(20.0f + assignWidth, y));
            materialTextureClear_[index].SetSize(XMFLOAT2(clearWidth, 28.0f));
            y += 34.0f;
        }

        using Material = wi::scene::MaterialComponent;
        const bool pom = materialInspectorShaderType_ == Material::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING;
        const bool anisotropic = materialInspectorShaderType_ == Material::SHADERTYPE_PBR_ANISOTROPIC;
        const bool cloth = materialInspectorShaderType_ == Material::SHADERTYPE_PBR_CLOTH ||
            materialInspectorShaderType_ == Material::SHADERTYPE_PBR_CLOTH_CLEARCOAT;
        const bool clearcoat = materialInspectorShaderType_ == Material::SHADERTYPE_PBR_CLEARCOAT ||
            materialInspectorShaderType_ == Material::SHADERTYPE_PBR_CLOTH_CLEARCOAT;
        const bool water = materialInspectorShaderType_ == Material::SHADERTYPE_WATER;
        const bool terrain = materialInspectorShaderType_ == Material::SHADERTYPE_PBR_TERRAINBLENDED;
        const bool interior = materialInspectorShaderType_ == Material::SHADERTYPE_INTERIORMAPPING;
        if (pom || anisotropic || cloth || clearcoat || water || terrain || interior)
        {
            section(materialShaderSpecificLabel_);
            if (pom) full(materialPomStrength_);
            if (anisotropic)
            {
                full(materialAnisotropyStrength_);
                full(materialAnisotropyRotation_);
            }
            if (cloth)
            {
                full(materialSheenRed_);
                full(materialSheenGreen_);
                full(materialSheenBlue_);
                full(materialSheenRoughness_);
            }
            if (clearcoat)
            {
                full(materialClearcoat_);
                full(materialClearcoatRoughness_);
            }
            if (water)
            {
                full(materialTransmission_);
                full(materialRefraction_);
            }
            if (terrain)
            {
                full(materialTerrainBlendHeight_);
                full(materialMeshBlend_);
            }
            if (interior)
            {
                full(materialInteriorScaleX_);
                full(materialInteriorScaleY_);
                full(materialInteriorScaleZ_);
                full(materialInteriorOffsetX_);
                full(materialInteriorOffsetY_);
                full(materialInteriorOffsetZ_);
                full(materialInteriorRotation_);
            }
        }
        materialInspectorBottom_ = y;
    }

    void StudioRenderPath::RefreshMaterialInspector(
        const bool candidateVisible,
        const wi::ecs::Entity selectedEntity)
    {
        auto setCommonVisible = [this](const bool visible)
        {
            materialLabel_.SetVisible(visible);
            materialShaderType_.SetVisible(visible);
            materialBlendMode_.SetVisible(visible);
            materialCoreLabel_.SetVisible(visible);
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&materialBaseColorRed_),
                static_cast<wi::gui::Widget*>(&materialBaseColorGreen_),
                static_cast<wi::gui::Widget*>(&materialBaseColorBlue_),
                static_cast<wi::gui::Widget*>(&materialOpacity_),
                static_cast<wi::gui::Widget*>(&materialMetalness_),
                static_cast<wi::gui::Widget*>(&materialRoughness_),
                static_cast<wi::gui::Widget*>(&materialReflectance_),
                static_cast<wi::gui::Widget*>(&materialNormalStrength_),
                static_cast<wi::gui::Widget*>(&materialAlphaRef_),
                static_cast<wi::gui::Widget*>(&materialEmissiveRed_),
                static_cast<wi::gui::Widget*>(&materialEmissiveGreen_),
                static_cast<wi::gui::Widget*>(&materialEmissiveBlue_),
                static_cast<wi::gui::Widget*>(&materialEmissiveStrength_),
                static_cast<wi::gui::Widget*>(&materialReceiveShadow_),
                static_cast<wi::gui::Widget*>(&materialCastShadow_),
                static_cast<wi::gui::Widget*>(&materialUseVertexColors_),
                static_cast<wi::gui::Widget*>(&materialDoubleSided_),
                static_cast<wi::gui::Widget*>(&materialUvLabel_),
                static_cast<wi::gui::Widget*>(&materialTilingU_),
                static_cast<wi::gui::Widget*>(&materialTilingV_),
                static_cast<wi::gui::Widget*>(&materialOffsetU_),
                static_cast<wi::gui::Widget*>(&materialOffsetV_),
                static_cast<wi::gui::Widget*>(&materialTexturesLabel_)})
            {
                widget->SetVisible(visible);
            }
            for (auto& widget : materialTextureAssign_) widget.SetVisible(visible);
            for (auto& widget : materialTextureClear_) widget.SetVisible(visible);
        };
        const auto hideSpecific = [this]()
        {
            materialShaderSpecificLabel_.SetVisible(false);
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&materialPomStrength_),
                static_cast<wi::gui::Widget*>(&materialAnisotropyStrength_),
                static_cast<wi::gui::Widget*>(&materialAnisotropyRotation_),
                static_cast<wi::gui::Widget*>(&materialSheenRed_),
                static_cast<wi::gui::Widget*>(&materialSheenGreen_),
                static_cast<wi::gui::Widget*>(&materialSheenBlue_),
                static_cast<wi::gui::Widget*>(&materialSheenRoughness_),
                static_cast<wi::gui::Widget*>(&materialClearcoat_),
                static_cast<wi::gui::Widget*>(&materialClearcoatRoughness_),
                static_cast<wi::gui::Widget*>(&materialTransmission_),
                static_cast<wi::gui::Widget*>(&materialRefraction_),
                static_cast<wi::gui::Widget*>(&materialTerrainBlendHeight_),
                static_cast<wi::gui::Widget*>(&materialMeshBlend_),
                static_cast<wi::gui::Widget*>(&materialInteriorScaleX_),
                static_cast<wi::gui::Widget*>(&materialInteriorScaleY_),
                static_cast<wi::gui::Widget*>(&materialInteriorScaleZ_),
                static_cast<wi::gui::Widget*>(&materialInteriorOffsetX_),
                static_cast<wi::gui::Widget*>(&materialInteriorOffsetY_),
                static_cast<wi::gui::Widget*>(&materialInteriorOffsetZ_),
                static_cast<wi::gui::Widget*>(&materialInteriorRotation_)})
            {
                widget->SetVisible(false);
            }
        };

        hideSpecific();
        materialSelector_.SetVisible(false);
        if (!candidateVisible || session_ == nullptr)
        {
            materialInspectorVisible_ = false;
            materialInspectorMaterialCount_ = 0;
            materialInspectorEntity_ = wi::ecs::INVALID_ENTITY;
            setCommonVisible(false);
            LayoutMaterialInspector(std::max(1.0f, inspectorPanel_.GetSize().x - 24.0f));
            return;
        }

        auto& scene = session_->Scenes().GetScene();
        const auto materials = bridge::CollectEditableMaterialEntities(scene, selectedEntity);
        if (materials.empty())
        {
            materialInspectorVisible_ = false;
            materialInspectorMaterialCount_ = 0;
            materialInspectorEntity_ = wi::ecs::INVALID_ENTITY;
            setCommonVisible(false);
            LayoutMaterialInspector(std::max(1.0f, inspectorPanel_.GetSize().x - 24.0f));
            return;
        }

        materialInspectorVisible_ = true;
        materialInspectorMaterialCount_ = materials.size();
        if (std::find(materials.begin(), materials.end(), materialInspectorEntity_) == materials.end())
            materialInspectorEntity_ = materials.front();
        setCommonVisible(true);

        materialSelector_.ClearItems();
        for (std::size_t index = 0; index < materials.size(); ++index)
        {
            materialSelector_.AddItem(
                MaterialDisplayName(scene, materials[index], index),
                static_cast<std::uint64_t>(materials[index]));
        }
        materialSelector_.SetVisible(materials.size() > 1);
        materialSelector_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(materialInspectorEntity_));

        auto* material = scene.materials.GetComponent(materialInspectorEntity_);
        if (material == nullptr)
        {
            materialInspectorVisible_ = false;
            setCommonVisible(false);
            return;
        }
        const auto state = bridge::CaptureMaterial(*material);
        materialInspectorShaderType_ = state.shaderType;
        materialShaderType_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.shaderType));
        materialBlendMode_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.blendMode));
        const bool customShader = material->GetCustomShaderID() >= 0;
        materialShaderType_.SetEnabled(!customShader);
        materialBlendMode_.SetEnabled(!customShader);
        if (customShader)
        {
            materialShaderType_.SetTooltip(
                "This material uses an SDK-registered custom shader. Gate 4 preserves custom shader ownership and does not replace it from Studio.");
        }

        materialBaseColorRed_.SetValue(state.baseColor.x);
        materialBaseColorGreen_.SetValue(state.baseColor.y);
        materialBaseColorBlue_.SetValue(state.baseColor.z);
        materialOpacity_.SetValue(state.baseColor.w);
        materialMetalness_.SetValue(state.metalness);
        materialRoughness_.SetValue(state.roughness);
        materialReflectance_.SetValue(state.reflectance);
        materialNormalStrength_.SetValue(state.normalMapStrength);
        materialAlphaRef_.SetValue(state.alphaRef);
        materialEmissiveRed_.SetValue(state.emissiveColor.x);
        materialEmissiveGreen_.SetValue(state.emissiveColor.y);
        materialEmissiveBlue_.SetValue(state.emissiveColor.z);
        materialEmissiveStrength_.SetValue(state.emissiveStrength);
        materialReceiveShadow_.SetCheck(state.receiveShadow);
        materialCastShadow_.SetCheck(state.castShadow);
        materialUseVertexColors_.SetCheck(state.useVertexColors);
        materialDoubleSided_.SetCheck(state.doubleSided);
        materialTilingU_.SetValue(state.texMulAdd.x);
        materialTilingV_.SetValue(state.texMulAdd.y);
        materialOffsetU_.SetValue(state.texMulAdd.z);
        materialOffsetV_.SetValue(state.texMulAdd.w);

        renegade::bridge::AssetRegistry registry;
        renegade::bridge::AssetRegistry* registryPtr = nullptr;
        if (session_->Projects().HasProject())
        {
            std::string error;
            const auto& project = session_->Projects().CurrentProject();
            if (renegade::bridge::ReadAssetRegistry(
                    project.rootPath, project.projectId, registry, error))
            {
                registryPtr = &registry;
            }
        }
        for (std::size_t index = 0; index < Gate4TextureSlots.size(); ++index)
        {
            const auto slot = Gate4TextureSlots[index];
            std::string detail;
            const auto display = BoundTextureDisplay(
                scene, materialInspectorEntity_, slot, registryPtr, detail);
            materialTextureAssign_[index].SetText(
                std::string(TextureSlotUiName(slot)) + " // " +
                (display == "EMPTY" ? "SELECT..." : display));
            materialTextureAssign_[index].SetTooltip(
                (slot == MaterialTextureSlot::Surface
                    ? std::string("Wicked native PACKED SURFACEMAP. ")
                    : std::string()) +
                (detail.empty() ? "No texture is currently bound." : detail) +
                "\nClick to select/import a replacement through the governed project asset pipeline.");
            const auto& texture = material->textures[bridge::WickedTextureSlot(slot)];
            const auto* metadata = scene.metadatas.GetComponent(materialInspectorEntity_);
            const bool governed = metadata != nullptr &&
                metadata->string_values.has(bridge::MaterialTextureSlotMetadataKey(slot));
            materialTextureClear_[index].SetEnabled(
                governed || !texture.name.empty() || texture.resource.IsValid());
        }

        using Material = wi::scene::MaterialComponent;
        const bool pom = state.shaderType == Material::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING;
        const bool anisotropic = state.shaderType == Material::SHADERTYPE_PBR_ANISOTROPIC;
        const bool cloth = state.shaderType == Material::SHADERTYPE_PBR_CLOTH ||
            state.shaderType == Material::SHADERTYPE_PBR_CLOTH_CLEARCOAT;
        const bool clearcoat = state.shaderType == Material::SHADERTYPE_PBR_CLEARCOAT ||
            state.shaderType == Material::SHADERTYPE_PBR_CLOTH_CLEARCOAT;
        const bool water = state.shaderType == Material::SHADERTYPE_WATER;
        const bool terrain = state.shaderType == Material::SHADERTYPE_PBR_TERRAINBLENDED;
        const bool interior = state.shaderType == Material::SHADERTYPE_INTERIORMAPPING;
        const bool anySpecific = pom || anisotropic || cloth || clearcoat || water || terrain || interior;
        materialShaderSpecificLabel_.SetVisible(anySpecific);
        materialPomStrength_.SetVisible(pom);
        materialAnisotropyStrength_.SetVisible(anisotropic);
        materialAnisotropyRotation_.SetVisible(anisotropic);
        materialSheenRed_.SetVisible(cloth);
        materialSheenGreen_.SetVisible(cloth);
        materialSheenBlue_.SetVisible(cloth);
        materialSheenRoughness_.SetVisible(cloth);
        materialClearcoat_.SetVisible(clearcoat);
        materialClearcoatRoughness_.SetVisible(clearcoat);
        materialTransmission_.SetVisible(water);
        materialRefraction_.SetVisible(water);
        materialTerrainBlendHeight_.SetVisible(terrain);
        materialMeshBlend_.SetVisible(terrain);
        materialInteriorScaleX_.SetVisible(interior);
        materialInteriorScaleY_.SetVisible(interior);
        materialInteriorScaleZ_.SetVisible(interior);
        materialInteriorOffsetX_.SetVisible(interior);
        materialInteriorOffsetY_.SetVisible(interior);
        materialInteriorOffsetZ_.SetVisible(interior);
        materialInteriorRotation_.SetVisible(interior);

        materialPomStrength_.SetValue(state.parallaxOcclusionMapping);
        materialAnisotropyStrength_.SetValue(state.anisotropyStrength);
        materialAnisotropyRotation_.SetValue(state.anisotropyRotationDegrees);
        materialSheenRed_.SetValue(state.sheenColor.x);
        materialSheenGreen_.SetValue(state.sheenColor.y);
        materialSheenBlue_.SetValue(state.sheenColor.z);
        materialSheenRoughness_.SetValue(state.sheenRoughness);
        materialClearcoat_.SetValue(state.clearcoat);
        materialClearcoatRoughness_.SetValue(state.clearcoatRoughness);
        materialTransmission_.SetValue(state.transmission);
        materialRefraction_.SetValue(state.refraction);
        materialTerrainBlendHeight_.SetValue(state.blendWithTerrainHeight);
        materialMeshBlend_.SetValue(state.meshBlend);
        materialInteriorScaleX_.SetValue(state.interiorMappingScale.x);
        materialInteriorScaleY_.SetValue(state.interiorMappingScale.y);
        materialInteriorScaleZ_.SetValue(state.interiorMappingScale.z);
        materialInteriorOffsetX_.SetValue(state.interiorMappingOffset.x);
        materialInteriorOffsetY_.SetValue(state.interiorMappingOffset.y);
        materialInteriorOffsetZ_.SetValue(state.interiorMappingOffset.z);
        materialInteriorRotation_.SetValue(state.interiorMappingRotationDegrees);

        LayoutMaterialInspector(std::max(1.0f, inspectorPanel_.GetSize().x - 24.0f));
    }

    bool StudioRenderPath::CommitSelectedMaterial(const bridge::MaterialState& state)
    {
        if (session_ == nullptr || materialInspectorEntity_ == wi::ecs::INVALID_ENTITY)
            return false;
        auto& scene = session_->Scenes().GetScene();
        if (scene.materials.GetComponent(materialInspectorEntity_) == nullptr ||
            bridge::IsTerrainOwnedMaterial(scene, materialInspectorEntity_))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetMaterialCommand>(
                scene, materialInspectorEntity_, state));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedMaterialShader(
        const wi::scene::MaterialComponent::SHADERTYPE shaderType)
    {
        if (session_ == nullptr) return;
        auto& scene = session_->Scenes().GetScene();
        const auto* material = scene.materials.GetComponent(materialInspectorEntity_);
        if (material == nullptr || material->GetCustomShaderID() >= 0) return;
        auto state = bridge::CaptureMaterial(*material);
        state.shaderType = shaderType;
        CommitSelectedMaterial(state);
    }

    void StudioRenderPath::ApplySelectedMaterialBlend(const wi::enums::BLENDMODE blendMode)
    {
        if (session_ == nullptr) return;
        auto& scene = session_->Scenes().GetScene();
        const auto* material = scene.materials.GetComponent(materialInspectorEntity_);
        if (material == nullptr || material->GetCustomShaderID() >= 0) return;
        auto state = bridge::CaptureMaterial(*material);
        state.blendMode = blendMode;
        CommitSelectedMaterial(state);
    }

    void StudioRenderPath::ApplySelectedMaterialToggle(
        const MaterialToggle toggle,
        const bool value)
    {
        if (session_ == nullptr) return;
        const auto* material = session_->Scenes().GetScene().materials.GetComponent(
            materialInspectorEntity_);
        if (material == nullptr) return;
        auto state = bridge::CaptureMaterial(*material);
        switch (toggle)
        {
        case MaterialToggle::ReceiveShadow: state.receiveShadow = value; break;
        case MaterialToggle::CastShadow: state.castShadow = value; break;
        case MaterialToggle::UseVertexColors: state.useVertexColors = value; break;
        case MaterialToggle::DoubleSided: state.doubleSided = value; break;
        }
        CommitSelectedMaterial(state);
    }

    void StudioRenderPath::SetMaterialFieldValue(
        bridge::MaterialState& state,
        const MaterialField field,
        const float value) noexcept
    {
        switch (field)
        {
        case MaterialField::BaseColorRed: state.baseColor.x = value; break;
        case MaterialField::BaseColorGreen: state.baseColor.y = value; break;
        case MaterialField::BaseColorBlue: state.baseColor.z = value; break;
        case MaterialField::Opacity: state.baseColor.w = value; break;
        case MaterialField::Metalness: state.metalness = value; break;
        case MaterialField::Roughness: state.roughness = value; break;
        case MaterialField::Reflectance: state.reflectance = value; break;
        case MaterialField::NormalMapStrength: state.normalMapStrength = value; break;
        case MaterialField::AlphaRef: state.alphaRef = value; break;
        case MaterialField::EmissiveRed: state.emissiveColor.x = value; break;
        case MaterialField::EmissiveGreen: state.emissiveColor.y = value; break;
        case MaterialField::EmissiveBlue: state.emissiveColor.z = value; break;
        case MaterialField::EmissiveStrength: state.emissiveStrength = value; break;
        case MaterialField::TilingU: state.texMulAdd.x = value; break;
        case MaterialField::TilingV: state.texMulAdd.y = value; break;
        case MaterialField::OffsetU: state.texMulAdd.z = value; break;
        case MaterialField::OffsetV: state.texMulAdd.w = value; break;
        case MaterialField::ParallaxOcclusion: state.parallaxOcclusionMapping = value; break;
        case MaterialField::AnisotropyStrength: state.anisotropyStrength = value; break;
        case MaterialField::AnisotropyRotation: state.anisotropyRotationDegrees = value; break;
        case MaterialField::SheenRed: state.sheenColor.x = value; break;
        case MaterialField::SheenGreen: state.sheenColor.y = value; break;
        case MaterialField::SheenBlue: state.sheenColor.z = value; break;
        case MaterialField::SheenRoughness: state.sheenRoughness = value; break;
        case MaterialField::Clearcoat: state.clearcoat = value; break;
        case MaterialField::ClearcoatRoughness: state.clearcoatRoughness = value; break;
        case MaterialField::Transmission: state.transmission = value; break;
        case MaterialField::Refraction: state.refraction = value; break;
        case MaterialField::BlendWithTerrainHeight: state.blendWithTerrainHeight = value; break;
        case MaterialField::MeshBlend: state.meshBlend = value; break;
        case MaterialField::InteriorScaleX: state.interiorMappingScale.x = value; break;
        case MaterialField::InteriorScaleY: state.interiorMappingScale.y = value; break;
        case MaterialField::InteriorScaleZ: state.interiorMappingScale.z = value; break;
        case MaterialField::InteriorOffsetX: state.interiorMappingOffset.x = value; break;
        case MaterialField::InteriorOffsetY: state.interiorMappingOffset.y = value; break;
        case MaterialField::InteriorOffsetZ: state.interiorMappingOffset.z = value; break;
        case MaterialField::InteriorRotation: state.interiorMappingRotationDegrees = value; break;
        }
        state = bridge::SanitizeMaterialState(state);
    }

    void StudioRenderPath::BeginMaterialSlider(const MaterialField field)
    {
        materialSliderActive_ = false;
        if (session_ == nullptr || materialInspectorEntity_ == wi::ecs::INVALID_ENTITY)
            return;
        const auto* material = session_->Scenes().GetScene().materials.GetComponent(
            materialInspectorEntity_);
        if (material == nullptr) return;
        materialSliderActive_ = true;
        materialSliderField_ = field;
        materialSliderEntity_ = materialInspectorEntity_;
        materialSliderBefore_ = bridge::CaptureMaterial(*material);
        materialSliderAfter_ = materialSliderBefore_;
    }

    void StudioRenderPath::PreviewMaterialSlider(
        const MaterialField field,
        const float value)
    {
        if (!materialSliderActive_ || materialSliderField_ != field || session_ == nullptr)
            return;
        materialSliderAfter_ = materialSliderBefore_;
        SetMaterialFieldValue(materialSliderAfter_, field, value);
        auto* material = session_->Scenes().GetScene().materials.GetComponent(materialSliderEntity_);
        if (material != nullptr)
            bridge::ApplyMaterial(*material, materialSliderAfter_);
    }

    void StudioRenderPath::CommitMaterialSlider(
        const MaterialField field,
        const float value)
    {
        if (!materialSliderActive_ || materialSliderField_ != field || session_ == nullptr)
            return;
        SetMaterialFieldValue(materialSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* material = scene.materials.GetComponent(materialSliderEntity_);
        if (material != nullptr)
        {
            bridge::ApplyMaterial(*material, materialSliderBefore_);
            (void)session_->Commands().Execute(
                std::make_unique<bridge::SetMaterialCommand>(
                    scene, materialSliderEntity_, materialSliderBefore_, materialSliderAfter_));
        }
        materialSliderActive_ = false;
        materialSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ChooseSelectedMaterialTexture(const bridge::MaterialTextureSlot slot)
    {
        if (session_ == nullptr || !session_->Projects().HasProject() ||
            materialInspectorEntity_ == wi::ecs::INVALID_ENTITY ||
            wi::jobsystem::IsBusy(materialTextureImportWorkload_))
            return;

        const auto materialEntity = materialInspectorEntity_;
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = std::string("Material ") + TextureSlotUiName(slot) + " texture";
        params.extensions = {"png", "tga", "dds", "jpg", "jpeg", "bmp", "hdr"};
        wi::helper::FileDialog(params,
            [this, materialEntity, slot](const std::string& sourcePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, materialEntity, slot, sourcePath](std::uint64_t)
                    {
                        if (sourcePath.empty() || session_ == nullptr) return;
                        auto& scene = session_->Scenes().GetScene();
                        if (scene.materials.GetComponent(materialEntity) == nullptr ||
                            bridge::IsTerrainOwnedMaterial(scene, materialEntity))
                        {
                            studioChrome_.SetStatusText("MATERIAL TEXTURE // TARGET NO LONGER EXISTS");
                            return;
                        }
                        const auto format = bridge::DetectResourceSourceFormat(sourcePath);
                        if (format == bridge::ResourceSourceFormat::Unknown ||
                            bridge::ClassifyResourceSourceFormat(format) != bridge::ResourceClass::Texture)
                        {
                            studioChrome_.SetStatusText("MATERIAL TEXTURE // UNSUPPORTED IMAGE FORMAT");
                            wi::helper::messageBox(
                                "Choose a supported image texture (PNG, TGA, DDS, JPG/JPEG, BMP or HDR).",
                                "Select Material Texture");
                            return;
                        }

                        struct ImportState
                        {
                            std::string projectRoot;
                            bridge::StableId projectId;
                            wi::ecs::Entity materialEntity = wi::ecs::INVALID_ENTITY;
                            bridge::MaterialTextureSlot slot = bridge::MaterialTextureSlot::BaseColor;
                            std::string sourcePath;
                            bridge::CreatorTextureImportResult imported;
                        };
                        auto state = std::make_shared<ImportState>();
                        const auto& project = session_->Projects().CurrentProject();
                        state->projectRoot = project.rootPath;
                        state->projectId = project.projectId;
                        state->materialEntity = materialEntity;
                        state->slot = slot;
                        state->sourcePath = sourcePath;
                        studioChrome_.SetStatusText(
                            std::string("MATERIAL TEXTURE // ") + TextureSlotUiName(slot) +
                            " // IMPORTING + REGISTERING // " +
                            fs::u8path(sourcePath).filename().generic_u8string());

                        wi::jobsystem::Execute(materialTextureImportWorkload_,
                            [this, state](wi::jobsystem::JobArgs)
                            {
                                bridge::CreatorTextureWorkflowService workflow;
                                state->imported = workflow.ImportTexture(
                                    state->projectRoot, state->projectId, state->sourcePath);
                                wi::eventhandler::Subscribe_Once(
                                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                    [this, state](std::uint64_t)
                                    {
                                        if (session_ == nullptr) return;
                                        if (!state->imported.succeeded)
                                        {
                                            studioChrome_.SetStatusText(
                                                std::string("MATERIAL TEXTURE // IMPORT FAILED // ") +
                                                state->imported.error);
                                            wi::helper::messageBox(
                                                "Could not prepare the selected material texture.\n" +
                                                state->imported.error,
                                                "Material Texture Import");
                                            RefreshAssetBrowser();
                                            return;
                                        }
                                        auto& liveScene = session_->Scenes().GetScene();
                                        if (liveScene.materials.GetComponent(state->materialEntity) == nullptr)
                                        {
                                            studioChrome_.SetStatusText(
                                                "MATERIAL TEXTURE // IMPORTED // TARGET NO LONGER EXISTS");
                                            RefreshAssetBrowser();
                                            return;
                                        }
                                        bridge::PreparedMaterialTextureAsset prepared;
                                        std::string error;
                                        if (!bridge::PrepareMaterialTextureAsset(
                                                state->projectRoot, state->projectId,
                                                state->imported.assetId, prepared, error))
                                        {
                                            studioChrome_.SetStatusText(
                                                "MATERIAL TEXTURE // PREPARE FAILED // " + error);
                                            RefreshAssetBrowser();
                                            return;
                                        }
                                        auto command = std::make_unique<bridge::SetMaterialTextureAssetCommand>(
                                            liveScene, state->materialEntity, state->slot, std::move(prepared));
                                        if (!session_->Commands().Execute(std::move(command)))
                                        {
                                            studioChrome_.SetStatusText("MATERIAL TEXTURE // ASSIGN FAILED");
                                            RefreshAssetBrowser();
                                            return;
                                        }
                                        RefreshAssetBrowser();
                                        RefreshInspector();
                                        RefreshStatus();
                                        studioChrome_.SetStatusText(
                                            std::string("MATERIAL TEXTURE // ") + TextureSlotUiName(state->slot) +
                                            " // GOVERNED + ASSIGNED // " +
                                            fs::u8path(state->sourcePath).filename().generic_u8string());
                                    });
                            });
                    });
            });
    }

    void StudioRenderPath::ClearSelectedMaterialTexture(const bridge::MaterialTextureSlot slot)
    {
        if (session_ == nullptr || materialInspectorEntity_ == wi::ecs::INVALID_ENTITY)
            return;
        auto& scene = session_->Scenes().GetScene();
        if (session_->Commands().Execute(
                std::make_unique<bridge::ClearMaterialTextureAssetCommand>(
                    scene, materialInspectorEntity_, slot)))
        {
            RefreshInspector();
            RefreshStatus();
        }
    }
}
