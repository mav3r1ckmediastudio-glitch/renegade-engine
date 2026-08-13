#include "renegade/bridge/CreatorModelMaterialPreparationService.h"

#include "renegade/bridge/CreatorSurfaceBuilderService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/ResourceImportService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string LowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char c)
                {
                    return c >= 'A' && c <= 'Z'
                        ? static_cast<char>(c + ('a' - 'A'))
                        : static_cast<char>(c);
                });
            return value;
        }

        std::string SanitizeStem(std::string value)
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

        bool SupportedTextureFile(const fs::path& path)
        {
            const auto format = DetectResourceSourceFormat(path.generic_u8string());
            return format != ResourceSourceFormat::Unknown &&
                ClassifyResourceSourceFormat(format) == ResourceClass::Texture;
        }

        fs::path ResolveDeclaredTexture(
            const fs::path& modelDirectory,
            const std::string& declared)
        {
            if (declared.empty())
                return {};
            std::error_code ec;
            fs::path path = fs::u8path(declared);
            if (!path.is_absolute())
                path = modelDirectory / path;
            path = fs::weakly_canonical(path, ec);
            if (ec || !fs::is_regular_file(path, ec) || ec || !SupportedTextureFile(path))
                return {};
            return path;
        }

        fs::path FindSuffixTexture(
            const fs::path& directory,
            const std::vector<std::string>& stems,
            const std::string& suffix)
        {
            static const std::array<const char*, 8> extensions = {
                ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".PNG"
            };
            std::error_code ec;
            for (const auto& stem : stems)
            {
                if (stem.empty())
                    continue;
                for (const char* extension : extensions)
                {
                    fs::path candidate = directory / fs::u8path(stem + suffix + extension);
                    if (fs::is_regular_file(candidate, ec) && !ec && SupportedTextureFile(candidate))
                        return fs::weakly_canonical(candidate, ec);
                    ec.clear();
                }
            }
            return {};
        }

        StableId GovernTexture(
            const fs::path& source,
            const std::string& projectRoot,
            const StableId& projectId,
            std::map<std::string, StableId>& cache,
            std::size_t& governedCount,
            std::string& error)
        {
            if (source.empty())
                return {};
            const std::string key = LowerAscii(source.generic_u8string());
            const auto existing = cache.find(key);
            if (existing != cache.end())
                return existing->second;

            CreatorTextureWorkflowService textures;
            CreatorTextureImportResult imported = textures.ImportTexture(
                projectRoot, projectId, source.generic_u8string());
            if (!imported.succeeded || !IsValidStableId(imported.assetId))
            {
                error = imported.error.empty()
                    ? "Could not govern detected material texture: " + source.generic_u8string()
                    : imported.error;
                return {};
            }
            ++governedCount;
            cache.emplace(key, imported.assetId);
            return imported.assetId;
        }

        std::vector<std::string> CandidateStems(
            const std::string& materialName,
            const std::string& modelStem)
        {
            std::vector<std::string> stems;
            const std::string material = SanitizeStem(materialName);
            if (!material.empty()) stems.push_back(material);
            if (!modelStem.empty() &&
                std::find(stems.begin(), stems.end(), modelStem) == stems.end())
                stems.push_back(modelStem);
            return stems;
        }
    }

    CreatorModelMaterialPreparationResult PrepareCreatorModelMaterials(
        const wi::scene::Scene& preparedScene,
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& modelSourcePath)
    {
        CreatorModelMaterialPreparationResult result;
        if (projectRoot.empty() || !IsValidStableId(projectId) || modelSourcePath.empty())
        {
            result.error = "Model material preparation requires an active project and model source.";
            return result;
        }

        std::error_code ec;
        const fs::path model = fs::weakly_canonical(fs::u8path(modelSourcePath), ec);
        if (ec || !fs::is_regular_file(model, ec) || ec)
        {
            result.error = "Model material preparation source is unavailable.";
            return result;
        }
        const fs::path directory = model.parent_path();
        const std::string modelStem = SanitizeStem(model.stem().generic_u8string());
        std::map<std::string, StableId> governedByPath;

        for (std::size_t index = 0; index < preparedScene.materials.GetCount(); ++index)
        {
            const auto materialEntity = preparedScene.materials.GetEntity(index);
            const auto& material = preparedScene.materials[index];
            const auto* name = preparedScene.names.GetComponent(materialEntity);
            const std::string materialName = name != nullptr && !name->name.empty()
                ? name->name : "Material_" + std::to_string(index + 1);
            const auto stems = CandidateStems(materialName, modelStem);

            CreatorMaterialImportRecipe recipe;
            recipe.materialIndex = static_cast<std::uint32_t>(index);

            auto choose = [&](const int slot, const std::string& suffix)
            {
                fs::path path = ResolveDeclaredTexture(directory, material.textures[slot].name);
                if (path.empty())
                    path = FindSuffixTexture(directory, stems, suffix);
                return path;
            };

            const fs::path baseColor = choose(
                wi::scene::MaterialComponent::BASECOLORMAP, "_color");
            const fs::path normal = choose(
                wi::scene::MaterialComponent::NORMALMAP, "_normal");
            fs::path surface = choose(
                wi::scene::MaterialComponent::SURFACEMAP, "_surface");
            const fs::path emissive = choose(
                wi::scene::MaterialComponent::EMISSIVEMAP, "_emissive");
            fs::path occlusion = choose(
                wi::scene::MaterialComponent::OCCLUSIONMAP, "_ao");

            const fs::path roughness = FindSuffixTexture(directory, stems, "_roughness");
            const fs::path metalness = FindSuffixTexture(directory, stems, "_metalness");
            if (surface.empty() && (!roughness.empty() || !metalness.empty() || !occlusion.empty()))
            {
                const fs::path generatedDirectory =
                    fs::u8path(projectRoot) / "Intermediate" / "GeneratedMaterials";
                const fs::path generatedSurface = generatedDirectory /
                    fs::u8path(modelStem + "_mat" + std::to_string(index) + "_surface.png");
                CreatorSurfaceBuildRequest request;
                request.roughnessPath = roughness.generic_u8string();
                request.metalnessPath = metalness.generic_u8string();
                request.occlusionPath = occlusion.generic_u8string();
                request.outputPath = generatedSurface.generic_u8string();
                const auto built = BuildCreatorSurfaceMap(request);
                if (!built.succeeded)
                {
                    result.error = "Could not build Wicked surface map for material '" +
                        materialName + "': " + built.error;
                    return result;
                }
                surface = generatedSurface;
                ++result.generatedSurfaceMaps;
            }

            recipe.baseColorAssetId = GovernTexture(
                baseColor, projectRoot, projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;
            recipe.normalAssetId = GovernTexture(
                normal, projectRoot, projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;
            recipe.surfaceAssetId = GovernTexture(
                surface, projectRoot, projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;
            recipe.emissiveAssetId = GovernTexture(
                emissive, projectRoot, projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;

            // When a packed surface was generated from AO, AO is already in R.
            // Keep a separate Wicked occlusion binding only when the source asset
            // explicitly declared one and no generated surface consumed it.
            if (surface.empty() || result.generatedSurfaceMaps == 0)
            {
                recipe.occlusionAssetId = GovernTexture(
                    occlusion, projectRoot, projectId, governedByPath,
                    result.governedTextures, result.error);
                if (!result.error.empty()) return result;
            }

            if (!recipe.baseColorAssetId.empty() || !recipe.normalAssetId.empty() ||
                !recipe.surfaceAssetId.empty() || !recipe.emissiveAssetId.empty() ||
                !recipe.occlusionAssetId.empty())
            {
                result.recipe.materials.push_back(std::move(recipe));
            }
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
