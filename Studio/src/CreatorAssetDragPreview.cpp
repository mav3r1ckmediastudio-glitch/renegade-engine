#include "RenegadeStudioChrome.h"

#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::uint32_t DragPreviewLayer = 1u << 30;
    constexpr const char* DragPreviewWrapperName =
        "Reusable Asset Drag Preview";

    struct DragPreviewState
    {
        renegade::bridge::StableId assetId;
        std::string assetPath;
        wi::scene::Scene* scene = nullptr;
        wi::ecs::Entity wrapper = wi::ecs::INVALID_ENTITY;
        renegade::bridge::ModelBounds bounds;
        float scale = 1.0f;
        XMFLOAT3 position = {};
        bool readyToCommit = false;
        renegade::bridge::PreparedReusableModelPlacement prepared;
        std::vector<wi::ecs::Entity> createdEntities;
    };

    DragPreviewState preview;
    std::size_t pendingPreviewCleanupCount = 0;
    bool dragCancelledUntilRelease = false;

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        return scene.transforms.Contains(entity) ||
            scene.layers.Contains(entity) ||
            scene.names.Contains(entity) ||
            scene.hierarchy.Contains(entity) ||
            scene.objects.Contains(entity) ||
            scene.meshes.Contains(entity) ||
            scene.materials.Contains(entity) ||
            scene.armatures.Contains(entity) ||
            scene.animations.Contains(entity);
    }

    bool IsLivePreviewWrapper(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity wrapper) noexcept
    {
        if (wrapper == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(wrapper))
        {
            return false;
        }
        const auto* name = scene.names.GetComponent(wrapper);
        return name != nullptr && name->name == DragPreviewWrapperName;
    }

    void HideAndDeferPreviewRemoval(
        wi::scene::Scene* scene,
        const wi::ecs::Entity wrapper,
        std::vector<wi::ecs::Entity> createdEntities)
    {
        if (scene == nullptr || !IsLivePreviewWrapper(*scene, wrapper))
            return;

        if (auto* transform = scene->transforms.GetComponent(wrapper))
        {
            transform->scale_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
            transform->SetDirty();
            transform->UpdateTransform();
        }
        for (const wi::ecs::Entity entity : createdEntities)
        {
            if (auto* layer = scene->layers.GetComponent(entity))
                layer->layerMask = 0u;
        }

        auto* session = renegade::bridge::StudioSession::Current();
        ++pendingPreviewCleanupCount;
        wi::eventhandler::Subscribe_Once(
            wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [session, scene, wrapper,
                createdEntities = std::move(createdEntities)](std::uint64_t)
            {
                const auto finish = []()
                {
                    if (pendingPreviewCleanupCount > 0)
                        --pendingPreviewCleanupCount;
                };

                if (session != renegade::bridge::StudioSession::Current() ||
                    session == nullptr ||
                    &session->Scenes().GetScene() != scene ||
                    !IsLivePreviewWrapper(*scene, wrapper))
                {
                    finish();
                    return;
                }

                scene->Entity_Remove(wrapper, true);
                for (const wi::ecs::Entity entity : createdEntities)
                {
                    if (entity == wrapper ||
                        scene->hierarchy.Contains(entity) ||
                        !EntityExists(*scene, entity))
                    {
                        continue;
                    }

                    scene->names.Remove(entity);
                    scene->layers.Remove(entity);
                    scene->transforms.Remove(entity);
                    scene->objects.Remove(entity);
                    scene->meshes.Remove(entity);
                    scene->materials.Remove(entity);
                    scene->armatures.Remove(entity);
                    scene->animations.Remove(entity);
                }
                finish();
            });
    }

    std::vector<wi::ecs::Entity> CollectEntities(
        const wi::scene::Scene& scene)
    {
        std::unordered_set<wi::ecs::Entity> entities;
        const auto collect = [&entities](const auto& components)
        {
            for (std::size_t index = 0; index < components.GetCount(); ++index)
                entities.insert(components.GetEntity(index));
        };
        collect(scene.transforms);
        collect(scene.layers);
        collect(scene.names);
        collect(scene.hierarchy);
        collect(scene.objects);
        collect(scene.meshes);
        collect(scene.materials);
        collect(scene.armatures);
        collect(scene.animations);
        return std::vector<wi::ecs::Entity>(entities.begin(), entities.end());
    }

    std::vector<wi::ecs::Entity> CollectNewEntities(
        const wi::scene::Scene& scene,
        const std::vector<wi::ecs::Entity>& before)
    {
        const std::unordered_set<wi::ecs::Entity> existing(
            before.begin(), before.end());
        std::vector<wi::ecs::Entity> created;
        for (const wi::ecs::Entity entity : CollectEntities(scene))
        {
            if (existing.find(entity) == existing.end())
                created.push_back(entity);
        }
        return created;
    }

    wi::allocator::shared_ptr<wi::scene::Scene> ClonePlacementScene(
        wi::scene::Scene& source,
        std::string& error)
    {
        auto clone = wi::allocator::make_shared_single<wi::scene::Scene>();
        wi::Archive archive;
        archive.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        source.componentLibrary.Serialize(archive, serializer);
        wi::jobsystem::Wait(serializer.ctx);
        archive.SetReadModeAndResetPos(true);
        clone->componentLibrary.Serialize(archive, serializer);
        wi::jobsystem::Wait(serializer.ctx);
        if (clone->transforms.GetCount() == 0 ||
            clone->objects.GetCount() == 0 ||
            clone->meshes.GetCount() == 0)
        {
            error =
                "The reusable asset could not be cloned for cursor preview.";
            return {};
        }
        error.clear();
        return clone;
    }

    wi::ecs::Entity FindRootEntity(
        const wi::scene::Scene& scene,
        const std::vector<wi::ecs::Entity>& created)
    {
        for (const wi::ecs::Entity entity : created)
        {
            if (!scene.transforms.Contains(entity))
                continue;
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY)
            {
                return entity;
            }
        }
        return wi::ecs::INVALID_ENTITY;
    }

    bool PointerInsideViewport(
        const renegade::studio::CreatorAssetStudioChrome& chrome,
        const XMFLOAT4& pointer) noexcept
    {
        const XMFLOAT4 bounds = chrome.ViewportBounds();
        return pointer.x >= bounds.x && pointer.x <= bounds.x + bounds.z &&
            pointer.y >= bounds.y && pointer.y <= bounds.y + bounds.w;
    }

    bool ResolveSurface(
        const wi::Canvas& canvas,
        const wi::scene::CameraComponent& camera,
        const XMFLOAT4& pointer,
        wi::scene::Scene& scene,
        XMFLOAT3& surface)
    {
        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            canvas,
            camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN,
            ~DragPreviewLayer,
            scene);
        if (picked.entity != wi::ecs::INVALID_ENTITY)
        {
            surface = picked.position;
            return true;
        }

        if (std::abs(ray.direction.y) <= 0.0001f)
            return false;
        const float distance = -ray.origin.y / ray.direction.y;
        if (distance < ray.TMin || distance > ray.TMax)
            return false;
        surface = XMFLOAT3(
            ray.origin.x + ray.direction.x * distance,
            0.0f,
            ray.origin.z + ray.direction.z * distance);
        return true;
    }

    bool PositionPreview(const XMFLOAT3& surface)
    {
        if (preview.scene == nullptr ||
            !IsLivePreviewWrapper(*preview.scene, preview.wrapper))
        {
            return false;
        }
        auto* transform =
            preview.scene->transforms.GetComponent(preview.wrapper);
        if (transform == nullptr)
            return false;

        preview.position = surface;
        if (preview.bounds.valid)
        {
            preview.position.y =
                renegade::bridge::ImportService::ResolveGroundedPlacementY(
                    surface.y,
                    preview.bounds,
                    preview.scale);
        }
        transform->translation_local = preview.position;
        transform->SetDirty();
        transform->UpdateTransform();
        return true;
    }

    bool CreatePreview(
        renegade::bridge::StudioSession& session,
        const renegade::bridge::StableId& assetId,
        const std::string& assetPath,
        const XMFLOAT3& surface,
        std::string& error)
    {
        const auto& project = session.Projects().CurrentProject();
        renegade::bridge::CreatorAssetWorkflowService workflow;
        auto prepared = workflow.PrepareModelPlacement(
            project.rootPath,
            project.projectId,
            assetId);
        if (!prepared.IsReady() || prepared.PeekMutableScene() == nullptr)
        {
            error = prepared.Result().error.empty()
                ? "The reusable asset could not be prepared for cursor preview."
                : prepared.Result().error;
            return false;
        }

        wi::scene::Scene* production = prepared.PeekMutableScene();
        const float scale =
            renegade::bridge::HasCreatorAuthoredTransform(*production)
            ? 1.0f
            : renegade::bridge::ImportService::ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Automatic,
                *production);
        const auto bounds =
            renegade::bridge::ImportService::MeasureModelBounds(*production);

        auto ghost = ClonePlacementScene(*production, error);
        if (!ghost.IsValid())
            return false;
        const auto restored =
            renegade::bridge::RestoreMaterialTextureBindings(
                *ghost,
                project.rootPath,
                project.projectId);
        if (!restored.succeeded)
        {
            error =
                "Cursor preview material textures could not be restored: " +
                restored.error;
            return false;
        }

        auto& scene = session.Scenes().GetScene();
        const auto before = CollectEntities(scene);
        scene.Merge(*ghost);
        auto created = CollectNewEntities(scene, before);
        const wi::ecs::Entity root = FindRootEntity(scene, created);
        if (root == wi::ecs::INVALID_ENTITY)
        {
            for (const wi::ecs::Entity entity : created)
            {
                if (!scene.hierarchy.Contains(entity))
                    scene.Entity_Remove(entity, true);
            }
            error = "The reusable asset cursor preview has no transform root.";
            return false;
        }

        const wi::ecs::Entity wrapper =
            scene.Entity_CreateTransform(DragPreviewWrapperName);
        auto* wrapperTransform = scene.transforms.GetComponent(wrapper);
        if (wrapperTransform == nullptr)
        {
            scene.Entity_Remove(root, true);
            error = "The reusable asset cursor preview root could not be created.";
            return false;
        }

        scene.Component_Attach(root, wrapper, true);
        created.push_back(wrapper);
        for (const wi::ecs::Entity entity : created)
        {
            if (!scene.layers.Contains(entity))
                scene.layers.Create(entity);
            if (auto* layer = scene.layers.GetComponent(entity))
                layer->layerMask = DragPreviewLayer;
        }

        preview = {};
        preview.assetId = assetId;
        preview.assetPath = fs::u8path(assetPath)
            .lexically_normal().generic_u8string();
        preview.scene = &scene;
        preview.wrapper = wrapper;
        preview.bounds = bounds;
        preview.scale = scale;
        preview.prepared = std::move(prepared);
        preview.createdEntities = std::move(created);
        preview.readyToCommit = false;
        if (!PositionPreview(surface))
        {
            auto* previewScene = preview.scene;
            const auto previewWrapper = preview.wrapper;
            auto previewEntities = std::move(preview.createdEntities);
            preview = {};
            HideAndDeferPreviewRemoval(
                previewScene, previewWrapper, std::move(previewEntities));
            error = "The reusable asset cursor preview could not be positioned.";
            return false;
        }

        error.clear();
        return true;
    }
}

namespace renegade::studio::detail
{
    void ClearCreatorAssetDragPreview()
    {
        if (preview.scene == nullptr ||
            preview.wrapper == wi::ecs::INVALID_ENTITY)
        {
            preview = {};
            return;
        }

        auto* scene = preview.scene;
        const auto wrapper = preview.wrapper;
        auto created = std::move(preview.createdEntities);
        preview = {};
        HideAndDeferPreviewRemoval(scene, wrapper, std::move(created));
    }

    bool ConsumeCreatorAssetDragPreview(
        const bridge::StableId& assetId,
        bridge::PreparedReusableModelPlacement& prepared,
        XMFLOAT3& position,
        float& scale)
    {
        if (!bridge::IsValidStableId(assetId) ||
            preview.assetId != assetId ||
            !preview.readyToCommit ||
            !preview.prepared.IsReady() ||
            preview.scene == nullptr ||
            !IsLivePreviewWrapper(*preview.scene, preview.wrapper))
        {
            return false;
        }

        prepared = std::move(preview.prepared);
        position = preview.position;
        scale = preview.scale;
        auto* scene = preview.scene;
        const auto wrapper = preview.wrapper;
        auto created = std::move(preview.createdEntities);
        preview = {};
        dragCancelledUntilRelease = false;
        HideAndDeferPreviewRemoval(scene, wrapper, std::move(created));
        return prepared.IsReady();
    }

    bool CreatorAssetDragPreviewBlocksSave() noexcept
    {
        return pendingPreviewCleanupCount != 0 ||
            preview.wrapper != wi::ecs::INVALID_ENTITY ||
            preview.prepared.IsReady();
    }

    void UpdateCreatorAssetDragPreview(
        const wi::Canvas& canvas,
        const wi::scene::CameraComponent& camera)
    {
        auto* chrome = CreatorAssetStudioChrome::Current();
        auto* session = bridge::StudioSession::Current();
        if (chrome == nullptr || session == nullptr ||
            !session->Projects().HasProject())
        {
            dragCancelledUntilRelease = false;
            ClearCreatorAssetDragPreview();
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            if (chrome->AssetBrowserDragCandidate() ||
                chrome->AssetBrowserDragging() ||
                preview.wrapper != wi::ecs::INVALID_ENTITY)
            {
                dragCancelledUntilRelease = true;
                ClearCreatorAssetDragPreview();
                chrome->SetStatusText("ASSET DRAG // CANCELLED");
            }
            return;
        }

        // Renegade chrome clears its drag flags and emits the drop callback
        // before this updater runs. Resolve the exact release-frame placement
        // first, while the retained prepared product and cursor ghost still
        // exist, then let Studio consume that same authoritative solution.
        if (wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            if (dragCancelledUntilRelease)
            {
                dragCancelledUntilRelease = false;
                ClearCreatorAssetDragPreview();
                return;
            }

            if (preview.wrapper == wi::ecs::INVALID_ENTITY ||
                preview.scene != &session->Scenes().GetScene() ||
                !PointerInsideViewport(*chrome, pointer))
            {
                ClearCreatorAssetDragPreview();
                return;
            }

            XMFLOAT3 surface;
            if (!ResolveSurface(
                    canvas,
                    camera,
                    pointer,
                    session->Scenes().GetScene(),
                    surface) ||
                !PositionPreview(surface))
            {
                ClearCreatorAssetDragPreview();
                return;
            }

            preview.readyToCommit = true;
            chrome->SetStatusText(
                "ASSET DRAG // RELEASED // COMMITTING EXACT PREVIEW");
            return;
        }

        if (dragCancelledUntilRelease)
            return;

        const bool dragging =
            chrome->AssetBrowserDragCandidate() &&
            chrome->AssetBrowserDragging() &&
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        if (!dragging)
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        const auto& assetId = chrome->SelectedCreatorAssetId();
        const std::string assetPath = chrome->AssetBrowserDragPath();
        if (!bridge::IsValidStableId(assetId) || assetPath.empty())
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        auto& scene = session->Scenes().GetScene();
        if (!PointerInsideViewport(*chrome, pointer))
        {
            // Asset drags originate in the drawer, outside the scene viewport.
            // Leaving the viewport only becomes cancellation after the ghost
            // has actually entered the scene once.
            if (preview.wrapper != wi::ecs::INVALID_ENTITY)
            {
                dragCancelledUntilRelease = true;
                ClearCreatorAssetDragPreview();
                chrome->SetStatusText(
                    "ASSET DRAG // CANCELLED // LEFT SCENE VIEWPORT");
            }
            return;
        }

        XMFLOAT3 surface;
        if (!ResolveSurface(canvas, camera, pointer, scene, surface))
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return;
        }

        const std::string normalized =
            fs::u8path(assetPath).lexically_normal().generic_u8string();
        if (preview.wrapper == wi::ecs::INVALID_ENTITY ||
            preview.assetId != assetId ||
            preview.assetPath != normalized ||
            preview.scene != &scene ||
            !IsLivePreviewWrapper(scene, preview.wrapper))
        {
            ClearCreatorAssetDragPreview();
            std::string error;
            if (!CreatePreview(session, assetId, normalized, surface, error))
            {
                chrome->SetStatusText(
                    "ASSET DRAG // PREVIEW FAILED // " + error);
                wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
                return;
            }
        }
        else
        {
            preview.readyToCommit = false;
            if (!PositionPreview(surface))
            {
                ClearCreatorAssetDragPreview();
                wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
                return;
            }
        }

        chrome->SetStatusText(
            "ASSET DRAG // TEXTURED CURSOR GHOST // RELEASE TO PLACE");
        wi::input::SetCursor(wi::input::CURSOR_HAND);
    }
}
