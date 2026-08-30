#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/RenderLutService.h"
#include "renegade/bridge/SceneService.h"

namespace
{
    namespace fs = std::filesystem;

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    void WriteBigEndian32(unsigned char* bytes, const std::uint32_t value)
    {
        bytes[0] = static_cast<unsigned char>((value >> 24u) & 0xFFu);
        bytes[1] = static_cast<unsigned char>((value >> 16u) & 0xFFu);
        bytes[2] = static_cast<unsigned char>((value >> 8u) & 0xFFu);
        bytes[3] = static_cast<unsigned char>(value & 0xFFu);
    }

    bool WritePngHeader(
        const fs::path& path,
        const std::uint32_t width,
        const std::uint32_t height,
        const unsigned char colorType)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) return false;

        std::array<unsigned char, 33> header{};
        constexpr std::array<unsigned char, 8> signature = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        };
        std::copy(signature.begin(), signature.end(), header.begin());
        WriteBigEndian32(header.data() + 8, 13);
        header[12] = 'I';
        header[13] = 'H';
        header[14] = 'D';
        header[15] = 'R';
        WriteBigEndian32(header.data() + 16, width);
        WriteBigEndian32(header.data() + 20, height);
        header[24] = 8;
        header[25] = colorType;
        header[26] = 0;
        header[27] = 0;
        header[28] = 0;

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(header.data()),
            static_cast<std::streamsize>(header.size()));
        return static_cast<bool>(output);
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path root = fs::temp_directory_path() /
        "renegade_phase5_gate5_lut_tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "Content", ec);
    if (ec) return Fail("could not create LUT test project");

    const fs::path validSource = root / "source" / "FilmLook.png";
    const fs::path invalidDimensions = root / "source" / "Wrong.png";
    const fs::path invalidRgb = root / "source" / "Rgb.png";
    if (!WritePngHeader(validSource, 256, 16, 6) ||
        !WritePngHeader(invalidDimensions, 128, 16, 6) ||
        !WritePngHeader(invalidRgb, 256, 16, 2))
    {
        return Fail("could not write LUT validation fixtures");
    }

    std::string error;
    if (!ValidateColorGradingLutPng(validSource.generic_u8string(), error))
        return Fail("valid 256x16 RGBA PNG was rejected");
    if (ValidateColorGradingLutPng(invalidDimensions.generic_u8string(), error))
        return Fail("wrong-size LUT was accepted");
    if (ValidateColorGradingLutPng(invalidRgb.generic_u8string(), error))
        return Fail("RGB LUT was accepted even though Wicked requires RGBA");

    std::string imported;
    if (!ImportCustomColorGradingLut(
            root.generic_u8string(), validSource.generic_u8string(), imported, error) ||
        imported != "Content/LUTs/Custom/FilmLook.png" ||
        !fs::is_regular_file(root / fs::u8path(imported)))
    {
        return Fail("custom LUT import did not create the governed project copy");
    }

    std::string importedSecond;
    if (!ImportCustomColorGradingLut(
            root.generic_u8string(), validSource.generic_u8string(), importedSecond, error) ||
        importedSecond != "Content/LUTs/Custom/FilmLook_2.png")
    {
        return Fail("custom LUT collision did not receive a deterministic suffix");
    }

    const auto projectLuts = ListProjectColorGradingLuts(root.generic_u8string());
    if (projectLuts.size() != 2 ||
        projectLuts[0].projectRelativePath.empty() ||
        projectLuts[1].projectRelativePath.empty())
    {
        return Fail("project LUT library did not enumerate imported LUTs");
    }

    std::string absolute;
    if (!ResolveColorGradingLutPath(
            root.generic_u8string(), imported, absolute, error) ||
        fs::weakly_canonical(fs::u8path(absolute), ec) !=
            fs::weakly_canonical(root / fs::u8path(imported), ec))
    {
        return Fail("project-relative LUT did not resolve inside project Content");
    }
    if (ResolveColorGradingLutPath(
            root.generic_u8string(), "../escape.png", absolute, error))
    {
        return Fail("unsafe LUT path escaped project Content");
    }

    // A blank scene gains one normal Environment carrier when a LUT is first
    // selected. Undo removes it again and Redo restores the same native entity.
    SceneService freshScenes;
    CommandService commands;
    if (!commands.Execute(std::make_unique<SetColorGradingLutCommand>(
            freshScenes.GetScene(), root.generic_u8string(), imported)))
    {
        return Fail("fresh-scene LUT command failed");
    }
    const auto createdEntity = freshScenes.WeatherEntity();
    if (createdEntity == wi::ecs::INVALID_ENTITY ||
        freshScenes.GetScene().weathers[0].colorGradingMapName != imported ||
        freshScenes.GetScene().weather.colorGradingMapName != imported)
    {
        return Fail("fresh-scene LUT command did not author native Weather state");
    }
    if (!commands.Undo() ||
        freshScenes.WeatherEntity() != wi::ecs::INVALID_ENTITY)
    {
        return Fail("fresh-scene LUT Undo did not restore a Weather-free scene");
    }
    if (!commands.Redo() || freshScenes.WeatherEntity() != createdEntity ||
        freshScenes.GetScene().weathers[0].colorGradingMapName != imported)
    {
        return Fail("fresh-scene LUT Redo did not restore the authored Weather entity");
    }

    // The LUT filename is native WeatherComponent serialized data. The GPU
    // resource is intentionally reconstructed later from the project root.
    wi::Archive snapshot;
    snapshot.SetReadModeAndResetPos(false);
    wi::ecs::EntitySerializer writer;
    freshScenes.GetScene().Entity_Serialize(snapshot, writer, createdEntity);
    wi::scene::Scene restored;
    snapshot.SetReadModeAndResetPos(true);
    wi::ecs::EntitySerializer reader;
    restored.Entity_Serialize(snapshot, reader);
    if (restored.weathers.GetCount() != 1 ||
        restored.weathers[0].colorGradingMapName != imported)
    {
        return Fail("native Weather serialization did not preserve the LUT path");
    }

    if (!RefreshColorGradingLutResource(
            restored, root.generic_u8string(), error) ||
        restored.weather.colorGradingMapName != imported)
    {
        return Fail("headless LUT refresh did not preserve serialized scene state");
    }

    // Existing Weather state must be restored exactly rather than removed.
    SceneService existingScenes;
    const auto weatherEntity = wi::ecs::CreateEntity();
    existingScenes.GetScene().names.Create(weatherEntity) = "Environment";
    auto& existingWeather = existingScenes.GetScene().weathers.Create(weatherEntity);
    existingWeather = existingScenes.GetScene().weather;
    existingWeather.colorGradingMapName = importedSecond;
    existingScenes.GetScene().weather = existingWeather;

    CommandService existingCommands;
    if (!existingCommands.Execute(std::make_unique<SetColorGradingLutCommand>(
            existingScenes.GetScene(), root.generic_u8string(), imported)) ||
        existingScenes.GetScene().weathers[0].colorGradingMapName != imported ||
        !existingCommands.Undo() ||
        existingScenes.GetScene().weathers[0].colorGradingMapName != importedSecond)
    {
        return Fail("existing Weather LUT Undo did not restore the exact prior selection");
    }

    fs::remove_all(root, ec);
    std::cout << "Phase 5 Gate 5 LUT tests passed\n";
    return 0;
}
