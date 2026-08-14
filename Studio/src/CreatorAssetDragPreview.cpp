#include "RenegadeStudioChrome.h"

#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace renegade::studio::detail
{
    namespace
    {
        constexpr std::uint32_t DragPreviewLayer = 1u << 31;

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

        bool IsInside(const XMFLOAT4& bounds, const XMFLOAT4& pointer) noexcept
        {
            return pointer.x >= bounds.x && pointer.x < bounds.z &&
                pointer.y >= bounds.y && pointer.y < bounds.w;
        }

        bool ResolveSurfacePosition(
            const XMFLOAT4& pointer,
            const wi::Canvas& canvas,
            const wi::scene::CameraComponent& camera,
            wi::scene::Scene& scene,
            XMFLOAT3& surfacePosition)
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
                surfacePosition = picked.position;
                return true;
            }

            if (std::abs(ray.direction.y) <= 0.0001f)
                return false;
            const float distance = -ray.origin.y / ray.direction.y;
            if (distance < ray.TMin || distance > ray.TMax)
                return false;
            surfacePosition = XMFLOAT3(
                ray.origin.x + ray.direction.x * distance,
                0.0f,
                ray.origin.z + ray.direction.z * distance);
            return true;
        }

        void MarkPreviewLayer(wi::scene::Scene& scene, const wi::ecs::Entity wrapper)
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
            if (!prepared.IsReady() || prepared.PeekMutableScene() == nullptr)
                return false;

            wi::scene::Scene* prefab = prepared.PeekMutableScene();
            const float scale = bridge::HasCreatorAuthoredTransform(*prefab)
                ? 1.0f
                : bridge::ImportService::ResolveScaleFactor(
                    bridge::ModelScaleMode::Automatic, *prefab);
            const bridge::ModelBounds bounds =
                bridge::ImportService::MeasureModelBounds(*prefab);
            if (!bounds.valid)
                return false;

            auto& scene = session.Scenes().GetScene();
            wi::unordered_set<wi::ecs::Entity> before;
            scene.FindAllEntities(before);

            auto payload = prepared.ReleaseScene();
            if (!payload.IsValid())
                return false;
            scene.Merge(*payload);

            wi::unordered_set<wi::ecs::Entity> afterMerge;
            scene.FindAllEntities(afterMerge);
            if (afterMerge.size() <= before.size())
                return false;

            const wi::ecs::Entity wrapper =
                scene.Entity_CreateTransform("Reusable Asset Drag Preview");
            auto* transform = scene.transforms.GetComponent(wrapper);
            if (transform == nullptr)
            {
                return false;
            }

            // Attach only newly-merged top-level transform roots. Resource and
            // component entities without transforms remain tracked separately
            // and are removed explicitly when the preview is cleared.
            std::vector<wi::ecs::Entity> roots;
            for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.transforms.GetEntity(index);
                if (entity == wrapper || before.count(entity) != 0)
                    continue;
                const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                if (hierarchy == nullptr ||
                    hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                    before.count(hierarchy->parentID) != 0)
                {
                    roots.push_back(entity);
                }
            }
            if (roots.empty())
            {
                scene.Entity_Remove(wrapper, false);
                for (const wi::ecs::Entity entity : afterMerge)
                {
                    if (before.count(entity) == 0)
                        scene.Entity_Remove(entity, false);
                }
                return false;
            }
            for (const wi::ecs::Entity root : roots)
                scene.Component_Attach(root, wrapper, true);

            preview.assetId = assetId;
            preview.assetPath = assetPath;
            preview.scene = &scene;
            preview.wrapper = wrapper;
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
            XMFLOAT3 position = surfacePosition;
            position.y = bridge::ImportService::ResolveGroundedPlacementY(
                surfacePosition.y, bounds, scale);
            transform->translation_local = position;
            transform->scale_local = XMFLOAT3(scale, scale, scale);
            transform->SetDirty();
            MarkPreviewLayer(scene, wrapper);
            chrome.SetStatusText(
                "PLACE ASSET // PREVIEW // RELEASE LMB TO COMMIT");
            return true;
        }
    }

    void ClearCreatorAssetDragPreview()
    {
        if (preview.scene != nullptr)
        {
            if (preview.wrapper != wi::ecs::INVALID_ENTITY &&
                preview.scene->transforms.GetComponent(preview.wrapper) != nullptr)
            {
                preview.scene->Entity_Remove(preview.wrapper, true);
            }

            // Instantiate can create resource/component entities that are not
            // hierarchy children of the returned wrapper. Remove only the exact
            // entities created by this one preview; never sweep production scene
            // state by component type or index.
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

    void UpdateCreatorAssetDragPreview(
        const wi::Canvas& canvas,
        const wi::scene::CameraComponent& camera)
    {
        CreatorAssetStudioChrome* chrome = CreatorAssetStudioChrome::Current();
        bridge::StudioSession* session = bridge::StudioSession::Current();
        if (chrome == nullptr || session == nullptr ||
            !session->Projects().HasProject())
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        const bool dragging = chrome->AssetBrowserDragCandidate() &&
            chrome->AssetBrowserDragging() &&
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        if (!dragging)
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        const std::string& dragPath = chrome->AssetBrowserDragPath();
        const bridge::StableId& assetId = chrome->SelectedCreatorAssetId();
        if (!bridge::IsValidStableId(assetId) || dragPath.empty() ||
            dragPath != chrome->SelectedCreatorAssetPath())
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        if (!IsInside(chrome->ViewportBounds(), pointer))
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        auto& scene = session->Scenes().GetScene();
        XMFLOAT3 surfacePosition;
        if (!ResolveSurfacePosition(
                pointer, canvas, camera, scene, surfacePosition))
        {
            ClearCreatorAssetDragPreview();
            return;
        }

        if (preview.wrapper == wi::ecs::INVALID_ENTITY ||
            preview.scene != &scene || preview.assetId != assetId ||
            preview.assetPath != dragPath)
        {
            ClearCreatorAssetDragPreview();
            if (!CreatePreview(*chrome, *session, assetId, dragPath, surfacePosition))
                ClearCreatorAssetDragPreview();
            return;
        }

        auto* transform = scene.transforms.GetComponent(preview.wrapper);
        if (transform == nullptr)
        {
            ClearCreatorAssetDragPreview();
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
