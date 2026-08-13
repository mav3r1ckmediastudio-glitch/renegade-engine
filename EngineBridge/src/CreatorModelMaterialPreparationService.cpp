#include "renegade/bridge/CreatorModelMaterialPreparationService.h"

#include "renegade/bridge/CreatorSurfaceBuilderService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/ResourceImportService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <vector>

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

        fs::path ResolveTexturePath(
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
            if (ec || !fs::is_regular_file(path, ec) || ec || !SupportedTextureFile(path))
                return {};
            return path;
        }

        fs::path FindSuffixTexture(
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
                        SupportedTextureFile(candidate))
                    {
                        return fs::weakly_canonical(candidate, ec);
                    }
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
                    ? "Could not govern material texture: " + source.generic_u8string()
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

        const CreatorMaterialSourceOverride* FindOverride(
            const std::vector<CreatorMaterialSourceOverride>& overrides,
            const std::uint32_t materialIndex)
        {
            const auto found = std::find_if(overrides.begin(), overrides.end(),
                [materialIndex](const CreatorMaterialSourceOverride& value)
                { return value.materialIndex == materialIndex; });
            return found == overrides.end() ? nullptr : &*found;
        }

        fs::path ChooseTexture(
            const CreatorTextureSourceChoice* creatorChoice,
            const fs::path& modelDirectory,
            const std::string& declared,
            const std::vector<std::string>& stems,
            const std::string& suffix,
            std::string& error)
        {
            if (creatorChoice != nullptr && creatorChoice->overridden)
            {
                if (creatorChoice->path.empty())
                    return {};
                fs::path chosen = ResolveTexturePath(modelDirectory, creatorChoice->path);
                if (chosen.empty())
                {
                    error = "Creator-selected texture is unavailable or unsupported: " +
                        creatorChoice->path;
                }
                return chosen;
            }
            fs::path path = ResolveTexturePath(modelDirectory, declared);
            if (path.empty())
                path = FindSuffixTexture(modelDirectory, stems, suffix);
            return path;
        }
    }

    CreatorModelMaterialPreparationResult PrepareCreatorModelMaterials(
        const CreatorModelMaterialPreparationRequest& request)
    {
        CreatorModelMaterialPreparationResult result;
        if (request.preparedScene == nullptr || request.projectRoot.empty() ||
            !IsValidStableId(request.projectId) || request.modelSourcePath.empty())
        {
            result.error = "Model material preparation requires a prepared scene, active project and model source.";
            return result;
        }

        std::error_code ec;
        const fs::path model = fs::weakly_canonical(
            fs::u8path(request.modelSourcePath), ec);
        if (ec || !fs::is_regular_file(model, ec) || ec)
        {
            result.error = "Model material preparation source is unavailable.";
            return result;
        }
        const fs::path directory = model.parent_path();
        const std::string modelStem = SanitizeStem(model.stem().generic_u8string());
        std::map<std::string, StableId> governedByPath;
        const wi::scene::Scene& preparedScene = *request.preparedScene;

        for (std::size_t index = 0; index < preparedScene.materials.GetCount(); ++index)
        {
            if (index > (std::numeric_limits<std::uint32_t>::max)())
            {
                result.error = "Imported model has more materials than the creator recipe supports.";
                return result;
            }
            const std::uint32_t materialIndex = static_cast<std::uint32_t>(index);
            const auto materialEntity = preparedScene.materials.GetEntity(index);
            const auto& material = preparedScene.materials[index];
            const auto* name = preparedScene.names.GetComponent(materialEntity);
            const std::string materialName = name != nullptr && !name->name.empty()
                ? name->name : "Material_" + std::to_string(index + 1);
            const auto stems = CandidateStems(materialName, modelStem);
            const CreatorMaterialSourceOverride* creator =
                FindOverride(request.overrides, materialIndex);

            auto choice = [creator](const CreatorTextureSourceChoice CreatorMaterialSourceOverride::* member)
                -> const CreatorTextureSourceChoice*
            {
                return creator == nullptr ? nullptr : &(creator->*member);
            };

            fs::path baseColor = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::baseColor), directory,
                material.textures[wi::scene::MaterialComponent::BASECOLORMAP].name,
                stems, "_color", result.error);
            if (!result.error.empty()) return result;
            fs::path normal = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::normal), directory,
                material.textures[wi::scene::MaterialComponent::NORMALMAP].name,
                stems, "_normal", result.error);
            if (!result.error.empty()) return result;
            fs::path surface = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::surface), directory,
                material.textures[wi::scene::MaterialComponent::SURFACEMAP].name,
                stems, "_surface", result.error);
            if (!result.error.empty()) return result;
            fs::path emissive = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::emissive), directory,
                material.textures[wi::scene::MaterialComponent::EMISSIVEMAP].name,
                stems, "_emissive", result.error);
            if (!result.error.empty()) return result;
            fs::path occlusion = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::occlusion), directory,
                material.textures[wi::scene::MaterialComponent::OCCLUSIONMAP].name,
                stems, "_ao", result.error);
            if (!result.error.empty()) return result;

            fs::path roughness = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::roughness), directory,
                {}, stems, "_roughness", result.error);
            if (!result.error.empty()) return result;
            fs::path metalness = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::metalness), directory,
                {}, stems, "_metalness", result.error);
            if (!result.error.empty()) return result;

            bool generatedSurface = false;
            if (surface.empty() &&
                (!roughness.empty() || !metalness.empty() || !occlusion.empty()))
            {
                const fs::path generatedDirectory =
                    fs::u8path(request.projectRoot) /
                    "Intermediate" / "GeneratedMaterials";
                const fs::path generatedSurface = generatedDirectory /
                    fs::u8path(modelStem + "_mat" + std::to_string(index) + "_surface.png");
                CreatorSurfaceBuildRequest surfaceRequest;
                surfaceRequest.roughnessPath = roughness.generic_u8string();
                surfaceRequest.metalnessPath = metalness.generic_u8string();
                surfaceRequest.occlusionPath = occlusion.generic_u8string();
                surfaceRequest.outputPath = generatedSurface.generic_u8string();
                const auto built = BuildCreatorSurfaceMap(surfaceRequest);
                if (!built.succeeded)
                {
                    result.error = "Could not build Wicked surface map for material '" +
                        materialName + "': " + built.error;
                    return result;
                }
                surface = generatedSurface;
                generatedSurface = true;
                ++result.generatedSurfaceMaps;
            }

            CreatorMaterialImportRecipe recipe;
            recipe.materialIndex = materialIndex;
            recipe.baseColorAssetId = GovernTexture(
                baseColor, request.projectRoot, request.projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;
            recipe.normalAssetId = GovernTexture(
                normal, request.projectRoot, request.projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;
            recipe.surfaceAssetId = GovernTexture(
                surface, request.projectRoot, request.projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;
            recipe.emissiveAssetId = GovernTexture(
                emissive, request.projectRoot, request.projectId, governedByPath,
                result.governedTextures, result.error);
            if (!result.error.empty()) return result;

            // AO is already packed into R when Renegade generated the Surface.
            // A separately supplied Surface can still coexist with an explicit
            // Wicked occlusion map, matching the source asset's authored intent.
            if (!generatedSurface)
            {
                recipe.occlusionAssetId = GovernTexture(
                    occlusion, request.projectRoot, request.projectId, governedByPath,
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
