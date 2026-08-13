#include "renegade/bridge/CreatorModelMaterialPreparationService.h"

#include "renegade/bridge/ResourceImportService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <vector>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string DetectionSanitizeStem(std::string value)
        {
            for (char& c : value)
            {
                const unsigned char b = static_cast<unsigned char>(c);
                if (!((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
                    (b >= '0' && b <= '9') || c == '-' || c == '_'))
                    c = '_';
            }
            while (!value.empty() && value.front() == '_') value.erase(value.begin());
            while (!value.empty() && value.back() == '_') value.pop_back();
            return value.empty() ? std::string("Material") : value;
        }

        bool DetectionSupportedTexture(const fs::path& path)
        {
            const auto format = DetectResourceSourceFormat(path.generic_u8string());
            return format != ResourceSourceFormat::Unknown &&
                ClassifyResourceSourceFormat(format) == ResourceClass::Texture;
        }

        fs::path DetectionResolveDeclared(
            const fs::path& modelDirectory,
            const std::string& value)
        {
            if (value.empty())
                return {};
            std::error_code ec;
            fs::path path = fs::u8path(value);
            if (!path.is_absolute())
                path = modelDirectory / path;
            path = fs::weakly_canonical(path, ec);
            if (ec || !fs::is_regular_file(path, ec) || ec ||
                !DetectionSupportedTexture(path))
                return {};
            return path;
        }

        fs::path DetectionFindSuffix(
            const fs::path& directory,
            const std::vector<std::string>& stems,
            const std::string& suffix)
        {
            static const std::array<const char*, 14> extensions = {
                ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG", ".tga",
                ".TGA", ".bmp", ".BMP", ".dds", ".DDS", ".hdr", ".HDR"
            };
            std::error_code ec;
            for (const auto& stem : stems)
            {
                if (stem.empty())
                    continue;
                for (const char* extension : extensions)
                {
                    const fs::path candidate =
                        directory / fs::u8path(stem + suffix + extension);
                    if (fs::is_regular_file(candidate, ec) && !ec &&
                        DetectionSupportedTexture(candidate))
                        return fs::weakly_canonical(candidate, ec);
                    ec.clear();
                }
            }
            return {};
        }

        CreatorTextureSourceChoice DetectionChoice(const fs::path& path)
        {
            CreatorTextureSourceChoice choice;
            if (!path.empty())
            {
                choice.overridden = true;
                choice.path = path.generic_u8string();
            }
            return choice;
        }
    }

    CreatorModelMaterialDetectionResult DetectCreatorModelMaterials(
        const wi::scene::Scene& preparedScene,
        const std::string& modelSourcePath)
    {
        CreatorModelMaterialDetectionResult result;
        if (modelSourcePath.empty())
        {
            result.error = "Material detection requires a model source path.";
            return result;
        }
        std::error_code ec;
        const fs::path model = fs::weakly_canonical(fs::u8path(modelSourcePath), ec);
        if (ec || !fs::is_regular_file(model, ec) || ec)
        {
            result.error = "Material detection model source is unavailable.";
            return result;
        }
        const fs::path directory = model.parent_path();
        const std::string modelStem = DetectionSanitizeStem(
            model.stem().generic_u8string());

        for (std::size_t index = 0; index < preparedScene.materials.GetCount(); ++index)
        {
            if (index > (std::numeric_limits<std::uint32_t>::max)())
            {
                result.error = "Imported model has more materials than the creator importer supports.";
                result.materials.clear();
                return result;
            }
            const auto entity = preparedScene.materials.GetEntity(index);
            const auto& material = preparedScene.materials[index];
            const auto* name = preparedScene.names.GetComponent(entity);
            const std::string materialStem = DetectionSanitizeStem(
                name != nullptr && !name->name.empty()
                    ? name->name
                    : "Material_" + std::to_string(index + 1));
            std::vector<std::string> stems{materialStem};
            if (modelStem != materialStem)
                stems.push_back(modelStem);

            const auto choose = [&](const int slot, const char* suffix)
            {
                fs::path path = DetectionResolveDeclared(
                    directory, material.textures[slot].name);
                if (path.empty())
                    path = DetectionFindSuffix(directory, stems, suffix);
                return path;
            };

            CreatorMaterialSourceOverride detected;
            detected.materialIndex = static_cast<std::uint32_t>(index);
            detected.baseColor = DetectionChoice(choose(
                wi::scene::MaterialComponent::BASECOLORMAP, "_color"));
            detected.normal = DetectionChoice(choose(
                wi::scene::MaterialComponent::NORMALMAP, "_normal"));
            detected.surface = DetectionChoice(choose(
                wi::scene::MaterialComponent::SURFACEMAP, "_surface"));
            detected.occlusion = DetectionChoice(choose(
                wi::scene::MaterialComponent::OCCLUSIONMAP, "_ao"));
            detected.emissive = DetectionChoice(choose(
                wi::scene::MaterialComponent::EMISSIVEMAP, "_emissive"));
            detected.roughness = DetectionChoice(
                DetectionFindSuffix(directory, stems, "_roughness"));
            detected.metalness = DetectionChoice(
                DetectionFindSuffix(directory, stems, "_metalness"));
            const bool hasAuthoredSurface = detected.surface.overridden ||
                detected.roughness.overridden || detected.metalness.overridden ||
                detected.occlusion.overridden;
            detected.roughnessValue = std::clamp(
                hasAuthoredSurface ? material.roughness : 0.75f, 0.0f, 1.0f);
            detected.metalnessValue = std::clamp(
                hasAuthoredSurface ? material.metalness : 0.0f, 0.0f, 1.0f);
            detected.reflectanceValue = std::clamp(
                hasAuthoredSurface ? material.reflectance : 0.04f, 0.0f, 1.0f);
            detected.normalStrengthValue = std::clamp(
                material.normalMapStrength, 0.0f, 4.0f);
            detected.aoStrengthValue = 1.0f;
            detected.emissiveStrengthValue = std::clamp(
                material.GetEmissiveStrength(), 0.0f, 100.0f);
            result.materials.push_back(std::move(detected));
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
