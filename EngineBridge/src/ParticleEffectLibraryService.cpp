#include "renegade/bridge/ParticleEffectLibraryService.h"

#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ParticleBlendModeService.h"
#include "renegade/bridge/ResourceImportService.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace
{
    namespace fs = std::filesystem;
    using nlohmann::json;
    using renegade::bridge::ParticleEffectPreset;
    using renegade::bridge::ParticleEffectPresetLayer;
    using renegade::bridge::ParticleEmitterState;
    using renegade::bridge::TransformState;

    std::string EntityName(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const std::string& fallback)
    {
        const auto* name = scene.names.GetComponent(entity);
        return name != nullptr && !name->name.empty() ? name->name : fallback;
    }

    std::string SafePackageStem(std::string value)
    {
        for (char& character : value)
        {
            const unsigned char byte = static_cast<unsigned char>(character);
            if (!(std::isalnum(byte) || character == '-' || character == '_'))
                character = '_';
        }
        while (!value.empty() && value.front() == '_')
            value.erase(value.begin());
        while (!value.empty() && value.back() == '_')
            value.pop_back();
        if (value.empty())
            value = "Particle_Effect";
        if (value.size() > 96)
            value.resize(96);
        return value;
    }

    std::string SourceExtension(
        const renegade::bridge::ResourceSourceFormat format)
    {
        for (const auto& capability : renegade::bridge::GetSupportedResourceFormats())
        {
            if (capability.format == format && capability.extension != nullptr &&
                capability.extension[0] != '\0')
            {
                std::string extension = capability.extension;
                if (!extension.empty() && extension.front() != '.')
                    extension.insert(extension.begin(), '.');
                return extension;
            }
        }
        return ".bin";
    }

    json Vector3(const XMFLOAT3& value)
    {
        return json::array({value.x, value.y, value.z});
    }

    json Vector4(const XMFLOAT4& value)
    {
        return json::array({value.x, value.y, value.z, value.w});
    }

    bool ReadVector3(const json& value, XMFLOAT3& output)
    {
        if (!value.is_array() || value.size() != 3 ||
            !value[0].is_number() || !value[1].is_number() || !value[2].is_number())
            return false;
        output = XMFLOAT3(
            value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        return true;
    }

    bool ReadVector4(const json& value, XMFLOAT4& output)
    {
        if (!value.is_array() || value.size() != 4 ||
            !value[0].is_number() || !value[1].is_number() ||
            !value[2].is_number() || !value[3].is_number())
            return false;
        output = XMFLOAT4(
            value[0].get<float>(), value[1].get<float>(),
            value[2].get<float>(), value[3].get<float>());
        return true;
    }

    json SerializeTransform(const TransformState& state)
    {
        return {
            {"translation", Vector3(state.translation)},
            {"rotation", Vector4(state.rotation)},
            {"scale", Vector3(state.scale)},
        };
    }

    bool DeserializeTransform(const json& root, TransformState& state)
    {
        return root.is_object() && root.contains("translation") &&
            root.contains("rotation") && root.contains("scale") &&
            ReadVector3(root["translation"], state.translation) &&
            ReadVector4(root["rotation"], state.rotation) &&
            ReadVector3(root["scale"], state.scale);
    }

    json SerializeEmitter(const ParticleEmitterState& state)
    {
        return {
            {"shader_type", static_cast<int>(state.shaderType)},
            {"max_particles", state.maxParticles},
            {"fixed_timestep", state.fixedTimestep},
            {"size", state.size},
            {"random_factor", state.randomFactor},
            {"normal_factor", state.normalFactor},
            {"emit_count", state.emitCount},
            {"life", state.life},
            {"random_life", state.randomLife},
            {"scale_x", state.scaleX},
            {"scale_y", state.scaleY},
            {"rotation", state.rotation},
            {"motion_blur", state.motionBlurAmount},
            {"mass", state.mass},
            {"random_color", state.randomColor},
            {"opacity_peak_start", state.opacityPeakStart},
            {"opacity_peak_end", state.opacityPeakEnd},
            {"burst_on_create", state.burstOnCreate},
            {"velocity", Vector3(state.velocity)},
            {"gravity", Vector3(state.gravity)},
            {"drag", state.drag},
            {"restitution", state.restitution},
            {"sph_h", state.sphH},
            {"sph_k", state.sphK},
            {"sph_p0", state.sphP0},
            {"sph_e", state.sphE},
            {"frames_x", state.framesX},
            {"frames_y", state.framesY},
            {"frame_count", state.frameCount},
            {"frame_start", state.frameStart},
            {"frame_rate", state.frameRate},
            {"paused", state.paused},
            {"sorted", state.sorted},
            {"depth_collision", state.depthCollision},
            {"sph", state.sph},
            {"volume", state.volume},
            {"frame_blending", state.frameBlending},
            {"colliders_disabled", state.collidersDisabled},
            {"take_color_from_mesh", state.takeColorFromMesh},
            {"color", Vector4(state.color)},
            {"emissive_color", Vector3(state.emissiveColor)},
            {"emissive_strength", state.emissiveStrength},
        };
    }

    template <typename T>
    bool ReadRequired(const json& root, const char* key, T& output)
    {
        if (!root.contains(key))
            return false;
        try
        {
            output = root.at(key).get<T>();
            return true;
        }
        catch (const json::exception&)
        {
            return false;
        }
    }

    bool DeserializeEmitter(const json& root, ParticleEmitterState& state)
    {
        if (!root.is_object())
            return false;

        int shaderType = 0;
        if (!ReadRequired(root, "shader_type", shaderType) ||
            shaderType < 0 ||
            shaderType >= wi::EmittedParticleSystem::PARTICLESHADERTYPE_COUNT ||
            !ReadRequired(root, "max_particles", state.maxParticles) ||
            !ReadRequired(root, "fixed_timestep", state.fixedTimestep) ||
            !ReadRequired(root, "size", state.size) ||
            !ReadRequired(root, "random_factor", state.randomFactor) ||
            !ReadRequired(root, "normal_factor", state.normalFactor) ||
            !ReadRequired(root, "emit_count", state.emitCount) ||
            !ReadRequired(root, "life", state.life) ||
            !ReadRequired(root, "random_life", state.randomLife) ||
            !ReadRequired(root, "scale_x", state.scaleX) ||
            !ReadRequired(root, "scale_y", state.scaleY) ||
            !ReadRequired(root, "rotation", state.rotation) ||
            !ReadRequired(root, "motion_blur", state.motionBlurAmount) ||
            !ReadRequired(root, "mass", state.mass) ||
            !ReadRequired(root, "random_color", state.randomColor) ||
            !ReadRequired(root, "opacity_peak_start", state.opacityPeakStart) ||
            !ReadRequired(root, "opacity_peak_end", state.opacityPeakEnd) ||
            !ReadRequired(root, "burst_on_create", state.burstOnCreate) ||
            !root.contains("velocity") || !ReadVector3(root["velocity"], state.velocity) ||
            !root.contains("gravity") || !ReadVector3(root["gravity"], state.gravity) ||
            !ReadRequired(root, "drag", state.drag) ||
            !ReadRequired(root, "restitution", state.restitution) ||
            !ReadRequired(root, "sph_h", state.sphH) ||
            !ReadRequired(root, "sph_k", state.sphK) ||
            !ReadRequired(root, "sph_p0", state.sphP0) ||
            !ReadRequired(root, "sph_e", state.sphE) ||
            !ReadRequired(root, "frames_x", state.framesX) ||
            !ReadRequired(root, "frames_y", state.framesY) ||
            !ReadRequired(root, "frame_count", state.frameCount) ||
            !ReadRequired(root, "frame_start", state.frameStart) ||
            !ReadRequired(root, "frame_rate", state.frameRate) ||
            !ReadRequired(root, "paused", state.paused) ||
            !ReadRequired(root, "sorted", state.sorted) ||
            !ReadRequired(root, "depth_collision", state.depthCollision) ||
            !ReadRequired(root, "sph", state.sph) ||
            !ReadRequired(root, "volume", state.volume) ||
            !ReadRequired(root, "frame_blending", state.frameBlending) ||
            !ReadRequired(root, "colliders_disabled", state.collidersDisabled) ||
            !ReadRequired(root, "take_color_from_mesh", state.takeColorFromMesh) ||
            !root.contains("color") || !ReadVector4(root["color"], state.color) ||
            !root.contains("emissive_color") ||
            !ReadVector3(root["emissive_color"], state.emissiveColor) ||
            !ReadRequired(root, "emissive_strength", state.emissiveStrength))
        {
            return false;
        }

        state.shaderType =
            static_cast<wi::EmittedParticleSystem::PARTICLESHADERTYPE>(shaderType);
        state.meshId = wi::ecs::INVALID_ENTITY; // scene-local IDs never enter presets
        state = renegade::bridge::SanitizeParticleEmitterState(state);
        return true;
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return false;
        if (!bytes.empty())
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        return stream.good();
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return false;
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        return stream.good();
    }

    bool CollectSourceLayers(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        std::string& effectName,
        std::vector<wi::ecs::Entity>& entities)
    {
        entities.clear();
        if (renegade::bridge::IsParticleEffectRoot(scene, selected))
        {
            effectName = EntityName(scene, selected, "Particle Effect");
            for (const auto& layer :
                renegade::bridge::CollectParticleEffectLayers(scene, selected))
                entities.push_back(layer.entity);
            return !entities.empty();
        }
        if (renegade::bridge::IsParticleEmitter(scene, selected))
        {
            effectName = EntityName(scene, selected, "Particle Emitter");
            entities.push_back(selected);
            return true;
        }
        return false;
    }
}

namespace renegade::bridge
{
    SaveParticleEffectLibraryResult SaveParticleEffectToLibrary(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity effectRootOrEmitter,
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& libraryRoot,
        const std::string& requestedName)
    {
        SaveParticleEffectLibraryResult result;
        if (projectRoot.empty() || projectId.empty() || libraryRoot.empty())
        {
            result.error = "Particle effect library requires project and library roots.";
            return result;
        }

        std::string capturedName;
        std::vector<wi::ecs::Entity> layerEntities;
        if (!CollectSourceLayers(
                scene, effectRootOrEmitter, capturedName, layerEntities))
        {
            result.error = "Selection is not a particle emitter/effect with at least one layer.";
            return result;
        }

        const std::string effectName = requestedName.empty()
            ? capturedName
            : requestedName;
        const fs::path package = fs::u8path(libraryRoot) /
            (SafePackageStem(effectName) + ParticleEffectPackageExtension);
        const fs::path textures = package / "textures";
        const fs::path temporary = fs::path(package.generic_u8string() + ".tmp");
        std::error_code error;
        fs::remove_all(temporary, error);
        error.clear();
        if (!fs::create_directories(temporary / "textures", error) && error)
        {
            result.error = "Could not create particle effect library package: " + error.message();
            return result;
        }

        json document;
        document["format"] = ParticleEffectLibraryFormat;
        document["schema_version"] = ParticleEffectLibrarySchemaVersion;
        document["name"] = effectName;
        document["layers"] = json::array();

        std::size_t textureIndex = 0;
        for (std::size_t index = 0; index < layerEntities.size(); ++index)
        {
            const auto entity = layerEntities[index];
            const auto* transform = scene.transforms.GetComponent(entity);
            if (!IsParticleEmitter(scene, entity) || transform == nullptr)
            {
                fs::remove_all(temporary, error);
                result.error = "Particle effect contains an invalid emitter layer.";
                return result;
            }

            TransformState local = CaptureTransform(*transform);
            if (!IsParticleEffectRoot(scene, effectRootOrEmitter))
                local.translation = XMFLOAT3(0.0f, 0.0f, 0.0f);

            json layer;
            layer["name"] = EntityName(
                scene, entity, "Layer " + std::to_string(index + 1));
            layer["transform"] = SerializeTransform(local);
            layer["emitter"] = SerializeEmitter(CaptureParticleEmitter(scene, entity));
            layer["blend_mode"] = static_cast<int>(CaptureParticleBlendMode(scene, entity));
            layer["texture"] = "";

            const auto* metadata = scene.metadatas.GetComponent(entity);
            if (metadata != nullptr &&
                metadata->string_values.has(MaterialBaseColorTextureAssetIdMetadataKey))
            {
                const StableId assetId = metadata->string_values.get(
                    MaterialBaseColorTextureAssetIdMetadataKey);
                if (!assetId.empty())
                {
                    PreparedMaterialTextureAsset prepared;
                    std::string prepareError;
                    if (!PrepareMaterialTextureAsset(
                            projectRoot, projectId, assetId, prepared, prepareError))
                    {
                        fs::remove_all(temporary, error);
                        result.error = "Could not bundle particle texture: " + prepareError;
                        return result;
                    }

                    fs::path sourceName = fs::u8path(prepared.logicalResourceName).filename();
                    std::string extension = sourceName.extension().generic_u8string();
                    if (extension.empty())
                        extension = SourceExtension(prepared.sourceFormat);
                    const std::string textureName =
                        "layer_" + std::to_string(index + 1) + "_" +
                        std::to_string(++textureIndex) + extension;
                    const fs::path relative = fs::path("textures") / textureName;
                    if (!WriteBytes(temporary / relative, prepared.payload))
                    {
                        fs::remove_all(temporary, error);
                        result.error = "Could not write bundled particle texture.";
                        return result;
                    }
                    layer["texture"] = relative.generic_u8string();
                    ++result.bundledTextureCount;
                }
            }
            document["layers"].push_back(std::move(layer));
        }

        if (!WriteText(temporary / ParticleEffectManifestName, document.dump(2)))
        {
            fs::remove_all(temporary, error);
            result.error = "Could not write particle effect manifest.";
            return result;
        }

        error.clear();
        fs::create_directories(package.parent_path(), error);
        if (error)
        {
            fs::remove_all(temporary, error);
            result.error = "Could not create particle effect library: " + error.message();
            return result;
        }

        error.clear();
        fs::remove_all(package, error);
        if (error)
        {
            fs::remove_all(temporary, error);
            result.error = "Could not replace existing particle effect preset: " + error.message();
            return result;
        }
        error.clear();
        fs::rename(temporary, package, error);
        if (error)
        {
            fs::remove_all(temporary, error);
            result.error = "Could not publish particle effect preset: " + error.message();
            return result;
        }

        result.succeeded = true;
        result.packagePath = package.generic_u8string();
        result.layerCount = layerEntities.size();
        return result;
    }

    bool LoadParticleEffectPreset(
        const std::string& packagePath,
        ParticleEffectPreset& preset,
        std::string& error)
    {
        preset = {};
        error.clear();
        const fs::path package = fs::u8path(packagePath);
        const fs::path manifest = package / ParticleEffectManifestName;
        std::ifstream stream(manifest, std::ios::binary);
        if (!stream)
        {
            error = "Particle effect manifest is missing or unreadable.";
            return false;
        }
        const std::string text(
            std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        const json document = json::parse(text, nullptr, false);
        if (document.is_discarded() || !document.is_object() ||
            document.value("format", std::string()) != ParticleEffectLibraryFormat ||
            document.value("schema_version", 0u) != ParticleEffectLibrarySchemaVersion ||
            !document.contains("name") || !document["name"].is_string() ||
            !document.contains("layers") || !document["layers"].is_array() ||
            document["layers"].empty())
        {
            error = "Particle effect manifest is malformed or unsupported.";
            return false;
        }

        preset.name = document["name"].get<std::string>();
        for (const auto& layerRoot : document["layers"])
        {
            if (!layerRoot.is_object() ||
                !layerRoot.contains("name") || !layerRoot["name"].is_string() ||
                !layerRoot.contains("transform") ||
                !layerRoot.contains("emitter") ||
                !layerRoot.contains("blend_mode") || !layerRoot["blend_mode"].is_number_integer() ||
                !layerRoot.contains("texture") || !layerRoot["texture"].is_string())
            {
                error = "Particle effect layer is malformed.";
                preset = {};
                return false;
            }

            ParticleEffectPresetLayer layer;
            layer.name = layerRoot["name"].get<std::string>();
            if (!DeserializeTransform(layerRoot["transform"], layer.localTransform) ||
                !DeserializeEmitter(layerRoot["emitter"], layer.emitter))
            {
                error = "Particle effect layer state is malformed.";
                preset = {};
                return false;
            }

            const int blend = layerRoot["blend_mode"].get<int>();
            if (blend < wi::enums::BLENDMODE_OPAQUE ||
                blend >= wi::enums::BLENDMODE_COUNT)
            {
                error = "Particle effect layer has an unsupported blend mode.";
                preset = {};
                return false;
            }
            layer.blendMode = static_cast<wi::enums::BLENDMODE>(blend);

            const std::string texture = layerRoot["texture"].get<std::string>();
            if (!texture.empty())
            {
                const fs::path relative = fs::u8path(texture).lexically_normal();
                if (relative.is_absolute() || relative.empty() ||
                    relative.generic_u8string().rfind("..", 0) == 0)
                {
                    error = "Particle effect texture path escapes its package.";
                    preset = {};
                    return false;
                }
                const fs::path full = package / relative;
                std::error_code pathError;
                if (!fs::is_regular_file(full, pathError) || pathError)
                {
                    error = "Particle effect bundled texture is missing.";
                    preset = {};
                    return false;
                }
                layer.textureLibraryRelativePath = relative.generic_u8string();
            }
            preset.layers.push_back(std::move(layer));
        }
        return true;
    }

    std::vector<ParticleEffectLibraryEntry> ListParticleEffectLibrary(
        const std::string& libraryRoot,
        std::string& warning)
    {
        warning.clear();
        std::vector<ParticleEffectLibraryEntry> entries;
        std::error_code error;
        const fs::path root = fs::u8path(libraryRoot);
        if (!fs::exists(root, error))
            return entries;
        if (error || !fs::is_directory(root, error))
        {
            warning = "Particle effect library root is not readable.";
            return entries;
        }

        for (fs::directory_iterator iterator(root, error), end;
            !error && iterator != end; iterator.increment(error))
        {
            if (!iterator->is_directory(error) || error ||
                iterator->path().extension() != ParticleEffectPackageExtension)
            {
                error.clear();
                continue;
            }

            ParticleEffectPreset preset;
            std::string loadError;
            if (!LoadParticleEffectPreset(
                    iterator->path().generic_u8string(), preset, loadError))
            {
                if (warning.empty())
                    warning = "One or more particle effect presets were skipped: " + loadError;
                continue;
            }
            entries.push_back({
                preset.name,
                iterator->path().generic_u8string(),
                preset.layers.size()});
        }
        if (error && warning.empty())
            warning = "Particle effect library enumeration ended early: " + error.message();

        std::sort(entries.begin(), entries.end(),
            [](const ParticleEffectLibraryEntry& left,
                const ParticleEffectLibraryEntry& right)
            {
                if (left.name != right.name)
                    return left.name < right.name;
                return left.packagePath < right.packagePath;
            });
        return entries;
    }
}
