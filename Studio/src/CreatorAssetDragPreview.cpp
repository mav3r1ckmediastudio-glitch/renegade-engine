#include "RenegadeStudioChrome.h"

#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/StudioSession.h"

#include <Utility/lodepng.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <unordered_set>
#include <vector>

namespace renegade::studio::detail
{
    namespace
    {
        namespace fs = std::filesystem;
        constexpr std::uint32_t DragPreviewLayer = 1u << 31;
        constexpr float BottomChromeHeight = 60.0f;
        constexpr unsigned ThumbnailSize = 512;

        struct DragPreviewState
        {
            bridge::StableId assetId;
            std::string assetPath;
            wi::scene::Scene* scene = nullptr;
            wi::ecs::Entity wrapper = wi::ecs::INVALID_ENTITY;
            bridge::ModelBounds bounds;
            float scale = 1.0f;
            std::vector<wi::ecs::Entity> createdEntities;
        };

        DragPreviewState preview;
        fs::path lastNormalizedThumbnail;
        fs::file_time_type lastNormalizedThumbnailWriteTime{};

        bool IsInside(
            const XMFLOAT4& bounds,
            const XMFLOAT4& pointer) noexcept
        {
            return pointer.x >= bounds.x && pointer.x < bounds.z &&
                pointer.y >= bounds.y && pointer.y < bounds.w;
        }

        void RemovePreview()
        {
            if (preview.scene != nullptr)
            {
                if (preview.wrapper != wi::ecs::INVALID_ENTITY &&
                    preview.scene->transforms.GetComponent(preview.wrapper) != nullptr)
                {
                    preview.scene->Entity_Remove(preview.wrapper, true);
                }

                wi::unordered_set<wi::ecs::Entity> remaining;
                preview.scene->FindAllEntities(remaining);
                for (const wi::ecs::Entity entity : preview.createdEntities)
                {
                    if (remaining.count(entity) != 0)
                        preview.scene->Entity_Remove(entity, false);
                }
            }
            preview = {};
        }

        wi::Canvas ReconstructCanvas(const CreatorAssetStudioChrome& chrome)
        {
            const XMFLOAT4 viewport = chrome.ViewportBounds();
            const float logicalWidth = viewport.z + chrome.InspectorWidth();
            const float logicalHeight = viewport.w + BottomChromeHeight +
                (chrome.ActiveBottomTab() >= 0 ? chrome.DrawerHeight() : 0.0f);
            wi::Canvas canvas;
            canvas.init(
                static_cast<std::uint32_t>(std::max(1.0f, logicalWidth)),
                static_cast<std::uint32_t>(std::max(1.0f, logicalHeight)),
                96.0f);
            return canvas;
        }

        bool ResolveSurfacePosition(
            const CreatorAssetStudioChrome& chrome,
            const XMFLOAT4& pointer,
            wi::scene::Scene& scene,
            XMFLOAT3& surfacePosition)
        {
            const wi::Canvas canvas = ReconstructCanvas(chrome);
            const auto ray = wi::renderer::GetPickRay(
                static_cast<long>(pointer.x),
                static_cast<long>(pointer.y),
                canvas,
                wi::scene::GetCamera());
            const auto picked = wi::scene::Pick(
                ray,
                wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN,
                ~DragPreviewLayer,
                scene);
            if (picked.entity != wi::ecs::INVALID_ENTITY)
            {
                surfacePosition = picked.position;
                return true;
            }

            // Match Studio's accepted editor-ground fallback exactly. Empty
            // scenes still provide an intentional Y=0 placement surface.
            if (std::abs(ray.direction.y) <= 0.0001f)
                return false;
            const float distance = -ray.origin.y / ray.direction.y;
            if (distance <= 0.0f)
                return false;
            surfacePosition = XMFLOAT3(
                ray.origin.x + ray.direction.x * distance,
                0.0f,
                ray.origin.z + ray.direction.z * distance);
            return true;
        }

        void MarkPreviewLayer(
            wi::scene::Scene& scene,
            const wi::ecs::Entity wrapper)
        {
            for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.objects.GetEntity(index);
                if (entity != wrapper && !scene.Entity_IsDescendant(entity, wrapper))
                    continue;
                auto* layer = scene.layers.GetComponent(entity);
                if (layer == nullptr)
                    layer = &scene.layers.Create(entity);
                layer->layerMask = DragPreviewLayer;
                layer->propagationMask = DragPreviewLayer;
                if (index < scene.aabb_objects.size())
                    scene.aabb_objects[index].layerMask = DragPreviewLayer;
            }
        }

        bool CreatePreview(
            CreatorAssetStudioChrome& chrome,
            bridge::StudioSession& session,
            const bridge::StableId& assetId,
            const std::string& assetPath,
            const XMFLOAT3& surfacePosition)
        {
            const auto& project = session.Projects().CurrentProject();
            bridge::CreatorAssetWorkflowService workflow;
            auto prepared = workflow.PrepareModelPlacement(
                project.rootPath,
                project.projectId,
                assetId);
            if (!prepared.IsReady() || prepared.PeekScene() == nullptr)
                return false;

            const wi::scene::Scene* preparedScene = prepared.PeekScene();
            const float scale = bridge::HasCreatorAuthoredTransform(*preparedScene)
                ? 1.0f
                : bridge::ImportService::ResolveScaleFactor(
                    bridge::ModelScaleMode::Automatic,
                    *preparedScene);
            const bridge::ModelBounds bounds =
                bridge::ImportService::MeasureModelBounds(*preparedScene);
            if (!bounds.valid)
                return false;

            XMFLOAT3 position = surfacePosition;
            position.y = bridge::ImportService::ResolveGroundedPlacementY(
                surfacePosition.y,
                bounds,
                scale);

            auto& scene = session.Scenes().GetScene();
            wi::unordered_set<wi::ecs::Entity> before;
            scene.FindAllEntities(before);

            bridge::PlaceReusableModelCommand command(
                scene,
                prepared.ReleaseScene(),
                assetId,
                position,
                scale);
            if (!command.Execute())
                return false;

            preview.assetId = assetId;
            preview.assetPath = assetPath;
            preview.scene = &scene;
            preview.wrapper = command.PlacedEntity();
            preview.bounds = bounds;
            preview.scale = scale;

            wi::unordered_set<wi::ecs::Entity> after;
            scene.FindAllEntities(after);
            preview.createdEntities.reserve(after.size());
            for (const wi::ecs::Entity entity : after)
            {
                if (before.count(entity) == 0)
                    preview.createdEntities.push_back(entity);
            }
            MarkPreviewLayer(scene, preview.wrapper);
            chrome.SetStatusText(
                "PLACE ASSET // PREVIEW // RELEASE LMB TO COMMIT");
            return true;
        }

        bool ReadFileBytes(const fs::path& path, std::vector<unsigned char>& bytes)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return false;
            bytes.assign(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
            return input.eof() || static_cast<bool>(input);
        }

        bool WriteFileBytes(const fs::path& path, const std::vector<unsigned char>& bytes)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
                return false;
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            return static_cast<bool>(output);
        }

        void NormalizeSelectedThumbnail(
            CreatorAssetStudioChrome& chrome,
            bridge::StudioSession& session)
        {
            const std::string& relativePath = chrome.SelectedCreatorAssetPath();
            if (relativePath.empty())
                return;

            fs::path thumbnail =
                fs::u8path(session.Projects().CurrentProject().rootPath) /
                fs::u8path(relativePath);
            thumbnail.replace_extension(".thumbnail.png");
            std::error_code ec;
            if (!fs::is_regular_file(thumbnail, ec) || ec)
                return;
            const auto writeTime = fs::last_write_time(thumbnail, ec);
            if (ec || (thumbnail == lastNormalizedThumbnail &&
                       writeTime == lastNormalizedThumbnailWriteTime))
            {
                return;
            }

            std::vector<unsigned char> encoded;
            if (!ReadFileBytes(thumbnail, encoded))
                return;
            std::vector<unsigned char> rgba;
            unsigned width = 0;
            unsigned height = 0;
            if (lodepng::decode(rgba, width, height, encoded) != 0 ||
                width == 0 || height == 0)
            {
                return;
            }

            // The importer intentionally places the 1.82 m scale reference to
            // the left of the centered asset. Convert the full viewport proof
            // capture into a consistent square asset card by cropping around
            // the model side of the frame, then downsample to a deterministic
            // 512x512 sidecar. This keeps Retake Thumbnail while preventing the
            // human/ruler/editor-stage reference from appearing in the browser.
            if (width != ThumbnailSize || height != ThumbnailSize)
            {
                const unsigned crop = std::min(width, height);
                const float desiredCenterX = static_cast<float>(width) * 0.60f;
                const int maxX = static_cast<int>(width - crop);
                const int maxY = static_cast<int>(height - crop);
                const int cropX = std::clamp(
                    static_cast<int>(std::lround(desiredCenterX - crop * 0.5f)),
                    0,
                    maxX);
                const int cropY = std::max(0, maxY / 2);

                std::vector<unsigned char> normalized(
                    static_cast<std::size_t>(ThumbnailSize) *
                    ThumbnailSize * 4u);
                for (unsigned y = 0; y < ThumbnailSize; ++y)
                {
                    const unsigned sourceY = static_cast<unsigned>(cropY) +
                        std::min(crop - 1u,
                            static_cast<unsigned>(
                                (static_cast<std::uint64_t>(y) * crop) /
                                ThumbnailSize));
                    for (unsigned x = 0; x < ThumbnailSize; ++x)
                    {
                        const unsigned sourceX = static_cast<unsigned>(cropX) +
                            std::min(crop - 1u,
                                static_cast<unsigned>(
                                    (static_cast<std::uint64_t>(x) * crop) /
                                    ThumbnailSize));
                        const std::size_t src =
                            (static_cast<std::size_t>(sourceY) * width + sourceX) * 4u;
                        const std::size_t dst =
                            (static_cast<std::size_t>(y) * ThumbnailSize + x) * 4u;
                        std::copy_n(rgba.data() + src, 4u, normalized.data() + dst);
                    }
                }

                std::vector<unsigned char> output;
                if (lodepng::encode(
                        output,
                        normalized,
                        ThumbnailSize,
                        ThumbnailSize) != 0 ||
                    !WriteFileBytes(thumbnail, output))
                {
                    return;
                }

                // Asset cards can already hold the filename-keyed Wicked
                // resource by the time the post-import reveal selects it.
                // Mark that shared resource stale and refresh it in place so
                // the normalized card becomes visible without reopening Studio.
                auto resource = wi::resourcemanager::Load(
                    thumbnail.generic_u8string());
                if (resource.IsValid())
                {
                    resource.SetOutdated();
                    wi::resourcemanager::ReloadOutdatedResources();
                }
            }

            lastNormalizedThumbnail = thumbnail;
            lastNormalizedThumbnailWriteTime =
                fs::last_write_time(thumbnail, ec);
            if (ec)
                lastNormalizedThumbnailWriteTime = {};
        }

        void UpdatePreview()
        {
            CreatorAssetStudioChrome* chrome = CreatorAssetStudioChrome::Current();
            bridge::StudioSession* session = bridge::StudioSession::Current();
            if (chrome == nullptr || session == nullptr ||
                !session->Projects().HasProject())
            {
                RemovePreview();
                return;
            }

            NormalizeSelectedThumbnail(*chrome, *session);

            const bool dragging = chrome->AssetBrowserDragCandidate() &&
                chrome->AssetBrowserDragging() &&
                wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
            if (!dragging)
            {
                RemovePreview();
                return;
            }

            const std::string& dragPath = chrome->AssetBrowserDragPath();
            const bridge::StableId& assetId = chrome->SelectedCreatorAssetId();
            if (!bridge::IsValidStableId(assetId) || dragPath.empty() ||
                dragPath != chrome->SelectedCreatorAssetPath())
            {
                RemovePreview();
                return;
            }

            const XMFLOAT4 pointer = wi::input::GetPointer();
            if (!IsInside(chrome->ViewportBounds(), pointer))
            {
                RemovePreview();
                return;
            }

            auto& scene = session->Scenes().GetScene();
            XMFLOAT3 surfacePosition;
            if (!ResolveSurfacePosition(*chrome, pointer, scene, surfacePosition))
            {
                RemovePreview();
                return;
            }

            if (preview.wrapper == wi::ecs::INVALID_ENTITY ||
                preview.scene != &scene || preview.assetId != assetId ||
                preview.assetPath != dragPath)
            {
                RemovePreview();
                CreatePreview(*chrome, *session, assetId, dragPath, surfacePosition);
                return;
            }

            auto* transform = scene.transforms.GetComponent(preview.wrapper);
            if (transform == nullptr)
            {
                RemovePreview();
                return;
            }
            XMFLOAT3 position = surfacePosition;
            position.y = bridge::ImportService::ResolveGroundedPlacementY(
                surfacePosition.y,
                preview.bounds,
                preview.scale);
            transform->translation_local = position;
            transform->SetDirty();
        }
    }

    void EnsureCreatorAssetDragPreviewInstalled()
    {
        static const wi::eventhandler::Handle handle =
            wi::eventhandler::Subscribe(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [](const std::uint64_t)
                {
                    UpdatePreview();
                });
        (void)handle;
    }
}
