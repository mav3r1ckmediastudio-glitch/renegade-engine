#include "renegade/bridge/DependencyService.h"

#include "renegade/bridge/SceneDocumentService.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

// Gate 5: nlohmann::json, vendored via WickedEngine/Editor/tiny_gltf.h and
// already linked into this library through ModelImporter_GLTF.cpp (see
// EngineBridge/CMakeLists.txt). Included directly, without tiny_gltf.h,
// because only raw JSON structure is needed here -- see
// GltfDependencyDocument's declaration in DependencyService.h for why this
// deliberately does not use tinygltf's own Model-loading API.
#include "json.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace renegade::bridge
{
    namespace
    {
        constexpr std::array<const char*,
            wi::scene::MaterialComponent::TEXTURESLOT_COUNT> MaterialTextureSlots = {
            "base_color", "normal", "surface", "emissive", "displacement",
            "occlusion", "transmission", "sheen_color", "sheen_roughness",
            "clearcoat", "clearcoat_roughness", "clearcoat_normal", "specular",
            "anisotropy", "transparency",
        };

        void AppendWisceneReference(
            WisceneDependencyDocument& document,
            const std::string& path,
            const DependencyClass dependencyClass,
            std::string provenance)
        {
            if (path.empty())
                return;
            document.references.push_back({
                path,
                dependencyClass,
                DependencyRequirement::Required,
                std::move(provenance),
                false,
            });
        }

        void WalkWisceneDependencies(
            const wi::scene::Scene& scene,
            WisceneDependencyDocument& document)
        {
            document.references.clear();
            document.embeddedGeneratedData.clear();

            for (std::size_t component = 0;
                component < scene.materials.GetCount(); ++component)
            {
                const auto& material = scene.materials[component];
                for (std::size_t slot = 0;
                    slot < MaterialTextureSlots.size(); ++slot)
                {
                    AppendWisceneReference(
                        document,
                        material.textures[slot].name,
                        DependencyClass::Texture,
                        "wiscene.material[" + std::to_string(component) +
                            "].texture." + MaterialTextureSlots[slot]);
                }
            }

            // Wicked serializes authored terrain height samples, blend maps
            // and heightmap-modifier pixels directly inside the WISCENE.
            // Capture deterministic evidence of those payloads without
            // pretending that they are separate filesystem dependencies.
            for (std::size_t component = 0;
                component < scene.terrains.GetCount(); ++component)
            {
                const auto& terrain = scene.terrains[component];
                std::vector<std::pair<wi::terrain::Chunk,
                    const wi::terrain::ChunkData*>> chunks;
                chunks.reserve(terrain.chunks.size());
                for (const auto& entry : terrain.chunks)
                    chunks.emplace_back(entry.first, &entry.second);
                std::sort(chunks.begin(), chunks.end(),
                    [](const auto& left, const auto& right)
                    {
                        if (left.first.x != right.first.x)
                            return left.first.x < right.first.x;
                        return left.first.z < right.first.z;
                    });

                const std::string terrainPrefix = "wiscene.terrain[" +
                    std::to_string(component) + "]";
                for (const auto& entry : chunks)
                {
                    const auto& chunk = entry.first;
                    const auto& data = *entry.second;
                    const std::string chunkPrefix = terrainPrefix + ".chunk[" +
                        std::to_string(chunk.x) + "," +
                        std::to_string(chunk.z) + "]";
                    if (!data.heightmap_data.empty())
                    {
                        document.embeddedGeneratedData.push_back({
                            chunkPrefix + ".heightmap",
                            static_cast<std::uint64_t>(data.heightmap_data.size()) *
                                sizeof(data.heightmap_data.front()),
                        });
                    }
                    for (std::size_t layer = 0;
                        layer < data.blendmap_layers.size(); ++layer)
                    {
                        const auto& pixels = data.blendmap_layers[layer].pixels;
                        if (!pixels.empty())
                        {
                            document.embeddedGeneratedData.push_back({
                                chunkPrefix + ".blendmap[" +
                                    std::to_string(layer) + "]",
                                static_cast<std::uint64_t>(pixels.size()),
                            });
                        }
                    }
                }

                for (std::size_t modifierIndex = 0;
                    modifierIndex < terrain.modifiers.size(); ++modifierIndex)
                {
                    const auto* modifier = terrain.modifiers[modifierIndex].get();
                    if (modifier == nullptr ||
                        modifier->type != wi::terrain::Modifier::Type::Heightmap)
                    {
                        continue;
                    }
                    const auto* heightmap =
                        static_cast<const wi::terrain::HeightmapModifier*>(modifier);
                    if (!heightmap->data.empty())
                    {
                        document.embeddedGeneratedData.push_back({
                            terrainPrefix + ".modifier[" +
                                std::to_string(modifierIndex) + "].heightmap",
                            static_cast<std::uint64_t>(heightmap->data.size()),
                        });
                    }
                }
            }

            for (std::size_t component = 0;
                component < scene.lights.GetCount(); ++component)
            {
                const auto& light = scene.lights[component];
                for (std::size_t texture = 0;
                    texture < light.lensFlareNames.size(); ++texture)
                {
                    AppendWisceneReference(
                        document,
                        light.lensFlareNames[texture],
                        DependencyClass::Texture,
                        "wiscene.light[" + std::to_string(component) +
                            "].lens_flare[" + std::to_string(texture) + "]");
                }
            }

            for (std::size_t component = 0;
                component < scene.probes.GetCount(); ++component)
            {
                AppendWisceneReference(
                    document,
                    scene.probes[component].textureName,
                    DependencyClass::Texture,
                    "wiscene.environment_probe[" +
                        std::to_string(component) + "].texture");
            }

            for (std::size_t component = 0;
                component < scene.weathers.GetCount(); ++component)
            {
                const auto& weather = scene.weathers[component];
                const std::string prefix = "wiscene.weather[" +
                    std::to_string(component) + "].";
                AppendWisceneReference(document, weather.skyMapName,
                    DependencyClass::Texture, prefix + "sky_map");
                AppendWisceneReference(document, weather.colorGradingMapName,
                    DependencyClass::Texture, prefix + "color_grading_map");
                AppendWisceneReference(document,
                    weather.volumetricCloudsWeatherMapFirstName,
                    DependencyClass::Texture, prefix + "cloud_weather_map_first");
                AppendWisceneReference(document,
                    weather.volumetricCloudsWeatherMapSecondName,
                    DependencyClass::Texture, prefix + "cloud_weather_map_second");
            }

            for (std::size_t component = 0;
                component < scene.sounds.GetCount(); ++component)
            {
                AppendWisceneReference(document, scene.sounds[component].filename,
                    DependencyClass::Audio,
                    "wiscene.sound[" + std::to_string(component) + "].filename");
            }

            for (std::size_t component = 0;
                component < scene.videos.GetCount(); ++component)
            {
                AppendWisceneReference(document, scene.videos[component].filename,
                    DependencyClass::Video,
                    "wiscene.video[" + std::to_string(component) + "].filename");
            }

            for (std::size_t component = 0;
                component < scene.scripts.GetCount(); ++component)
            {
                AppendWisceneReference(document, scene.scripts[component].filename,
                    DependencyClass::Script,
                    "wiscene.script[" + std::to_string(component) + "].filename");
            }
        }

        bool IsWithin(const std::filesystem::path& root,
                      const std::filesystem::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *rootPart != *candidatePart)
                    return false;
            }
            return true;
        }

#if !defined(_WIN32)
        bool AsciiPathCaseEquivalent(
            const std::string& left, const std::string& right)
        {
            if (left.size() != right.size())
                return false;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                const auto fold = [](const unsigned char character)
                {
                    return character >= 'A' && character <= 'Z'
                        ? static_cast<unsigned char>(character + ('a' - 'A'))
                        : character;
                };
                if (fold(static_cast<unsigned char>(left[index])) !=
                    fold(static_cast<unsigned char>(right[index])))
                    return false;
            }
            return true;
        }
#endif

#if defined(_WIN32)
        bool TryDecodeUtf8(const std::string& value, std::wstring& decoded)
        {
            if (value.size() > static_cast<std::size_t>(
                    (std::numeric_limits<int>::max)()))
                return false;
            const int size = static_cast<int>(value.size());
            const int required = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size, nullptr, 0);
            if (required <= 0)
                return false;
            decoded.resize(static_cast<std::size_t>(required));
            return MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size,
                decoded.data(), required) == required;
        }
#endif

        bool PathCaseEquivalent(
            const std::string& left, const std::string& right)
        {
            if (left == right)
                return true;
#if defined(_WIN32)
            std::wstring decodedLeft;
            std::wstring decodedRight;
            if (!TryDecodeUtf8(left, decodedLeft) ||
                !TryDecodeUtf8(right, decodedRight))
                return false;
            return CompareStringOrdinal(
                decodedLeft.data(), static_cast<int>(decodedLeft.size()),
                decodedRight.data(), static_cast<int>(decodedRight.size()),
                TRUE) == CSTR_EQUAL;
#else
            // Renegade's supported target is Windows x64. Keep host-side
            // syntax tests deterministic without pretending that a POSIX
            // byte string implements Windows Unicode filename semantics.
            return AsciiPathCaseEquivalent(left, right);
#endif
        }

        std::string StablePathId(const std::string& path)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (const unsigned char value : path)
            {
                hash ^= value;
                hash *= 1099511628211ull;
            }
            std::ostringstream stream;
            stream << "asset:" << std::hex << std::setfill('0')
                   << std::setw(16) << hash;
            return stream.str();
        }

        DependencyDiagnostic PathDiagnostic(
            const DependencyDiagnosticCode code,
            const std::string& sourceId,
            const std::string& path,
            const std::string& message)
        {
            return {code, sourceId, path, message};
        }
    }

    void InspectWisceneDependencies(
        const wi::scene::Scene& scene,
        WisceneDependencyDocument& document)
    {
        WalkWisceneDependencies(scene, document);
    }

    WisceneDependencyReader MakeWisceneDependencyReader()
    {
        return [](const std::string& path,
            WisceneDependencyDocument& document,
            std::string& error)
        {
            document.references.clear();
            auto prepared = PrepareWickedSceneOpen(path);
            if (!prepared.IsReady() || prepared.ReadOnlyScene() == nullptr)
            {
                error = prepared.Error().empty()
                    ? "The WISCENE was not ready for dependency inspection."
                    : prepared.Error();
                return false;
            }
            InspectWisceneDependencies(*prepared.ReadOnlyScene(), document);
            error.clear();
            return true;
        };
    }

    namespace
    {
        // Percent-decodes a glTF URI. The spec requires this: whitespace and
        // other reserved characters in a URI may be represented as %XX
        // escapes (e.g. "a b.png" is declared as "a%20b.png"), and the raw
        // escaped form is never a valid filesystem path.
        std::string PercentDecodeUri(const std::string& uri)
        {
            std::string decoded;
            decoded.reserve(uri.size());
            for (std::size_t index = 0; index < uri.size(); ++index)
            {
                if (uri[index] == '%' && index + 2 < uri.size())
                {
                    const auto hexToNibble = [](const char character)
                        -> int
                    {
                        if (character >= '0' && character <= '9')
                            return character - '0';
                        if (character >= 'a' && character <= 'f')
                            return character - 'a' + 10;
                        if (character >= 'A' && character <= 'F')
                            return character - 'A' + 10;
                        return -1;
                    };
                    const int high = hexToNibble(uri[index + 1]);
                    const int low = hexToNibble(uri[index + 2]);
                    if (high >= 0 && low >= 0)
                    {
                        decoded.push_back(static_cast<char>(
                            (high << 4) | low));
                        index += 2;
                        continue;
                    }
                }
                decoded.push_back(uri[index]);
            }
            return decoded;
        }

        bool IsDataUri(const std::string& uri)
        {
            constexpr char DataScheme[] = "data:";
            if (uri.size() < sizeof(DataScheme) - 1)
                return false;
            for (std::size_t index = 0; index < sizeof(DataScheme) - 1; ++index)
            {
                const unsigned char character =
                    static_cast<unsigned char>(uri[index]);
                const char folded = character >= 'A' && character <= 'Z'
                    ? static_cast<char>(character + ('a' - 'A'))
                    : static_cast<char>(character);
                if (folded != DataScheme[index])
                    return false;
            }
            return true;
        }

        bool IsGltfSourcePath(const std::string& path)
        {
            std::string extension =
                std::filesystem::u8path(path).extension().generic_u8string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char character)
                {
                    return character >= 'A' && character <= 'Z'
                        ? static_cast<char>(character + ('a' - 'A'))
                        : static_cast<char>(character);
                });
            return extension == ".gltf" || extension == ".glb";
        }

        // A glTF (.gltf) file is the JSON text directly. A GLB (.glb) file
        // is a binary container: a 12-byte header ("glTF" magic, version,
        // total length) followed by chunks, each a 4-byte length + 4-byte
        // type + payload; the JSON chunk (type 0x4E4F534A, "JSON" as a
        // little-endian uint32) is always first per spec. This extracts just
        // that JSON text from either form without touching any binary
        // geometry data.
        bool ExtractGltfJsonText(
            const std::vector<std::uint8_t>& bytes,
            std::string& jsonText,
            std::string& error)
        {
            constexpr std::size_t GlbHeaderSize = 12;
            constexpr std::uint32_t GlbMagic = 0x46546C67; // "glTF"
            constexpr std::uint32_t JsonChunkType = 0x4E4F534A; // "JSON"

            const auto readUint32 = [&bytes](const std::size_t offset)
            {
                return static_cast<std::uint32_t>(bytes[offset]) |
                    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                    (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                    (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
            };

            const bool looksLikeGlb =
                bytes.size() >= GlbHeaderSize && readUint32(0) == GlbMagic;
            if (!looksLikeGlb)
            {
                jsonText.assign(bytes.begin(), bytes.end());
                return true;
            }

            constexpr std::size_t ChunkHeaderSize = 8;
            if (bytes.size() < GlbHeaderSize + ChunkHeaderSize)
            {
                error = "GLB file is smaller than its required header.";
                return false;
            }
            const std::uint32_t version = readUint32(4);
            const std::uint32_t declaredLength = readUint32(8);
            if (version != 2 || declaredLength != bytes.size())
            {
                error = "GLB header must declare version 2 and the exact file length.";
                return false;
            }
            const std::uint32_t chunkLength = readUint32(GlbHeaderSize);
            const std::uint32_t chunkType =
                readUint32(GlbHeaderSize + 4);
            const std::size_t chunkDataStart =
                GlbHeaderSize + ChunkHeaderSize;
            if (chunkType != JsonChunkType ||
                bytes.size() < chunkDataStart + chunkLength)
            {
                error = "GLB file's first chunk is not a valid JSON chunk.";
                return false;
            }
            jsonText.assign(
                bytes.begin() + static_cast<std::ptrdiff_t>(chunkDataStart),
                bytes.begin() + static_cast<std::ptrdiff_t>(
                    chunkDataStart + chunkLength));
            return true;
        }

        void CollectGltfArrayUris(
            const nlohmann::json& document,
            const char* arrayKey,
            const DependencyClass dependencyClass,
            const std::string& baseDirectory,
            GltfDependencyDocument& result)
        {
            const auto arrayIt = document.find(arrayKey);
            if (arrayIt == document.end() || !arrayIt->is_array())
                return;
            for (std::size_t index = 0; index < arrayIt->size(); ++index)
            {
                const auto& element = (*arrayIt)[index];
                if (!element.is_object())
                    continue;
                const auto uriIt = element.find("uri");
                if (uriIt == element.end() || !uriIt->is_string())
                    continue; // bufferView-based: embedded, not external.
                const std::string uri = uriIt->get<std::string>();
                if (uri.empty() || IsDataUri(uri))
                    continue; // embedded base64 data, not a file dependency.

                const auto declaredPath =
                    (std::filesystem::u8path(baseDirectory) /
                        std::filesystem::u8path(PercentDecodeUri(uri)))
                        .lexically_normal();
                DependencyCandidate candidate;
                candidate.declaredPath = declaredPath.generic_u8string();
                candidate.dependencyClass = dependencyClass;
                candidate.requirement = DependencyRequirement::Required;
                candidate.provenance = std::string("gltf.") + arrayKey + "[" +
                    std::to_string(index) + "].uri";
                result.references.push_back(std::move(candidate));
            }
        }

        std::uint32_t CountGltfMaterialTextureSlots(
            const nlohmann::json& document)
        {
            const auto materials = document.find("materials");
            if (materials == document.end() || !materials->is_array())
                return 0;

            std::uint32_t count = 0;
            const auto countTextureInfo = [&count](
                const nlohmann::json& object, const char* key)
            {
                const auto value = object.find(key);
                if (value != object.end() && value->is_object() &&
                    value->find("index") != value->end() &&
                    (*value)["index"].is_number_integer())
                {
                    ++count;
                }
            };
            for (const auto& material : *materials)
            {
                if (!material.is_object())
                    continue;
                const auto pbr = material.find("pbrMetallicRoughness");
                if (pbr != material.end() && pbr->is_object())
                {
                    countTextureInfo(*pbr, "baseColorTexture");
                    countTextureInfo(*pbr, "metallicRoughnessTexture");
                }
                countTextureInfo(material, "normalTexture");
                countTextureInfo(material, "occlusionTexture");
                countTextureInfo(material, "emissiveTexture");
            }
            return count;
        }
    }

    GltfDependencyReader MakeGltfDependencyReader()
    {
        return [](const std::string& path,
            GltfDependencyDocument& document,
            std::string& error)
        {
            document.references.clear();
            document.materialTextureSlotCount = 0;
            document.animationCount = 0;

            std::ifstream stream(
                std::filesystem::u8path(path), std::ios::binary);
            if (!stream.is_open())
            {
                error = "Could not open the glTF/GLB file: " + path;
                return false;
            }
            const std::vector<std::uint8_t> bytes(
                (std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
            stream.close();

            std::string jsonText;
            if (!ExtractGltfJsonText(bytes, jsonText, error))
                return false;

            const nlohmann::json parsed = nlohmann::json::parse(
                jsonText, /* callback */ nullptr, /* allow_exceptions */ false);
            if (parsed.is_discarded() || !parsed.is_object())
            {
                error = "The glTF/GLB file's JSON structure could not be parsed: " + path;
                return false;
            }

            const auto asset = parsed.find("asset");
            if (asset == parsed.end() || !asset->is_object())
            {
                error = "The glTF/GLB document is missing its required asset object: " +
                    path;
                return false;
            }
            const auto version = asset->find("version");
            if (version == asset->end() || !version->is_string() ||
                version->get<std::string>().rfind("2.", 0) != 0)
            {
                error = "The glTF/GLB document is not glTF 2.x: " + path;
                return false;
            }

            const std::string baseDirectory =
                std::filesystem::u8path(path).parent_path().generic_u8string();
            CollectGltfArrayUris(parsed, "buffers",
                DependencyClass::ImportedContent, baseDirectory, document);
            CollectGltfArrayUris(parsed, "images",
                DependencyClass::Texture, baseDirectory, document);
            document.materialTextureSlotCount =
                CountGltfMaterialTextureSlots(parsed);
            const auto animations = parsed.find("animations");
            document.animationCount = animations != parsed.end() &&
                animations->is_array()
                ? static_cast<std::uint32_t>(animations->size())
                : 0;

            error.clear();
            return true;
        };
    }

    DependencyPathResult ResolveDependencyPath(
        const std::string& projectRoot,
        const std::string& declaredPath)
    {
        DependencyPathResult result;
        if (projectRoot.empty() || declaredPath.empty())
        {
            result.error = "Project root and dependency path are required.";
            return result;
        }

#if defined(_WIN32)
        std::wstring decodedProjectRoot;
        std::wstring decodedDeclaredPath;
        if (!TryDecodeUtf8(projectRoot, decodedProjectRoot) ||
            !TryDecodeUtf8(declaredPath, decodedDeclaredPath))
        {
            result.error = "Project root and dependency path must be valid UTF-8.";
            return result;
        }
#endif

        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(
            std::filesystem::u8path(projectRoot), error);
        if (error || !std::filesystem::is_directory(root, error))
        {
            result.error = "Project root does not resolve to a directory.";
            return result;
        }

        const std::filesystem::path declared =
            std::filesystem::u8path(declaredPath);
        if (declared.is_absolute())
        {
            result.error = "Absolute dependency paths are outside the project.";
            return result;
        }

        const auto lexical = (root / declared).lexically_normal();
        if (!IsWithin(root, lexical))
        {
            result.error = "Dependency path escapes the project root.";
            return result;
        }

        const auto resolved = std::filesystem::weakly_canonical(lexical, error);
        if (error || !IsWithin(root, resolved))
        {
            result.error = "Dependency resolves outside the project root.";
            return result;
        }

        const auto declaredRelative =
            lexical.lexically_relative(root).lexically_normal();
        if (declaredRelative.empty())
        {
            result.error = "Dependency path could not be made project-relative.";
            return result;
        }

        result.accepted = true;
        result.exists = std::filesystem::is_regular_file(resolved, error) && !error;
        result.absolutePath = resolved.generic_u8string();
        result.canonicalRelativePath = declaredRelative.generic_u8string();
        return result;
    }

    DependencyPathRegistration DependencyPathRegistry::Register(
        const std::string& sourceId,
        const std::string& canonicalRelativePath)
    {
        DependencyPathRegistration result;
        const auto entry = std::find_if(
            registeredPaths_.begin(), registeredPaths_.end(),
            [&canonicalRelativePath](const std::string& existing)
            {
                return PathCaseEquivalent(existing, canonicalRelativePath);
            });
        if (entry == registeredPaths_.end())
        {
            registeredPaths_.push_back(canonicalRelativePath);
            result.inserted = true;
            return result;
        }

        result.existingCanonicalRelativePath = *entry;
        const bool exactDuplicate = *entry == canonicalRelativePath;
        result.diagnostics.push_back({
            exactDuplicate ? DependencyDiagnosticCode::Duplicate
                           : DependencyDiagnosticCode::CaseCollision,
            sourceId,
            canonicalRelativePath,
            exactDuplicate
                ? "Dependency path was already registered."
                : "Dependency path differs from an existing path only by case: " +
                    *entry,
        });
        return result;
    }

    namespace
    {
        bool ValidateProviderContext(
            const DependencyProviderContext& context,
            std::string& absolutePath,
            std::string& error)
        {
            if (context.source == nullptr)
            {
                error = "Dependency provider requires a source node.";
                return false;
            }
            const auto resolved = ResolveDependencyPath(
                context.projectRoot, context.source->projectRelativePath);
            if (!resolved.accepted || !resolved.exists)
            {
                error = resolved.accepted
                    ? "Dependency source document does not exist."
                    : resolved.error;
                return false;
            }
            absolutePath = resolved.absolutePath;
            return true;
        }

        void EmitPath(const DependencyCandidateSink& emit,
            std::string path, const DependencyClass dependencyClass,
            std::string provenance)
        {
            if (!path.empty())
                emit({std::move(path), dependencyClass,
                    DependencyRequirement::Required,
                    std::move(provenance), false});
        }

        // Rewrites an absolute declared path into a project-relative one
        // wherever that is lexically possible, so ResolveDependencyPath sees
        // an ordinary relative path instead of unconditionally rejecting an
        // absolute one. Used for content whose declared paths are computed
        // or resolved at runtime rather than authored as project-relative
        // strings: WISCENE native components (e.g. terrain's runtime-built
        // default-material paths) and glTF buffer/image URIs (declared
        // relative to the glTF file's own directory, not the project root).
        DependencyCandidate ProjectRelativeCandidate(
            const std::string& projectRoot,
            const DependencyCandidate& reference)
        {
            DependencyCandidate candidate = reference;
            const auto declared =
                std::filesystem::u8path(candidate.declaredPath);
            if (!declared.is_absolute())
                return candidate;

            std::error_code error;
            const auto root = std::filesystem::weakly_canonical(
                std::filesystem::u8path(projectRoot), error);
            if (error)
                return candidate;

            const auto relative = declared.lexically_normal().lexically_relative(root);
            if (!relative.empty())
                candidate.declaredPath = relative.generic_u8string();
            return candidate;
        }
    }

    ProjectDependencyProvider::ProjectDependencyProvider(ProjectDependencyReader reader)
        : reader_(std::move(reader)) {}
    const char* ProjectDependencyProvider::Name() const noexcept { return "project-document"; }
    std::uint32_t ProjectDependencyProvider::Version() const noexcept { return 1; }
    bool ProjectDependencyProvider::Supports(DependencyClass value) const noexcept
    { return value == DependencyClass::ProjectDocument; }
    bool ProjectDependencyProvider::Discover(const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&, std::string& error) const
    {
        std::string path;
        if (!reader_ || !ValidateProviderContext(context, path, error))
        {
            if (!reader_) error = "Project dependency reader is not configured.";
            return false;
        }
        ProjectDependencyDocument document;
        if (!reader_(path, document, error)) return false;
        EmitPath(emit, document.startupScene, DependencyClass::Scene,
            "project.startup_scene");
        EmitPath(emit, document.startupFlow, DependencyClass::StoryFlowDocument,
            "project.startup_flow");
        EmitPath(emit, document.startupScreen, DependencyClass::RuntimeScreenDocument,
            "project.startup_screen");
        for (std::size_t index = 0; index < document.alwaysInclude.size(); ++index)
        {
            DependencyCandidate candidate = document.alwaysInclude[index];
            if (candidate.declaredPath.empty())
                continue;
            candidate.requirement = DependencyRequirement::Required;
            if (candidate.provenance.empty())
                candidate.provenance =
                    "project.always_include[" + std::to_string(index) + "]";
            emit(candidate);
        }
        error.clear();
        return true;
    }

    StoryFlowDependencyProvider::StoryFlowDependencyProvider(StoryFlowDependencyReader reader)
        : reader_(std::move(reader)) {}
    const char* StoryFlowDependencyProvider::Name() const noexcept { return "story-flow"; }
    std::uint32_t StoryFlowDependencyProvider::Version() const noexcept { return 1; }
    bool StoryFlowDependencyProvider::Supports(DependencyClass value) const noexcept
    { return value == DependencyClass::StoryFlowDocument; }
    bool StoryFlowDependencyProvider::Discover(const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&, std::string& error) const
    {
        std::string path;
        if (!reader_ || !ValidateProviderContext(context, path, error))
        {
            if (!reader_) error = "Story Flow dependency reader is not configured.";
            return false;
        }
        StoryFlowDependencyDocument document;
        if (!reader_(path, document, error)) return false;
        for (std::size_t index = 0; index < document.scenePathHints.size(); ++index)
            EmitPath(emit, document.scenePathHints[index], DependencyClass::Scene,
                "story_flow.level[" + std::to_string(index) + "].scene_path_hint");
        error.clear();
        return true;
    }

    RuntimeScreenDependencyProvider::RuntimeScreenDependencyProvider(
        RuntimeScreenDependencyReader reader) : reader_(std::move(reader)) {}
    const char* RuntimeScreenDependencyProvider::Name() const noexcept { return "runtime-screen"; }
    std::uint32_t RuntimeScreenDependencyProvider::Version() const noexcept { return 1; }
    bool RuntimeScreenDependencyProvider::Supports(DependencyClass value) const noexcept
    { return value == DependencyClass::RuntimeScreenDocument; }
    bool RuntimeScreenDependencyProvider::Discover(const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&, std::string& error) const
    {
        std::string path;
        if (!reader_ || !ValidateProviderContext(context, path, error))
        {
            if (!reader_) error = "Runtime Screen dependency reader is not configured.";
            return false;
        }
        RuntimeScreenDependencyDocument document;
        if (!reader_(path, document, error)) return false;
        for (std::size_t index = 0; index < document.imagePaths.size(); ++index)
            EmitPath(emit, document.imagePaths[index], DependencyClass::Texture,
                "runtime_screen.image[" + std::to_string(index) + "].resource");
        for (std::size_t index = 0; index < document.fontPaths.size(); ++index)
            EmitPath(emit, document.fontPaths[index], DependencyClass::Font,
                "runtime_screen.font[" + std::to_string(index) + "].resource");
        error.clear();
        return true;
    }

    WisceneDependencyProvider::WisceneDependencyProvider(
        WisceneDependencyReader reader) : reader_(std::move(reader)) {}
    const char* WisceneDependencyProvider::Name() const noexcept
    { return "wiscene"; }
    std::uint32_t WisceneDependencyProvider::Version() const noexcept
    { return 1; }
    bool WisceneDependencyProvider::Supports(DependencyClass value) const noexcept
    { return value == DependencyClass::Scene; }
    bool WisceneDependencyProvider::Discover(
        const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&,
        std::string& error) const
    {
        std::string path;
        if (!reader_ || !ValidateProviderContext(context, path, error))
        {
            if (!reader_) error = "WISCENE dependency reader is not configured.";
            return false;
        }
        WisceneDependencyDocument document;
        if (!reader_(path, document, error)) return false;
        for (const auto& reference : document.references)
            emit(ProjectRelativeCandidate(context.projectRoot, reference));
        error.clear();
        return true;
    }

    GltfDependencyProvider::GltfDependencyProvider(
        GltfDependencyReader reader) : reader_(std::move(reader)) {}
    const char* GltfDependencyProvider::Name() const noexcept
    { return "gltf"; }
    std::uint32_t GltfDependencyProvider::Version() const noexcept
    { return 1; }
    bool GltfDependencyProvider::Supports(DependencyClass value) const noexcept
    { return value == DependencyClass::ImportedContent; }
    bool GltfDependencyProvider::Discover(
        const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&,
        std::string& error) const
    {
        std::string path;
        if (!reader_ || !ValidateProviderContext(context, path, error))
        {
            if (!reader_) error = "glTF dependency reader is not configured.";
            return false;
        }
        // ImportedContent is intentionally broader than glTF: Phase 4 also
        // admits OBJ, FBX, VRM/VRMA and PLY. This provider must be inert for
        // those formats so registering it cannot make unrelated imported
        // roots fail JSON parsing.
        if (!IsGltfSourcePath(path))
        {
            error.clear();
            return true;
        }
        GltfDependencyDocument document;
        if (!reader_(path, document, error)) return false;
        for (const auto& reference : document.references)
            emit(ProjectRelativeCandidate(context.projectRoot, reference));
        error.clear();
        return true;
    }

    DeclaredReferenceDependencyProvider::DeclaredReferenceDependencyProvider(
        std::vector<DeclaredDependencyReference> references)
        : references_(std::move(references))
    {
        std::stable_sort(references_.begin(), references_.end(),
            [](const auto& left, const auto& right)
            {
                if (left.sourcePath != right.sourcePath)
                    return left.sourcePath < right.sourcePath;
                return left.candidate.declaredPath < right.candidate.declaredPath;
            });
    }
    const char* DeclaredReferenceDependencyProvider::Name() const noexcept
    { return "declared-reference"; }
    std::uint32_t DeclaredReferenceDependencyProvider::Version() const noexcept { return 1; }
    bool DeclaredReferenceDependencyProvider::Supports(DependencyClass value) const noexcept
    { return value == DependencyClass::Script || value == DependencyClass::Data; }
    bool DeclaredReferenceDependencyProvider::Discover(
        const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&, std::string& error) const
    {
        if (context.source == nullptr)
        {
            error = "Declared-reference provider requires a source node.";
            return false;
        }
        for (const auto& reference : references_)
            if (reference.sourcePath == context.source->projectRelativePath)
                emit(reference.candidate);
        error.clear();
        return true;
    }

    LuaDependencyPolicyProvider::LuaDependencyPolicyProvider(
        std::vector<DeclaredDependencyReference> declarations,
        std::vector<LuaComputedDependencyReference> computedReferences)
        : declarations_(std::move(declarations)),
          computedReferences_(std::move(computedReferences))
    {
        std::stable_sort(declarations_.begin(), declarations_.end(),
            [](const auto& left, const auto& right)
            {
                if (left.sourcePath != right.sourcePath)
                    return left.sourcePath < right.sourcePath;
                if (left.candidate.declaredPath != right.candidate.declaredPath)
                {
                    return left.candidate.declaredPath <
                        right.candidate.declaredPath;
                }
                return left.candidate.provenance < right.candidate.provenance;
            });
        std::stable_sort(computedReferences_.begin(), computedReferences_.end(),
            [](const auto& left, const auto& right)
            {
                if (left.sourcePath != right.sourcePath)
                    return left.sourcePath < right.sourcePath;
                if (left.expression != right.expression)
                    return left.expression < right.expression;
                return left.provenance < right.provenance;
            });
    }

    const char* LuaDependencyPolicyProvider::Name() const noexcept
    {
        return "lua-policy";
    }

    std::uint32_t LuaDependencyPolicyProvider::Version() const noexcept
    {
        return 1;
    }

    bool LuaDependencyPolicyProvider::Supports(
        const DependencyClass value) const noexcept
    {
        return value == DependencyClass::Script;
    }

    bool LuaDependencyPolicyProvider::Discover(
        const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink& diagnose,
        std::string& error) const
    {
        if (context.source == nullptr)
        {
            error = "Lua policy provider requires a source node.";
            return false;
        }

        for (const auto& declaration : declarations_)
        {
            if (declaration.sourcePath != context.source->projectRelativePath)
                continue;
            if (declaration.candidate.dependencyClass != DependencyClass::Script ||
                declaration.candidate.declaredPath.empty())
            {
                error = "Lua policy contains an invalid declared script dependency.";
                return false;
            }
            emit(declaration.candidate);
        }

        for (const auto& reference : computedReferences_)
        {
            if (reference.sourcePath != context.source->projectRelativePath)
                continue;
            diagnose({
                DependencyDiagnosticCode::UndeclaredComputedReference,
                reference.expression,
                "Computed Lua dependency is not explicitly declared" +
                    (reference.provenance.empty()
                        ? std::string{"."}
                        : ": " + reference.provenance),
            });
        }

        error.clear();
        return true;
    }

    DependencyCollector::DependencyCollector(std::string projectRoot)
        : projectRoot_(std::move(projectRoot))
    {
    }

    bool DependencyCollector::RegisterProvider(
        const IDependencyProvider& provider,
        std::string& error)
    {
        const std::string name = provider.Name() == nullptr
            ? std::string{} : std::string(provider.Name());
        if (name.empty() || provider.Version() == 0)
        {
            error = "Dependency providers require a name and non-zero version.";
            return false;
        }
        if (!providers_.emplace(name, &provider).second)
        {
            error = "A dependency provider named '" + name + "' is already registered.";
            return false;
        }
        error.clear();
        return true;
    }

    bool DependencyCollector::AddRoot(
        const DependencyRoot& root,
        std::string& error)
    {
        const auto resolved = ResolveDependencyPath(projectRoot_, root.declaredPath);
        if (!resolved.accepted)
        {
            graph_.diagnostics.push_back(PathDiagnostic(
                DependencyDiagnosticCode::OutsideProject,
                {}, root.declaredPath, resolved.error));
            error = resolved.error;
            return false;
        }

        const auto registration = pathRegistry_.Register(
            {}, resolved.canonicalRelativePath);
        graph_.diagnostics.insert(
            graph_.diagnostics.end(),
            registration.diagnostics.begin(), registration.diagnostics.end());
        if (!registration.inserted)
        {
            error = "Dependency root path was already registered.";
            return false;
        }

        DependencyNode node;
        node.id = StablePathId(resolved.canonicalRelativePath);
        node.projectRelativePath = resolved.canonicalRelativePath;
        node.dependencyClass = root.dependencyClass;
        node.requirement = root.requirement;
        node.provider = root.provenance.empty() ? "root" : root.provenance;
        node.providerVersion = 1;
        graph_.rootIds.push_back(node.id);
        graph_.nodes.push_back(std::move(node));
        if (!resolved.exists)
        {
            graph_.diagnostics.push_back(PathDiagnostic(
                DependencyDiagnosticCode::Missing,
                graph_.rootIds.back(), resolved.canonicalRelativePath,
                root.requirement == DependencyRequirement::Required
                    ? "Required graph root does not exist."
                    : "Declared graph root does not exist."));
        }
        error.clear();
        return true;
    }

    bool DependencyCollector::DiscoverRootDependencies(std::string& error)
    {
        const std::size_t rootCount = graph_.rootIds.size();
        for (std::size_t index = 0; index < rootCount; ++index)
        {
            const DependencyNode source = graph_.nodes[index];
            for (const auto& [name, provider] : providers_)
            {
                (void)name;
                if (!provider->Supports(source.dependencyClass))
                    continue;

                DependencyProviderContext context{projectRoot_, &source};
                std::vector<DependencyCandidate> candidates;
                std::vector<DependencyProviderDiagnostic> diagnostics;
                std::string providerError;
                const bool succeeded = provider->Discover(
                    context,
                    [&candidates](const DependencyCandidate& candidate)
                    {
                        candidates.push_back(candidate);
                    },
                    [&diagnostics](
                        const DependencyProviderDiagnostic& diagnostic)
                    {
                        diagnostics.push_back(diagnostic);
                    },
                    providerError);
                if (!succeeded)
                {
                    error = "Dependency provider '" + std::string(provider->Name()) +
                        "' failed for " + source.projectRelativePath + ": " +
                        providerError;
                    return false;
                }
                for (const auto& candidate : candidates)
                    AcceptCandidate(source, *provider, candidate);
                for (const auto& diagnostic : diagnostics)
                {
                    graph_.diagnostics.push_back(PathDiagnostic(
                        diagnostic.code, source.id, diagnostic.path,
                        diagnostic.message));
                }
            }
        }
        error.clear();
        return true;
    }

    const DependencyGraph& DependencyCollector::Graph() const noexcept
    {
        return graph_;
    }

    void DependencyCollector::AcceptCandidate(
        const DependencyNode& source,
        const IDependencyProvider& provider,
        const DependencyCandidate& candidate)
    {
        const auto resolved = ResolveDependencyPath(projectRoot_, candidate.declaredPath);
        if (!resolved.accepted)
        {
            graph_.diagnostics.push_back(PathDiagnostic(
                DependencyDiagnosticCode::OutsideProject,
                source.id, candidate.declaredPath, resolved.error));
            return;
        }

        const auto registration = pathRegistry_.Register(
            source.id, resolved.canonicalRelativePath);
        graph_.diagnostics.insert(
            graph_.diagnostics.end(),
            registration.diagnostics.begin(), registration.diagnostics.end());

        const std::string& identityPath = registration.inserted
            ? resolved.canonicalRelativePath
            : registration.existingCanonicalRelativePath;
        const std::string targetId = StablePathId(identityPath);
        if (registration.inserted)
        {
            DependencyNode node;
            node.id = targetId;
            node.projectRelativePath = resolved.canonicalRelativePath;
            node.dependencyClass = candidate.dependencyClass;
            node.requirement = candidate.requirement;
            node.provider = provider.Name();
            node.providerVersion = provider.Version();
            node.runtimeSupport = candidate.runtimeSupport;
            graph_.nodes.push_back(std::move(node));
            if (!resolved.exists)
            {
                graph_.diagnostics.push_back(PathDiagnostic(
                    DependencyDiagnosticCode::Missing,
                    source.id, resolved.canonicalRelativePath,
                    "Declared dependency does not exist."));
            }
        }
        graph_.edges.push_back({source.id, targetId, candidate.provenance});
    }

    namespace
    {
        constexpr std::array<std::pair<DependencyClass, const char*>, 13>
            DependencyClassNames = {{
                {DependencyClass::ProjectDocument, "project_document"},
                {DependencyClass::StoryFlowDocument, "story_flow_document"},
                {DependencyClass::RuntimeScreenDocument, "runtime_screen_document"},
                {DependencyClass::Scene, "scene"},
                {DependencyClass::ImportedContent, "imported_content"},
                {DependencyClass::Texture, "texture"},
                {DependencyClass::Audio, "audio"},
                {DependencyClass::Video, "video"},
                {DependencyClass::Font, "font"},
                {DependencyClass::Script, "script"},
                {DependencyClass::Data, "data"},
                {DependencyClass::GeneratedData, "generated_data"},
                {DependencyClass::RuntimeSupport, "runtime_support"},
            }};
    }

    const char* DependencyClassName(const DependencyClass dependencyClass) noexcept
    {
        for (const auto& entry : DependencyClassNames)
            if (entry.first == dependencyClass)
                return entry.second;
        return "";
    }

    bool TryParseDependencyClassName(
        const std::string& name, DependencyClass& dependencyClass) noexcept
    {
        for (const auto& entry : DependencyClassNames)
        {
            if (name == entry.second)
            {
                dependencyClass = entry.first;
                return true;
            }
        }
        return false;
    }
}
