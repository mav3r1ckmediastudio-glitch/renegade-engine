#include "renegade/bridge/RenderLutService.h"

#include <wiInitializer.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::array<unsigned char, 8> PngSignature = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    };

    std::uint32_t ReadBigEndian32(const unsigned char* bytes) noexcept
    {
        return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
            (static_cast<std::uint32_t>(bytes[1]) << 16u) |
            (static_cast<std::uint32_t>(bytes[2]) << 8u) |
            static_cast<std::uint32_t>(bytes[3]);
    }

    bool IsWithin(const fs::path& child, const fs::path& parent)
    {
        auto childPart = child.begin();
        for (auto parentPart = parent.begin(); parentPart != parent.end();
            ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
                return false;
        }
        return true;
    }

    bool ResolveProjectRoot(
        const std::string& projectRoot,
        fs::path& root,
        std::string& error)
    {
        if (projectRoot.empty())
        {
            error = "A color-grading LUT requires an active project.";
            return false;
        }
        std::error_code ec;
        root = fs::weakly_canonical(fs::absolute(fs::u8path(projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            error = "The active project root is unavailable.";
            return false;
        }
        error.clear();
        return true;
    }

    bool IsProjectContentPath(const fs::path& relative)
    {
        if (relative.empty() || relative.is_absolute())
            return false;
        const auto first = relative.begin();
        if (first == relative.end() || first->generic_u8string() != "Content")
            return false;
        for (const auto& part : relative)
        {
            if (part == "..")
                return false;
        }
        return true;
    }

    std::string BuiltInDisplayName(const std::size_t oneBasedIndex)
    {
        std::ostringstream stream;
        stream << "BUILT-IN // " << std::setw(2) << std::setfill('0')
               << oneBasedIndex;
        return stream.str();
    }

    bool CopyValidatedLut(
        const fs::path& source,
        const fs::path& destination,
        std::string& error)
    {
        if (!renegade::bridge::ValidateColorGradingLutPng(
                source.generic_u8string(), error))
        {
            return false;
        }
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            error = "Could not create the project LUT library directory.";
            return false;
        }
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            error = "Could not copy the LUT into project Content.";
            return false;
        }
        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    bool ValidateColorGradingLutPng(
        const std::string& filePath,
        std::string& error)
    {
        const fs::path path = fs::u8path(filePath);
        std::string extension = path.extension().generic_u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension != ".png")
        {
            error = "Color-grading LUTs must be PNG files.";
            return false;
        }

        std::ifstream input(path, std::ios::binary);
        std::array<unsigned char, 33> header{};
        if (!input.read(reinterpret_cast<char*>(header.data()),
                static_cast<std::streamsize>(header.size())))
        {
            error = "The LUT PNG could not be read.";
            return false;
        }
        if (!std::equal(PngSignature.begin(), PngSignature.end(), header.begin()) ||
            std::string(reinterpret_cast<const char*>(header.data() + 12), 4) != "IHDR")
        {
            error = "The selected file is not a valid PNG image.";
            return false;
        }

        const std::uint32_t width = ReadBigEndian32(header.data() + 16);
        const std::uint32_t height = ReadBigEndian32(header.data() + 20);
        const unsigned char bitDepth = header[24];
        const unsigned char colorType = header[25];
        if (width != 256 || height != 16 || bitDepth != 8 || colorType != 6)
        {
            error =
                "Wicked color-grading LUTs must be 256x16, 8-bit RGBA PNG files.";
            return false;
        }

        error.clear();
        return true;
    }

    bool InstallBuiltInColorGradingLut(
        const std::string& projectRoot,
        const std::string& builtInLibraryRoot,
        const std::size_t oneBasedIndex,
        std::string& projectRelativePath,
        std::string& error)
    {
        projectRelativePath.clear();
        if (oneBasedIndex == 0 || oneBasedIndex > BuiltInColorGradingLutCount)
        {
            error = "The selected built-in LUT index is outside the Renegade library.";
            return false;
        }

        fs::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return false;
        const fs::path source = fs::u8path(builtInLibraryRoot) /
            (std::to_string(oneBasedIndex) + ".png");
        const fs::path relative = fs::path("Content") / "LUTs" / "BuiltIn" /
            (std::to_string(oneBasedIndex) + ".png");
        const fs::path destination = root / relative;
        if (!CopyValidatedLut(source, destination, error))
            return false;

        projectRelativePath = relative.generic_u8string();
        return true;
    }

    bool ImportCustomColorGradingLut(
        const std::string& projectRoot,
        const std::string& sourcePath,
        std::string& projectRelativePath,
        std::string& error)
    {
        projectRelativePath.clear();
        if (!ValidateColorGradingLutPng(sourcePath, error))
            return false;

        fs::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return false;

        const fs::path source = fs::u8path(sourcePath);
        const fs::path directory = root / "Content" / "LUTs" / "Custom";
        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec)
        {
            error = "Could not create Content/LUTs/Custom.";
            return false;
        }

        std::string stem = source.stem().generic_u8string();
        if (stem.empty())
            stem = "CustomLUT";
        fs::path destination = directory / (stem + ".png");
        std::size_t suffix = 2;
        while (fs::exists(destination, ec) && !ec)
        {
            destination = directory /
                (stem + "_" + std::to_string(suffix++) + ".png");
        }
        if (ec)
        {
            error = "Could not inspect the project LUT library.";
            return false;
        }

        if (!CopyValidatedLut(source, destination, error))
            return false;
        projectRelativePath = fs::relative(destination, root, ec).generic_u8string();
        if (ec || projectRelativePath.empty())
        {
            error = "Could not resolve the imported LUT inside the project.";
            return false;
        }
        error.clear();
        return true;
    }

    std::vector<ColorGradingLutEntry> ListProjectColorGradingLuts(
        const std::string& projectRoot)
    {
        std::vector<ColorGradingLutEntry> result;
        fs::path root;
        std::string ignored;
        if (!ResolveProjectRoot(projectRoot, root, ignored))
            return result;

        const fs::path directory = root / "Content" / "LUTs" / "Custom";
        std::error_code ec;
        if (!fs::is_directory(directory, ec) || ec)
            return result;

        for (const auto& entry : fs::directory_iterator(directory, ec))
        {
            if (ec) break;
            if (!entry.is_regular_file(ec) || ec) continue;
            std::string validationError;
            if (!ValidateColorGradingLutPng(
                    entry.path().generic_u8string(), validationError))
            {
                continue;
            }
            const auto relative = fs::relative(entry.path(), root, ec);
            if (ec) break;
            result.push_back({
                std::string("PROJECT // ") + entry.path().stem().generic_u8string(),
                relative.generic_u8string(),
                false,
            });
        }
        std::sort(result.begin(), result.end(),
            [](const ColorGradingLutEntry& left, const ColorGradingLutEntry& right)
            { return left.displayName < right.displayName; });
        return result;
    }

    bool ResolveColorGradingLutPath(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& absolutePath,
        std::string& error)
    {
        absolutePath.clear();
        fs::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return false;
        const fs::path relative = fs::u8path(projectRelativePath).lexically_normal();
        if (!IsProjectContentPath(relative))
        {
            error = "The LUT reference must be a safe project-relative Content path.";
            return false;
        }
        std::error_code ec;
        const fs::path candidate = fs::weakly_canonical(root / relative, ec);
        if (ec || !fs::is_regular_file(candidate, ec) || ec ||
            !IsWithin(candidate, root))
        {
            error = "The referenced project LUT is missing or outside the project.";
            return false;
        }
        if (!ValidateColorGradingLutPng(candidate.generic_u8string(), error))
            return false;
        absolutePath = candidate.generic_u8string();
        error.clear();
        return true;
    }

    bool RefreshColorGradingLutResource(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        std::string& error)
    {
        if (scene.weathers.GetCount() == 0)
        {
            error.clear();
            return true;
        }
        auto& weather = scene.weathers[0];
        if (weather.colorGradingMapName.empty())
        {
            weather.colorGradingMap = {};
            scene.weather = weather;
            error.clear();
            return true;
        }
        if (weather.colorGradingMap.IsValid())
        {
            scene.weather = weather;
            error.clear();
            return true;
        }

        std::string absolutePath;
        if (!ResolveColorGradingLutPath(
                projectRoot, weather.colorGradingMapName, absolutePath, error))
        {
            return false;
        }
        if (!wi::initializer::IsInitializeFinished())
        {
            scene.weather = weather;
            error.clear();
            return true;
        }

        auto resource = wi::resourcemanager::Load(
            absolutePath,
            wi::resourcemanager::Flags::IMPORT_COLORGRADINGLUT);
        if (!resource.IsValid() || !resource.GetTexture().IsValid())
        {
            error = "Wicked could not decode the project color-grading LUT.";
            return false;
        }
        weather.colorGradingMap = std::move(resource);
        scene.weather = weather;
        error.clear();
        return true;
    }

    SetColorGradingLutCommand::SetColorGradingLutCommand(
        wi::scene::Scene& scene,
        std::string projectRoot,
        std::string projectRelativePath)
        : scene_(&scene)
        , projectRoot_(std::move(projectRoot))
        , afterName_(std::move(projectRelativePath))
        , resolvedWeatherBefore_(scene.weather)
    {
        if (scene.weathers.GetCount() > 0)
        {
            entity_ = scene.weathers.GetEntity(0);
            const auto& weather = scene.weathers[0];
            beforeName_ = weather.colorGradingMapName;
            beforeResource_ = weather.colorGradingMap;
        }
    }

    bool SetColorGradingLutCommand::Execute()
    {
        if (scene_ == nullptr || beforeName_ == afterName_)
            return false;

        if (createdWeather_ &&
            scene_->weathers.GetComponent(entity_) == nullptr)
        {
            if (!hasSnapshot_)
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            if (scene_->Entity_Serialize(snapshot_, serializer) != entity_)
                return false;
        }

        bool createdThisCall = false;
        if (entity_ == wi::ecs::INVALID_ENTITY)
        {
            if (afterName_.empty())
                return false;
            entity_ = wi::ecs::CreateEntity();
            scene_->names.Create(entity_) = "Environment";
            scene_->weathers.Create(entity_) = resolvedWeatherBefore_;
            createdWeather_ = true;
            createdThisCall = true;
        }

        if (!ApplyAfter())
        {
            if (createdThisCall)
            {
                scene_->Entity_Remove(entity_);
                scene_->weather = resolvedWeatherBefore_;
                entity_ = wi::ecs::INVALID_ENTITY;
                createdWeather_ = false;
            }
            return false;
        }
        return true;
    }

    bool SetColorGradingLutCommand::ApplyAfter()
    {
        auto* weather = scene_->weathers.GetComponent(entity_);
        if (weather == nullptr)
            return false;

        wi::Resource resource;
        if (!afterName_.empty())
        {
            std::string absolutePath;
            std::string error;
            if (!ResolveColorGradingLutPath(
                    projectRoot_, afterName_, absolutePath, error))
            {
                return false;
            }
            if (wi::initializer::IsInitializeFinished())
            {
                resource = wi::resourcemanager::Load(
                    absolutePath,
                    wi::resourcemanager::Flags::IMPORT_COLORGRADINGLUT);
                if (!resource.IsValid() || !resource.GetTexture().IsValid())
                    return false;
            }
        }

        weather->colorGradingMapName = afterName_;
        weather->colorGradingMap = std::move(resource);
        RefreshResolvedWeather();
        return true;
    }

    void SetColorGradingLutCommand::Undo()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY)
            return;
        auto* weather = scene_->weathers.GetComponent(entity_);
        if (weather == nullptr)
            return;

        if (createdWeather_)
        {
            if (!hasSnapshot_)
            {
                snapshot_.SetReadModeAndResetPos(false);
                wi::ecs::EntitySerializer serializer;
                scene_->Entity_Serialize(snapshot_, serializer, entity_);
                hasSnapshot_ = true;
            }
            scene_->Entity_Remove(entity_);
            scene_->weather = resolvedWeatherBefore_;
            return;
        }

        weather->colorGradingMapName = beforeName_;
        weather->colorGradingMap = beforeResource_;
        RefreshResolvedWeather();
    }

    void SetColorGradingLutCommand::RefreshResolvedWeather()
    {
        if (scene_ == nullptr || scene_->weathers.GetCount() == 0)
            return;
        if (scene_->weathers.GetEntity(0) == entity_)
            scene_->weather = scene_->weathers[0];
    }
}
