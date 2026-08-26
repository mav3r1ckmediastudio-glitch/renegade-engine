#include "renegade/bridge/CreatorModelMaterialPreparationService.h"

#include "renegade/bridge/CreatorSurfaceBuilderService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/ResourceImportService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
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

        bool CanPassThroughPrepackedSurface(const fs::path& surface)
        {
            // Wicked can load authored DDS Surface maps directly. Renegade's
            // CPU Surface builder is intentionally used for channel packing,
            // but its image decoder does not decode DDS. Do not destroy a
            // valid prepacked Wicked Surface merely to repack it through an
            // unsupported decoder; govern/load the DDS as-authored instead.
            return !surface.empty() &&
                LowerAscii(surface.extension().generic_u8string()) == ".dds";
        }

        std::string BuildPreviewSurfaceRevision(
            const fs::path& surface,
            const fs::path& roughness,
            const fs::path& metalness,
            const fs::path& occlusion,
            const CreatorMaterialSourceOverride& settings)
        {
            // Wicked caches resources by filename. A creator-selected Surface
            // replacement must therefore produce a new preview filename, or
            // Load() will return the previous GPU resource even though the
            // source picker and material recipe both contain the new path.
            std::uint64_t hash = 14695981039346656037ull;
            const auto append = [&hash](const std::string& value)
            {
                for (const unsigned char byte : value)
                {
                    hash ^= byte;
                    hash *= 1099511628211ull;
                }
                hash ^= 0xffu;
                hash *= 1099511628211ull;
            };
            const auto appendPath = [&append](const fs::path& path)
            {
                append(path.generic_u8string());
                if (path.empty())
                    return;
                std::error_code ec;
                const auto size = fs::file_size(path, ec);
                append(ec ? std::string("missing") : std::to_string(size));
                ec.clear();
                const auto stamp = fs::last_write_time(path, ec);
                append(ec ? std::string("unstamped")
                    : std::to_string(stamp.time_since_epoch().count()));
            };
            appendPath(surface);
            appendPath(roughness);
            appendPath(metalness);
            appendPath(occlusion);
            append(std::to_string(settings.roughnessValue));
            append(std::to_string(settings.metalnessValue));
            append(std::to_string(settings.reflectanceValue));
            append(std::to_string(settings.aoStrengthValue));

            std::ostringstream out;
            out << std::hex << std::setw(16) << std::setfill('0') << hash;
            return out.str();
        }
    }

    std::string CreatorMaterialPreviewSurfaceRevision(
        const std::string& surfacePath,
        const std::string& roughnessPath,
        const std::string& metalnessPath,
        const std::string& occlusionPath,
        const CreatorMaterialSourceOverride& settings)
    {
        return BuildPreviewSurfaceRevision(
            fs::u8path(surfacePath),
            fs::u8path(roughnessPath),
            fs::u8path(metalnessPath),
            fs::u8path(occlusionPath),
            settings);
    }

    bool ApplyCreatorModelMaterialPreview(
        wi::scene::Scene& scene,
        const std::string& modelSourcePath,
        const std::string& previewOutputDirectory,
        const std::vector<CreatorMaterialSourceOverride>& overrides,
        const std::vector<wi::ecs::Entity>& materialEntities,
        std::string& error)
    {
        std::error_code ec;
        const fs::path model = fs::weakly_canonical(
            fs::u8path(modelSourcePath), ec);
        if (ec || !fs::is_regular_file(model, ec) || ec)
        {
            error = "Material preview source is unavailable.";
            return false;
        }
        const fs::path directory = model.parent_path();
        const fs::path outputDirectory = fs::u8path(previewOutputDirectory);
        const std::string modelStem = SanitizeStem(model.stem().generic_u8string());

        const std::size_t materialCount = materialEntities.empty()
            ? scene.materials.GetCount()
            : materialEntities.size();
        for (std::size_t index = 0; index < materialCount; ++index)
        {
            const auto entity = materialEntities.empty()
                ? scene.materials.GetEntity(index)
                : index < materialEntities.size()
                    ? materialEntities[index]
                    : wi::ecs::INVALID_ENTITY;
            auto* material = scene.materials.GetComponent(entity);
            if (material == nullptr)
                continue;
            const auto* name = scene.names.GetComponent(entity);
            const std::string materialName = name != nullptr && !name->name.empty()
                ? name->name : "Material_" + std::to_string(index + 1);
            const auto stems = CandidateStems(materialName, modelStem);
            const auto* creator = FindOverride(
                overrides, static_cast<std::uint32_t>(index));
            if (creator == nullptr)
                continue;

            auto choice = [creator](const CreatorTextureSourceChoice CreatorMaterialSourceOverride::* member)
                -> const CreatorTextureSourceChoice*
            {
                return &(creator->*member);
            };
            fs::path baseColor = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::baseColor), directory,
                material->textures[wi::scene::MaterialComponent::BASECOLORMAP].name,
                stems, "_color", error);
            if (!error.empty()) return false;
            fs::path normal = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::normal), directory,
                material->textures[wi::scene::MaterialComponent::NORMALMAP].name,
                stems, "_normal", error);
            if (!error.empty()) return false;
            fs::path surface = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::surface), directory,
                material->textures[wi::scene::MaterialComponent::SURFACEMAP].name,
                stems, "_surface", error);
            if (!error.empty()) return false;
            fs::path emissive = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::emissive), directory,
                material->textures[wi::scene::MaterialComponent::EMISSIVEMAP].name,
                stems, "_emissive", error);
            if (!error.empty()) return false;
            fs::path occlusion = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::occlusion), directory,
                material->textures[wi::scene::MaterialComponent::OCCLUSIONMAP].name,
                stems, "_ao", error);
            if (!error.empty()) return false;
            fs::path roughness = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::roughness), directory,
                {}, stems, "_roughness", error);
            if (!error.empty()) return false;
            fs::path metalness = ChooseTexture(
                choice(&CreatorMaterialSourceOverride::metalness), directory,
                {}, stems, "_metalness", error);
            if (!error.empty()) return false;

            const bool prepackedSurfacePassThrough =
                CanPassThroughPrepackedSurface(surface);
            if (!prepackedSurfacePassThrough &&
                (!surface.empty() || !roughness.empty() ||
                    !metalness.empty() || !occlusion.empty()))
            {
                CreatorSurfaceBuildRequest request;
                request.surfacePath = surface.generic_u8string();
                request.roughnessPath = roughness.generic_u8string();
                request.metalnessPath = metalness.generic_u8string();
                request.occlusionPath = occlusion.generic_u8string();
                request.outputPath = (outputDirectory / fs::u8path(
                    modelStem + "_mat" + std::to_string(index) + "_" +
                    CreatorMaterialPreviewSurfaceRevision(
                        surface.generic_u8string(),
                        roughness.generic_u8string(),
                        metalness.generic_u8string(),
                        occlusion.generic_u8string(),
                        *creator) +
                    "_surface.png"))
                    .generic_u8string();
                const auto toByte = [](const float value)
                {
                    return static_cast<std::uint8_t>(std::clamp(
                        value, 0.0f, 1.0f) * 255.0f + 0.5f);
                };
                request.defaultRoughness = toByte(creator->roughnessValue);
                request.defaultMetalness = toByte(creator->metalnessValue);
                request.reflectance = toByte(creator->reflectanceValue);
                request.aoStrength = creator->aoStrengthValue;
                const auto built = BuildCreatorSurfaceMap(request);
                if (!built.succeeded)
                {
                    error = built.error;
                    return false;
                }
                surface = fs::u8path(built.outputPath);
                // AO is now represented by the packed Surface R channel.
                occlusion.clear();
            }

            const auto applyTexture = [material, &error](
                const CreatorTextureSourceChoice& selected,
                const fs::path& path,
                const int slot)
            {
                auto& texture = material->textures[slot];
                if (path.empty())
                {
                    if (selected.overridden)
                    {
                        texture.name.clear();
                        texture.resource = {};
                        material->SetDirty();
                    }
                    return true;
                }
                wi::Resource resource = wi::resourcemanager::Load(
                    path.generic_u8string(),
                    material->GetTextureSlotResourceFlags(
                        static_cast<wi::scene::MaterialComponent::TEXTURESLOT>(slot)));
                if (!resource.IsValid())
                {
                    error = "Could not load creator-selected preview texture: " +
                        path.generic_u8string();
                    return false;
                }
                texture.name = path.generic_u8string();
                texture.resource = std::move(resource);
                material->SetDirty();
                return true;
            };
            if (!applyTexture(creator->baseColor, baseColor,
                    wi::scene::MaterialComponent::BASECOLORMAP) ||
                !applyTexture(creator->normal, normal,
                    wi::scene::MaterialComponent::NORMALMAP) ||
                !applyTexture(creator->surface, surface,
                    wi::scene::MaterialComponent::SURFACEMAP) ||
                !applyTexture(creator->emissive, emissive,
                    wi::scene::MaterialComponent::EMISSIVEMAP) ||
                !applyTexture(creator->occlusion, occlusion,
                    wi::scene::MaterialComponent::OCCLUSIONMAP))
            {
                return false;
            }

            material->SetRoughness(std::clamp(creator->roughnessValue, 0.0f, 1.0f));
            material->SetMetalness(std::clamp(creator->metalnessValue, 0.0f, 1.0f));
            material->SetReflectance(std::clamp(creator->reflectanceValue, 0.0f, 1.0f));
            material->SetNormalMapStrength(std::clamp(
                creator->normalStrengthValue, 0.0f, 4.0f));
            material->SetEmissiveStrength(std::clamp(
                creator->emissiveStrengthValue, 0.0f, 100.0f));
        }
        error.clear();
        return true;
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

            bool surfaceWasGenerated = false;
            const bool prepackedSurfacePassThrough =
                CanPassThroughPrepackedSurface(surface);
            if (!prepackedSurfacePassThrough &&
                (!surface.empty() || !roughness.empty() ||
                    !metalness.empty() || !occlusion.empty()))
            {
                const fs::path generatedDirectory =
                    fs::u8path(request.projectRoot) /
                    "Intermediate" / "GeneratedMaterials";
                const fs::path generatedSurfacePath = generatedDirectory /
                    fs::u8path(modelStem + "_mat" + std::to_string(index) + "_surface.png");
                CreatorSurfaceBuildRequest surfaceRequest;
                surfaceRequest.surfacePath = surface.generic_u8string();
                surfaceRequest.roughnessPath = roughness.generic_u8string();
                surfaceRequest.metalnessPath = metalness.generic_u8string();
                surfaceRequest.occlusionPath = occlusion.generic_u8string();
                surfaceRequest.outputPath = generatedSurfacePath.generic_u8string();
                const auto toByte = [](const float value)
                {
                    return static_cast<std::uint8_t>(std::clamp(
                        value, 0.0f, 1.0f) * 255.0f + 0.5f);
                };
                surfaceRequest.defaultRoughness = toByte(
                    creator == nullptr ? 0.75f : creator->roughnessValue);
                surfaceRequest.defaultMetalness = toByte(
                    creator == nullptr ? 0.0f : creator->metalnessValue);
                surfaceRequest.reflectance = toByte(
                    creator == nullptr ? 0.04f : creator->reflectanceValue);
                surfaceRequest.aoStrength = creator == nullptr
                    ? 1.0f : creator->aoStrengthValue;
                const auto built = BuildCreatorSurfaceMap(surfaceRequest);
                if (!built.succeeded)
                {
                    result.error = "Could not build Wicked surface map for material '" +
                        materialName + "': " + built.error;
                    return result;
                }
                surface = generatedSurfacePath;
                surfaceWasGenerated = true;
                ++result.generatedSurfaceMaps;
            }

            CreatorMaterialImportRecipe recipe;
            recipe.materialIndex = materialIndex;
            recipe.hasScalarSettings = true;
            recipe.roughness = creator == nullptr
                ? std::clamp(material.roughness, 0.0f, 1.0f)
                : std::clamp(creator->roughnessValue, 0.0f, 1.0f);
            recipe.metalness = creator == nullptr
                ? std::clamp(material.metalness, 0.0f, 1.0f)
                : std::clamp(creator->metalnessValue, 0.0f, 1.0f);
            recipe.reflectance = creator == nullptr
                ? std::clamp(material.reflectance, 0.0f, 1.0f)
                : std::clamp(creator->reflectanceValue, 0.0f, 1.0f);
            recipe.normalStrength = creator == nullptr
                ? std::clamp(material.normalMapStrength, 0.0f, 4.0f)
                : std::clamp(creator->normalStrengthValue, 0.0f, 4.0f);
            recipe.aoStrength = creator == nullptr
                ? 1.0f : std::clamp(creator->aoStrengthValue, 0.0f, 1.0f);
            recipe.emissiveStrength = creator == nullptr
                ? std::clamp(material.GetEmissiveStrength(), 0.0f, 100.0f)
                : std::clamp(creator->emissiveStrengthValue, 0.0f, 100.0f);
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
            if (!surfaceWasGenerated)
            {
                recipe.occlusionAssetId = GovernTexture(
                    occlusion, request.projectRoot, request.projectId, governedByPath,
                    result.governedTextures, result.error);
                if (!result.error.empty()) return result;
            }

            result.recipe.materials.push_back(std::move(recipe));
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
