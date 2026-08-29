#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    inline constexpr std::size_t BuiltInColorGradingLutCount = 50;

    struct ColorGradingLutEntry
    {
        std::string displayName;
        std::string projectRelativePath;
        bool builtIn = false;
    };

    // Wicked's pinned native color-grading importer requires an 8-bit
    // 256x16 RGBA PNG. Validation happens before a file enters project Content
    // so a bad LUT cannot become a latent package/runtime failure.
    [[nodiscard]] bool ValidateColorGradingLutPng(
        const std::string& filePath,
        std::string& error);

    // Copies one Renegade built-in LUT from the Studio installation into the
    // active project. Scenes reference only this project-local copy so Test
    // Level and packaged Runtime never depend on the editor installation.
    [[nodiscard]] bool InstallBuiltInColorGradingLut(
        const std::string& projectRoot,
        const std::string& builtInLibraryRoot,
        std::size_t oneBasedIndex,
        std::string& projectRelativePath,
        std::string& error);

    // Imports one creator-supplied native Wicked LUT into Content/LUTs/Custom.
    // Name collisions receive a deterministic numeric suffix instead of
    // overwriting another project LUT.
    [[nodiscard]] bool ImportCustomColorGradingLut(
        const std::string& projectRoot,
        const std::string& sourcePath,
        std::string& projectRelativePath,
        std::string& error);

    [[nodiscard]] std::vector<ColorGradingLutEntry>
    ListProjectColorGradingLuts(const std::string& projectRoot);

    [[nodiscard]] bool ResolveColorGradingLutPath(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& absolutePath,
        std::string& error);

    // WeatherComponent serializes colorGradingMapName but intentionally not
    // the decoded wi::Resource. Call this after scene replacement/load to
    // restore the native IMPORT_COLORGRADINGLUT resource from the project path.
    [[nodiscard]] bool RefreshColorGradingLutResource(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        std::string& error);

    // Command-backed scene LUT selection. If a blank scene has no serialized
    // WeatherComponent yet, the first LUT selection creates a normal visible
    // Environment carrier from Scene::weather; Undo removes that carrier again.
    class SetColorGradingLutCommand final : public ICommand
    {
    public:
        SetColorGradingLutCommand(
            wi::scene::Scene& scene,
            std::string projectRoot,
            std::string projectRelativePath);

        [[nodiscard]] bool Execute() override;
        void Undo() override;

    private:
        [[nodiscard]] bool ApplyAfter();
        void RefreshResolvedWeather();

        wi::scene::Scene* scene_ = nullptr;
        std::string projectRoot_;
        std::string beforeName_;
        std::string afterName_;
        wi::Resource beforeResource_;
        wi::scene::WeatherComponent resolvedWeatherBefore_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        bool createdWeather_ = false;
        bool hasSnapshot_ = false;
        wi::Archive snapshot_;
    };
}
