#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class LightmapUvSource : std::uint8_t
    {
        CopyUv0,
        CopyUv1,
        KeepAtlas,
        GenerateAtlas,
    };

    struct LightmapBakeSettings
    {
        std::uint32_t resolution = 512;
        LightmapUvSource uvSource = LightmapUvSource::GenerateAtlas;
        bool blockCompression = true;
    };

    struct VertexAoBakeSettings
    {
        std::uint32_t rayCount = 256;
        float rayLength = 100.0f;
    };

    struct LightmapBakeStatus
    {
        std::size_t targetCount = 0;
        std::size_t bakedCount = 0;
        std::size_t activeCount = 0;
        std::size_t vertexAoCount = 0;
        std::uint32_t highestIteration = 0;
        bool eligible = false;
        std::string message;
    };

    class LightmapBakeSession
    {
    public:
        LightmapBakeSession();
        ~LightmapBakeSession();
        LightmapBakeSession(LightmapBakeSession&&) noexcept;
        LightmapBakeSession& operator=(LightmapBakeSession&&) noexcept;

        LightmapBakeSession(const LightmapBakeSession&) = delete;
        LightmapBakeSession& operator=(const LightmapBakeSession&) = delete;

        [[nodiscard]] bool IsActive() const noexcept;
        [[nodiscard]] const std::vector<wi::ecs::Entity>& Targets() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        friend class LightmapBakeService;
    };

    class LightmapBakeService
    {
    public:
        [[nodiscard]] static bool IsValidResolution(std::uint32_t resolution) noexcept;

        [[nodiscard]] static LightmapBakeStatus CaptureStatus(
            const wi::scene::Scene& scene,
            const std::vector<wi::ecs::Entity>& targets);

        // Wicked serializes baked lightmap bytes but the pinned ObjectComponent
        // read path only rebuilds Vertex AO render data. Rehydrate the native
        // lightmap texture before a prepared scene is adopted by Studio/Runtime.
        static bool HydratePersistedLightmaps(
            wi::scene::Scene& scene,
            std::string& error);

        static bool BeginBake(
            wi::scene::Scene& scene,
            const std::vector<wi::ecs::Entity>& targets,
            const LightmapBakeSettings& settings,
            LightmapBakeSession& session,
            std::string& error);

        static bool StopAndSave(
            wi::scene::Scene& scene,
            LightmapBakeSession& session,
            CommandService& commands,
            std::string& error);

        static void Cancel(
            wi::scene::Scene& scene,
            LightmapBakeSession& session) noexcept;

        static bool ClearLightmaps(
            wi::scene::Scene& scene,
            const std::vector<wi::ecs::Entity>& targets,
            CommandService& commands,
            std::string& error);

        static bool ComputeVertexAo(
            wi::scene::Scene& scene,
            const std::vector<wi::ecs::Entity>& targets,
            const VertexAoBakeSettings& settings,
            CommandService& commands,
            std::string& error);

        static bool ClearVertexAo(
            wi::scene::Scene& scene,
            const std::vector<wi::ecs::Entity>& targets,
            CommandService& commands,
            std::string& error);
    };
}
