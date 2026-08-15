#include "RenegadeStudioChrome.h"

#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/StudioSession.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::uint32_t DragPreviewLayer = 1u << 31;
    constexpr const char* DragPreviewWrapperName =
        "Reusable Asset Drag Preview";
    constexpr const char* LegacyPayloadRootName =
        "Reusable Asset Payload";

    struct LayerState
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::uint32_t layerMask = ~0u;
        std::uint32_t propagationMask = ~0u;
        bool existed = false;
    };

    struct PreparationJob
    {
        wi::jobsystem::context context;
        renegade::bridge::StableId projectId;
        renegade::bridge::StableId assetId;
        std::string projectRoot;
        std::string assetPath;
        renegade::bridge::PreparedReusableModelPlacement prepared;
        renegade::bridge::ModelBounds bounds;
        float scale = 1.0f;
        std::string error;
    };

    struct DragPreviewState
    {
        renegade::bridge::StableId assetId;
        std::string assetPath;
        wi::scene::Scene* scene = nullptr;
        wi::ecs::Entity wrapper = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payloadRoot = wi::ecs::INVALID_ENTITY;
        renegade::bridge::ModelBounds bounds;
        float scale = 1.0f;
        XMFLOAT3 position = {};
        std::size_t firstMaterialIndex = 0;
        std::vector<wi::ecs::Entity> createdEntities;
        std::vector<LayerState> originalLayers;
    };

    DragPreviewState preview;
    std::shared_ptr<PreparationJob> preparation;
    std::size_t pendingPreviewCleanupCount = 0;
    bool dragCancelledUntilRelease = false;

    std::string NormalizePath(const std::string& value)
    {
        return fs::u8path(value).lexically_normal().generic_u8string();
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
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

    bool SamePreparedAsset(
        const PreparationJob& job,
        const renegade::bridge::StableId& projectId,
        const renegade::bridge::StableId& assetId,
        const std::string& assetPath) noexcept
    {
        return job.projectId == projectId &&
            job.assetId == assetId &&
            job.assetPath == assetPath;
    }

    void FillPreparationMetrics(PreparationJob& job)
    {
        if (!job.prepared.IsReady() || job.prepared.PeekScene() == nullptr)
        {
            if (job.error.empty())
            {
                job.error = job.prepared.Result().error.empty()
                    ? "The reusable asset could not be prepared for placement."
                    : job.prepared.Result().error;
            }
            return;
        }

        const auto* scene = job.prepared.PeekScene();
        job.scale = renegade::bridge::HasCreatorAuthoredTransform(*scene)
            ? 1.0f
            : renegade::bridge::ImportService::ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Automatic,
                *scene);
        job.bounds = renegade::bridge::ImportService::MeasureModelBounds(*scene);
        job.error.clear();
    }

    std::vector<wi::ecs::Entity> CollectNewEntities(
        const wi::scene::Scene& scene,
        const wi::unordered_set<wi::ecs::Entity>& before)
    {
        wi::unordered_set<wi::ecs::Entity> after;
        scene.FindAllEntities(after);
        std::vector<wi::ecs::Entity> created;
        created.reserve(after.size());
        for (const wi::ecs::Entity entity : after)
        {
            if (before.count(entity) == 0)
                created.push_back(entity);
        }
        return created;
    }

    std::vector<wi::ecs::Entity> FindRootTransforms(
        const wi::scene::Scene& scene,
        const std::vector<wi::ecs::Entity>& created)
    {
        wi::unordered_set<wi::ecs::Entity> createdSet;
        for (const auto entity : created)
            createdSet.insert(entity);

        std::vector<wi::ecs::Entity> roots;
        for (const wi::ecs::Entity entity : created)
        {
            if (!scene.transforms.Contains(entity))
                continue;
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                createdSet.count(hierarchy->parentID) == 0)
            {
                roots.push_back(entity);
            }
        }
        return roots;
    }

    wi::ecs::Entity FindCreatorPayloadRoot(
        const wi::scene::Scene& scene,
        const std::vector<wi::ecs::Entity>& created)
    {
        wi::unordered_set<wi::ecs::Entity> createdSet;
        for (const auto entity : created)
            createdSet.insert(entity);
        for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
        {
            if (scene.names[index].name !=
                renegade::bridge::CreatorAuthoredTransformRootName)
            {
                continue;
            }
            const auto entity = scene.names.GetEntity(index);
            if (createdSet.count(entity) != 0 && scene.transforms.Contains(entity))
                return entity;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    void CaptureAndSetPreviewLayers(
        wi::scene::Scene& scene,
        const wi::ecs::Entity wrapper,
        std::vector<LayerState>& states)
    {
        states.clear();
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const auto entity = scene.objects.GetEntity(index);
            if (entity != wrapper && !scene.Entity_IsDescendant(entity, wrapper))
                continue;

            LayerState state;
            state.entity = entity;
            if (const auto* layer = scene.layers.GetComponent(entity))
            {
                state.existed = true;
                state.layerMask = layer->layerMask;
                state.propagationMask = layer->propagationMask;
            }
            states.push_back(state);

            auto* layer = scene.layers.GetComponent(entity);
            if (layer == nullptr)
                layer = &scene.layers.Create(entity);
            layer->layerMask = DragPreviewLayer;
            layer->propagationMask = DragPreviewLayer;
            if (index < scene.aabb_objects.size())
                scene.aabb_objects[index].layerMask = DragPreviewLayer;
        }
    }

    void RestorePreviewLayers(
        wi::scene::Scene& scene,
        const std::vector<LayerState>& states)
    {
        for (const auto& state : states)
        {
            if (!scene.objects.Contains(state.entity))
                continue;
            auto* layer = scene.layers.GetComponent(state.entity);
            if (layer == nullptr)
                layer = &scene.layers.Create(state.entity);
            layer->layerMask = state.existed ? state.layerMask : ~0u;
            layer->propagationMask = state.existed
                ? state.propagationMask
                : ~0u;
            const auto index = scene.objects.GetIndex(state.entity);
            if (index < scene.aabb_objects.size())
                scene.aabb_objects[index].layerMask = layer->layerMask;
        }
    }

    void HideCreatedEntities(
        wi::scene::Scene& scene,
        const wi::ecs::Entity wrapper,
        const std::vector<wi::ecs::Entity>& created)
    {
        if (wrapper != wi::ecs::INVALID_ENTITY && scene.transforms.Contains(wrapper))
        {
            if (auto* transform = scene.transforms.GetComponent(wrapper))
            {
                transform->scale_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
                transform->SetDirty();
                transform->UpdateTransform();
            }
        }
        for (const auto entity : created)
        {
            if (!scene.objects.Contains(entity))
                continue;
            auto* layer = scene.layers.GetComponent(entity);
            if (layer == nullptr)
                layer = &scene.layers.Create(entity);
            layer->layerMask = 0u;
            layer->propagationMask = 0u;
            const auto index = scene.objects.GetIndex(entity);
            if (index < scene.aabb_objects.size())
                scene.aabb_objects[index].layerMask = 0u;
        }
    }

    void HideAndDeferPreviewRemoval(
        wi::scene::Scene* scene,
        const wi::ecs::Entity wrapper,
        std::vector<wi::ecs::Entity> createdEntities)
    {
        if (scene == nullptr)
            return;

        HideCreatedEntities(*scene, wrapper, createdEntities);
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

                if (session == nullptr ||
                    session != renegade::bridge::StudioSession::Current() ||
                    &session->Scenes().GetScene() != scene)
                {
                    finish();
                    return;
                }

                if (wrapper != wi::ecs::INVALID_ENTITY &&
                    EntityExists(*scene, wrapper))
                {
                    scene->Entity_Remove(wrapper, true);
                }
                wi::unordered_set<wi::ecs::Entity> remaining;
                scene->FindAllEntities(remaining);
                for (const auto entity : createdEntities)
                {
                    if (remaining.count(entity) != 0)
                        scene->Entity_Remove(entity, false);
                }
                finish();
            });
    }

    bool PointerInsideViewport(
        const renegade::studio::CreatorAssetStudioChrome& chrome,
        const XMFLOAT4& pointer) noexcept
    {
        const XMFLOAT4 bounds = chrome.ViewportBounds();
        return pointer.x >= bounds.x && pointer.x < bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.w;
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
        auto* transform = preview.scene->transforms.GetComponent(preview.wrapper);
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
        transform->scale_local = XMFLOAT3(
            preview.scale, preview.scale, preview.scale);
        transform->SetDirty();
        transform->UpdateTransform();
        return true;
    }

    bool CreatePreviewFromPrepared(
        renegade::bridge::StudioSession& session,
        const renegade::bridge::StableId& assetId,
        const std::string& assetPath,
        const XMFLOAT3& surface,
        std::string& error)
    {
        if (!preparation ||
            wi::jobsystem::IsBusy(preparation->context) ||
            preparation->assetId != assetId ||
            preparation->assetPath != assetPath ||
            !preparation->prepared.IsReady())
        {
            error = preparation && !preparation->error.empty()
                ? preparation->error
                : "The reusable asset is still preparing for placement.";
            return false;
        }

        auto preparedScene = preparation->prepared.ReleaseScene();
        if (!preparedScene.IsValid())
        {
            error = "The prepared reusable asset scene is unavailable.";
            preparation.reset();
            return false;
        }

        auto& scene = session.Scenes().GetScene();
        wi::unordered_set<wi::ecs::Entity> before;
        scene.FindAllEntities(before);
        const std::size_t firstMaterialIndex = scene.materials.GetCount();
        scene.Merge(*preparedScene);
        preparedScene.reset();

        auto created = CollectNewEntities(scene, before);
        auto roots = FindRootTransforms(scene, created);
        if (roots.empty())
        {
            HideAndDeferPreviewRemoval(&scene, wi::ecs::INVALID_ENTITY,
                std::move(created));
            error = "The prepared reusable asset contains no transform root.";
            preparation.reset();
            return false;
        }

        wi::ecs::Entity payloadRoot = FindCreatorPayloadRoot(scene, created);
        if (payloadRoot == wi::ecs::INVALID_ENTITY && roots.size() == 1)
            payloadRoot = roots.front();
        if (payloadRoot == wi::ecs::INVALID_ENTITY)
        {
            payloadRoot = scene.Entity_CreateTransform(LegacyPayloadRootName);
            created.push_back(payloadRoot);
        }
        for (const auto root : roots)
        {
            if (root != payloadRoot)
                scene.Component_Attach(root, payloadRoot, true);
        }

        const wi::ecs::Entity wrapper =
            scene.Entity_CreateTransform(DragPreviewWrapperName);
        created.push_back(wrapper);
        if (!scene.transforms.Contains(wrapper) ||
            !scene.transforms.Contains(payloadRoot))
        {
            HideAndDeferPreviewRemoval(&scene, wrapper, std::move(created));
            error = "The reusable asset cursor instance could not be created.";
            preparation.reset();
            return false;
        }
        scene.Component_Attach(payloadRoot, wrapper, true);

        preview = {};
        preview.assetId = assetId;
        preview.assetPath = assetPath;
        preview.scene = &scene;
        preview.wrapper = wrapper;
        preview.payloadRoot = payloadRoot;
        preview.bounds = preparation->bounds;
        preview.scale = preparation->scale;
        preview.firstMaterialIndex = firstMaterialIndex;
        preview.createdEntities = std::move(created);
        CaptureAndSetPreviewLayers(
            scene, wrapper, preview.originalLayers);

        wi::unordered_set<wi::ecs::Entity> createdSet;
        for (const auto entity : preview.createdEntities)
            createdSet.insert(entity);
        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            if (createdSet.count(scene.animations.GetEntity(index)) != 0)
                scene.animations[index].Play();
        }

        preparation.reset();
        if (!PositionPreview(surface))
        {
            auto* failedScene = preview.scene;
            const auto failedWrapper = preview.wrapper;
            auto failedEntities = std::move(preview.createdEntities);
            preview = {};
            HideAndDeferPreviewRemoval(
                failedScene, failedWrapper, std::move(failedEntities));
            error = "The reusable asset cursor instance could not be positioned.";
            return false;
        }

        error.clear();
        return true;
    }
}

namespace renegade::studio::detail
{
    void RequestCreatorAssetDragPreparation(
        const bridge::StableId& assetId,
        const std::string& assetPath)
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(assetId) || assetPath.empty())
        {
            return;
        }
        if (preview.assetId == assetId && preview.scene != nullptr &&
            IsLivePreviewWrapper(*preview.scene, preview.wrapper))
        {
            return;
        }

        const auto& project = session->Projects().CurrentProject();
        const std::string normalized = NormalizePath(assetPath);
        if (preparation && SamePreparedAsset(
                *preparation,
                project.projectId,
                assetId,
                normalized))
        {
            return;
        }

        auto job = std::make_shared<PreparationJob>();
        job->projectId = project.projectId;
        job->assetId = assetId;
        job->projectRoot = project.rootPath;
        job->assetPath = normalized;
        preparation = job;

        wi::jobsystem::Execute(
            job->context,
            [job](wi::jobsystem::JobArgs)
            {
                bridge::CreatorAssetWorkflowService workflow;
                job->prepared = workflow.PrepareModelPlacement(
                    job->projectRoot,
                    job->projectId,
                    job->assetId);
                FillPreparationMetrics(*job);
            });

        if (auto* chrome = CreatorAssetStudioChrome::Current())
        {
            chrome->SetStatusText(
                "ASSET // PREPARING RUNTIME INSTANCE IN BACKGROUND");
        }
    }

    void PrimeCreatorAssetDragPreparation(
        const bridge::StableId& assetId,
        const std::string& assetPath,
        bridge::PreparedReusableModelPlacement prepared)
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(assetId) || assetPath.empty())
        {
            return;
        }
        auto job = std::make_shared<PreparationJob>();
        const auto& project = session->Projects().CurrentProject();
        job->projectId = project.projectId;
        job->assetId = assetId;
        job->projectRoot = project.rootPath;
        job->assetPath = NormalizePath(assetPath);
        job->prepared = std::move(prepared);
        FillPreparationMetrics(*job);
        preparation = std::move(job);
    }

    bool CreatorAssetDragPreviewOwnsDrop(
        const bridge::StableId& assetId) noexcept
    {
        return bridge::IsValidStableId(assetId) &&
            preview.assetId == assetId &&
            preview.scene != nullptr &&
            IsLivePreviewWrapper(*preview.scene, preview.wrapper);
    }

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
        const auto assetId = preview.assetId;
        const auto assetPath = preview.assetPath;
        auto created = std::move(preview.createdEntities);
        preview = {};
        HideAndDeferPreviewRemoval(scene, wrapper, std::move(created));
        RequestCreatorAssetDragPreparation(assetId, assetPath);
    }

    bool CreatorAssetDragPreviewBlocksSave() noexcept
    {
        return pendingPreviewCleanupCount != 0 ||
            preview.wrapper != wi::ecs::INVALID_ENTITY;
    }

    wi::ecs::Entity UpdateCreatorAssetDragPreview(
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
            return wi::ecs::INVALID_ENTITY;
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
            return wi::ecs::INVALID_ENTITY;
        }

        if (wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            if (dragCancelledUntilRelease)
            {
                dragCancelledUntilRelease = false;
                ClearCreatorAssetDragPreview();
                return wi::ecs::INVALID_ENTITY;
            }
            if (preview.scene == nullptr ||
                !IsLivePreviewWrapper(*preview.scene, preview.wrapper))
            {
                return wi::ecs::INVALID_ENTITY;
            }
            if (!PointerInsideViewport(*chrome, pointer))
            {
                ClearCreatorAssetDragPreview();
                chrome->SetStatusText("ASSET DRAG // CANCELLED // OUTSIDE VIEWPORT");
                return wi::ecs::INVALID_ENTITY;
            }

            XMFLOAT3 surface;
            auto& scene = session->Scenes().GetScene();
            if (!ResolveSurface(canvas, camera, pointer, scene, surface) ||
                !PositionPreview(surface))
            {
                ClearCreatorAssetDragPreview();
                chrome->SetStatusText("ASSET DRAG // DROP FAILED // NO SURFACE");
                return wi::ecs::INVALID_ENTITY;
            }

            RestorePreviewLayers(scene, preview.originalLayers);
            if (auto* name = scene.names.GetComponent(preview.wrapper))
                name->name = "Reusable Asset Instance";

            const auto placedAssetId = preview.assetId;
            const auto placedAssetPath = preview.assetPath;
            auto command = std::make_unique<bridge::PlaceReusableModelCommand>(
                scene,
                placedAssetId,
                preview.wrapper,
                preview.payloadRoot,
                preview.firstMaterialIndex);
            auto* placed = command.get();
            if (!session->Commands().Execute(std::move(command)))
            {
                if (auto* name = scene.names.GetComponent(preview.wrapper))
                    name->name = DragPreviewWrapperName;
                ClearCreatorAssetDragPreview();
                chrome->SetStatusText("ASSET DRAG // DROP COMMIT FAILED");
                return wi::ecs::INVALID_ENTITY;
            }

            const wi::ecs::Entity placedEntity = placed->PlacedEntity();
            preview = {};
            dragCancelledUntilRelease = false;
            wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
            RequestCreatorAssetDragPreparation(placedAssetId, placedAssetPath);
            return placedEntity;
        }

        if (dragCancelledUntilRelease)
            return wi::ecs::INVALID_ENTITY;

        const bool dragging =
            chrome->AssetBrowserDragCandidate() &&
            chrome->AssetBrowserDragging() &&
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        if (!dragging)
        {
            ClearCreatorAssetDragPreview();
            return wi::ecs::INVALID_ENTITY;
        }

        const auto assetId = chrome->SelectedCreatorAssetId();
        const std::string assetPath = NormalizePath(chrome->AssetBrowserDragPath());
        if (!bridge::IsValidStableId(assetId) || assetPath.empty())
        {
            ClearCreatorAssetDragPreview();
            return wi::ecs::INVALID_ENTITY;
        }

        auto& scene = session->Scenes().GetScene();
        if (!PointerInsideViewport(*chrome, pointer))
        {
            if (preview.wrapper != wi::ecs::INVALID_ENTITY)
            {
                dragCancelledUntilRelease = true;
                ClearCreatorAssetDragPreview();
                chrome->SetStatusText(
                    "ASSET DRAG // CANCELLED // LEFT SCENE VIEWPORT");
            }
            return wi::ecs::INVALID_ENTITY;
        }

        XMFLOAT3 surface;
        if (!ResolveSurface(canvas, camera, pointer, scene, surface))
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return wi::ecs::INVALID_ENTITY;
        }

        if (preview.wrapper == wi::ecs::INVALID_ENTITY ||
            preview.assetId != assetId ||
            preview.assetPath != assetPath ||
            preview.scene != &scene ||
            !IsLivePreviewWrapper(scene, preview.wrapper))
        {
            if (preview.wrapper != wi::ecs::INVALID_ENTITY)
                ClearCreatorAssetDragPreview();

            const auto& project = session->Projects().CurrentProject();
            if (!preparation || !SamePreparedAsset(
                    *preparation,
                    project.projectId,
                    assetId,
                    assetPath))
            {
                RequestCreatorAssetDragPreparation(assetId, assetPath);
            }
            if (!preparation ||
                wi::jobsystem::IsBusy(preparation->context))
            {
                chrome->SetStatusText(
                    "ASSET DRAG // PREPARING IN BACKGROUND // KEEP HOLDING");
                wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
                return wi::ecs::INVALID_ENTITY;
            }
            if (!preparation->error.empty() || !preparation->prepared.IsReady())
            {
                chrome->SetStatusText(
                    "ASSET DRAG // PREPARATION FAILED // " + preparation->error);
                wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
                return wi::ecs::INVALID_ENTITY;
            }

            std::string error;
            if (!CreatePreviewFromPrepared(
                    *session, assetId, assetPath, surface, error))
            {
                chrome->SetStatusText("ASSET DRAG // PREVIEW FAILED // " + error);
                wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
                return wi::ecs::INVALID_ENTITY;
            }
        }
        else if (!PositionPreview(surface))
        {
            ClearCreatorAssetDragPreview();
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return wi::ecs::INVALID_ENTITY;
        }

        chrome->SetStatusText(
            "ASSET DRAG // LIVE TEXTURED INSTANCE // RELEASE TO PLACE");
        wi::input::SetCursor(wi::input::CURSOR_HAND);
        return wi::ecs::INVALID_ENTITY;
    }
}
