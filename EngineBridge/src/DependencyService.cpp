#include "renegade/bridge/DependencyService.h"

#include "renegade/bridge/SceneDocumentService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

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

        DependencyCandidate ProjectRelativeWisceneCandidate(
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
        const DependencyCandidateSink& emit, std::string& error) const
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
        const DependencyCandidateSink& emit, std::string& error) const
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
        const DependencyCandidateSink& emit, std::string& error) const
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
            emit(ProjectRelativeWisceneCandidate(context.projectRoot, reference));
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
        const DependencyCandidateSink& emit, std::string& error) const
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
                std::string providerError;
                const bool succeeded = provider->Discover(
                    context,
                    [&candidates](const DependencyCandidate& candidate)
                    {
                        candidates.push_back(candidate);
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
}
