from pathlib import Path
import re


def load(path):
    return Path(path).read_text(encoding="utf-8")


def save(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path, old, new):
    text = load(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one target, found {count}")
    save(path, text.replace(old, new, 1))


def regex_once(path, pattern, replacement):
    text = load(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{path}: expected one regex target, found {count}")
    save(path, updated)


app = "Studio/src/StudioApplication.cpp"
replace_once(app, "#include <cmath>\n", "#include <cmath>\n#include <cstring>\n")

pre_state = r'''
    struct CreatorThumbnailWeatherSnapshot
    {
        bool valid = false;
        std::uint32_t flags = 0;
        XMFLOAT3 horizon = {};
        XMFLOAT3 zenith = {};
        float skyExposure = 1.0f;
        float fogDensity = 0.0f;
        float stars = 0.0f;
        std::string skyMapName;
        wi::Resource skyMap;
    };

    CreatorThumbnailWeatherSnapshot CaptureThumbnailWeather(
        const wi::scene::WeatherComponent& weather)
    {
        CreatorThumbnailWeatherSnapshot snapshot;
        snapshot.valid = true;
        snapshot.flags = weather._flags;
        snapshot.horizon = weather.horizon;
        snapshot.zenith = weather.zenith;
        snapshot.skyExposure = weather.skyExposure;
        snapshot.fogDensity = weather.fogDensity;
        snapshot.stars = weather.stars;
        snapshot.skyMapName = weather.skyMapName;
        snapshot.skyMap = weather.skyMap;
        return snapshot;
    }

    void ApplyNeutralThumbnailWeather(
        wi::scene::WeatherComponent& weather)
    {
        weather.SetRealisticSky(false);
        weather.SetVolumetricClouds(false);
        weather.SetHeightFog(false);
        weather.SetOverrideFogColor(false);
        weather.horizon = XMFLOAT3(0.065f, 0.07f, 0.075f);
        weather.zenith = XMFLOAT3(0.065f, 0.07f, 0.075f);
        weather.skyExposure = 1.0f;
        weather.fogDensity = 0.0f;
        weather.stars = 0.0f;
        weather.skyMapName.clear();
        weather.skyMap = {};
    }

    void RestoreThumbnailWeather(
        wi::scene::WeatherComponent& weather,
        const CreatorThumbnailWeatherSnapshot& snapshot)
    {
        if (!snapshot.valid)
            return;
        weather._flags = snapshot.flags;
        weather.horizon = snapshot.horizon;
        weather.zenith = snapshot.zenith;
        weather.skyExposure = snapshot.skyExposure;
        weather.fogDensity = snapshot.fogDensity;
        weather.stars = snapshot.stars;
        weather.skyMapName = snapshot.skyMapName;
        weather.skyMap = snapshot.skyMap;
    }

    bool SaveCreatorSquareThumbnail(
        const wi::graphics::Texture& texture,
        const std::string& path)
    {
        if (!texture.IsValid())
            return false;

        auto desc = texture.GetDesc();
        if (desc.width == 0 || desc.height == 0 ||
            desc.depth != 1 || desc.array_size != 1 ||
            wi::graphics::GetFormatPlaneCount(desc.format) != 1 ||
            wi::graphics::GetFormatBlockSize(desc.format) != 1)
        {
            return false;
        }

        const std::uint32_t stride =
            wi::graphics::GetFormatStride(desc.format);
        if (stride == 0)
            return false;

        wi::vector<std::uint8_t> pixels;
        if (!wi::helper::saveTextureToMemory(texture, pixels))
            return false;

        const std::uint32_t side = std::min(desc.width, desc.height);
        const std::uint32_t offsetX = (desc.width - side) / 2;
        const std::uint32_t offsetY = (desc.height - side) / 2;
        const std::size_t sourceRow =
            static_cast<std::size_t>(desc.width) * stride;
        const std::size_t squareRow =
            static_cast<std::size_t>(side) * stride;
        const std::size_t required =
            sourceRow * static_cast<std::size_t>(desc.height);
        if (pixels.size() < required)
            return false;

        wi::vector<std::uint8_t> square(
            squareRow * static_cast<std::size_t>(side));
        for (std::uint32_t row = 0; row < side; ++row)
        {
            const std::uint8_t* source =
                pixels.data() +
                (static_cast<std::size_t>(row + offsetY) * sourceRow) +
                (static_cast<std::size_t>(offsetX) * stride);
            std::uint8_t* destination =
                square.data() +
                static_cast<std::size_t>(row) * squareRow;
            std::memcpy(destination, source, squareRow);
        }

        desc.width = side;
        desc.height = side;
        desc.depth = 1;
        desc.array_size = 1;
        desc.mip_levels = 1;
        return wi::helper::saveTextureToFile(square, desc, path);
    }

    void ApplyCreatorImportPreviewLighting();
'''
replace_once(
    app,
    "    struct CreatorModelImportWorkspaceState\n",
    pre_state + "    struct CreatorModelImportWorkspaceState\n",
)

replace_once(
    app,
    """        std::string thumbnailCapturePath;
        bool thumbnailCapturePending = false;
        renegade::bridge::PreparedModelImport preparedForCommit;
""",
    """        std::string thumbnailCapturePath;
        bool thumbnailCapturePending = false;
        bool thumbnailPresentationOverridden = false;
        float thumbnailRestoreLightIntensity = 4.0f;
        float thumbnailRestoreLightAzimuth = -35.0f;
        float thumbnailRestoreLightElevation = 35.0f;
        float thumbnailRestoreAmbientBrightness = 0.35f;
        CreatorThumbnailWeatherSnapshot thumbnailSceneWeatherBefore;
        CreatorThumbnailWeatherSnapshot thumbnailEntityWeatherBefore;
        renegade::bridge::PreparedModelImport preparedForCommit;
""",
)

post_state = r'''

    void BeginCreatorThumbnailPresentation()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.active)
            return;

        creatorModelImporter.thumbnailRestoreLightIntensity =
            creatorModelImporter.lightIntensity;
        creatorModelImporter.thumbnailRestoreLightAzimuth =
            creatorModelImporter.lightAzimuth;
        creatorModelImporter.thumbnailRestoreLightElevation =
            creatorModelImporter.lightElevation;
        creatorModelImporter.thumbnailRestoreAmbientBrightness =
            creatorModelImporter.ambientBrightness;

        auto& scene = session->Scenes().GetScene();
        creatorModelImporter.thumbnailSceneWeatherBefore =
            CaptureThumbnailWeather(scene.weather);
        creatorModelImporter.thumbnailEntityWeatherBefore = {};
        ApplyNeutralThumbnailWeather(scene.weather);
        if (auto* weather =
                scene.weathers.GetComponent(creatorModelImporter.weatherEntity))
        {
            creatorModelImporter.thumbnailEntityWeatherBefore =
                CaptureThumbnailWeather(*weather);
            ApplyNeutralThumbnailWeather(*weather);
        }

        creatorModelImporter.lightIntensity = 4.0f;
        creatorModelImporter.lightAzimuth = -35.0f;
        creatorModelImporter.lightElevation = 35.0f;
        creatorModelImporter.ambientBrightness = 0.35f;
        creatorModelImporter.thumbnailPresentationOverridden = true;
        ApplyCreatorImportPreviewLighting();
    }

    void RestoreCreatorThumbnailPresentation()
    {
        if (!creatorModelImporter.thumbnailPresentationOverridden)
            return;

        auto* session = renegade::bridge::StudioSession::Current();
        if (session != nullptr)
        {
            auto& scene = session->Scenes().GetScene();
            RestoreThumbnailWeather(
                scene.weather,
                creatorModelImporter.thumbnailSceneWeatherBefore);
            if (auto* weather =
                    scene.weathers.GetComponent(
                        creatorModelImporter.weatherEntity))
            {
                RestoreThumbnailWeather(
                    *weather,
                    creatorModelImporter.thumbnailEntityWeatherBefore);
            }
        }

        creatorModelImporter.lightIntensity =
            creatorModelImporter.thumbnailRestoreLightIntensity;
        creatorModelImporter.lightAzimuth =
            creatorModelImporter.thumbnailRestoreLightAzimuth;
        creatorModelImporter.lightElevation =
            creatorModelImporter.thumbnailRestoreLightElevation;
        creatorModelImporter.ambientBrightness =
            creatorModelImporter.thumbnailRestoreAmbientBrightness;
        creatorModelImporter.thumbnailPresentationOverridden = false;
        ApplyCreatorImportPreviewLighting();
    }
'''
replace_once(
    app,
    "    CreatorModelImportWorkspaceState creatorModelImporter;\n",
    "    CreatorModelImportWorkspaceState creatorModelImporter;\n" + post_state,
)

replace_once(
    app,
    """            const bool captured = wi::helper::saveTextureToFile(
                GetRenderResult3D(),
                creatorModelImporter.thumbnailCapturePath);
            creatorModelImporter.thumbnailCapturePending = false;
            if (captured)
""",
    """            const bool captured = SaveCreatorSquareThumbnail(
                GetRenderResult3D(),
                creatorModelImporter.thumbnailCapturePath);
            creatorModelImporter.thumbnailCapturePending = false;
            RestoreCreatorThumbnailPresentation();
            if (captured)
""",
)
replace_once(
    app,
    "                    \"THUMBNAIL CAPTURED // CLEAN ASSET FRAME\");\n",
    "                    \"THUMBNAIL CAPTURED // SQUARE AUTO-FRAMED ASSET\");\n",
)

capture = r'''    void StudioRenderPath::CaptureCreatorImportThumbnail()
    {
        if (session_ == nullptr || !creatorModelImporter.active ||
            creatorModelImporter.committing ||
            creatorModelImporter.thumbnailCapturePending ||
            !session_->Projects().HasProject())
            return;

        BeginCreatorThumbnailPresentation();
        FrameCreatorImportPreviewCamera();

        const fs::path directory =
            fs::u8path(session_->Projects().CurrentProject().rootPath) /
            "Intermediate" / "Imports";
        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec)
        {
            RestoreCreatorThumbnailPresentation();
            creatorImportThumbnailStatus.SetText(
                "THUMBNAIL FAILED // CANNOT CREATE IMPORT CACHE");
            return;
        }

        const fs::path capturePath =
            directory / ".creator-asset-thumbnail.png";
        fs::remove(capturePath, ec);
        creatorModelImporter.thumbnailCapturePath =
            capturePath.generic_u8string();
        creatorModelImporter.thumbnailCapturePending = true;
        creatorImportThumbnailStatus.SetText(
            "CAPTURING SQUARE AUTO-FRAMED ASSET...");
        importScaleApplyButton_.SetEnabled(false);
    }

'''
regex_once(
    app,
    r"    void StudioRenderPath::CaptureCreatorImportThumbnail\(\)\n    \{.*?\n    \}\n\n    void StudioRenderPath::ApplyImportScaleMode",
    capture + "    void StudioRenderPath::ApplyImportScaleMode",
)

text = load(app)
for required in (
    "SaveCreatorSquareThumbnail",
    "BeginCreatorThumbnailPresentation",
    "FrameCreatorImportPreviewCamera();",
    "SQUARE AUTO-FRAMED ASSET",
):
    if required not in text:
        raise RuntimeError(f"thumbnail recovery missing: {required}")

print("PR57_THUMBNAIL_PATCH_APPLIED")
