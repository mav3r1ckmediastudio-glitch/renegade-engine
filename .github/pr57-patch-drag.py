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
replace_once(
    app,
    """        creatorAssetDropPending_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        studioChrome_.SetStatusText("PLACE ASSET // CANCELLED");
""",
    """        creatorAssetDropPending_ = false;
        detail::ClearCreatorAssetDragPreview();
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        studioChrome_.SetStatusText("PLACE ASSET // CANCELLED");
""",
)

handler = r'''    bool StudioRenderPath::HandleCreatorAssetPlacement(
        const XMFLOAT4& pointer)
    {
        if (!creatorAssetPlacementActive_ || session_ == nullptr)
            return false;

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            CancelCreatorAssetPlacement();
            return true;
        }

        if (creatorAssetDropPending_)
        {
            bridge::PreparedReusableModelPlacement prepared;
            XMFLOAT3 position = {};
            float scale = 1.0f;
            if (!detail::ConsumeCreatorAssetDragPreview(
                    creatorAssetPlacementId_,
                    prepared,
                    position,
                    scale))
            {
                CancelCreatorAssetPlacement();
                studioChrome_.SetStatusText(
                    "PLACE ASSET // DROP CANCELLED // PREVIEW NOT READY");
                return true;
            }
            creatorAssetDropPending_ = false;

            auto& scene = session_->Scenes().GetScene();
            const bridge::StableId assetId = creatorAssetPlacementId_;
            auto command =
                std::make_unique<bridge::PlaceReusableModelCommand>(
                    scene,
                    prepared.ReleaseScene(),
                    assetId,
                    position,
                    scale);
            auto* placed = command.get();
            if (!session_->Commands().Execute(std::move(command)))
            {
                creatorAssetPlacementActive_ = false;
                creatorAssetPlacementId_.clear();
                creatorAssetPlacementLabel_.clear();
                wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
                studioChrome_.SetStatusText(
                    "PLACE ASSET // DROP COMMIT FAILED");
                return true;
            }

            const std::string label = creatorAssetPlacementLabel_;
            creatorAssetPlacementActive_ = false;
            creatorAssetPlacementId_.clear();
            creatorAssetPlacementLabel_.clear();
            creatorAssetDropPending_ = false;
            wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
            session_->Selection().Select(placed->PlacedEntity());
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
            studioChrome_.SetStatusText(
                "PLACE ASSET // " + label +
                " // EXACT CURSOR PREVIEW COMMITTED // STABLE RASSET INSTANCE");
            return true;
        }

        if (flyCameraActive_ ||
            GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer))
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return true;
        }

        auto& scene = session_->Scenes().GetScene();
        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN,
            ~0u,
            scene);
        XMFLOAT3 surfacePosition = picked.position;
        bool hasSurface = picked.entity != wi::ecs::INVALID_ENTITY;
        if (!hasSurface && std::abs(ray.direction.y) > 0.0001f)
        {
            const float distance = -ray.origin.y / ray.direction.y;
            if (distance >= ray.TMin && distance <= ray.TMax)
            {
                surfacePosition = XMFLOAT3(
                    ray.origin.x + ray.direction.x * distance,
                    0.0f,
                    ray.origin.z + ray.direction.z * distance);
                hasSurface = true;
            }
        }
        if (!hasSurface)
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return true;
        }

        wi::input::SetCursor(wi::input::CURSOR_CROSS);
        wi::renderer::DrawSphere(
            wi::primitive::Sphere(surfacePosition, 0.16f),
            XMFLOAT4(1.0f, 0.36f, 0.06f, 0.9f),
            false);
        if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            return true;

        const auto& project = session_->Projects().CurrentProject();
        bridge::CreatorAssetWorkflowService workflow;
        auto prepared = workflow.PrepareModelPlacement(
            project.rootPath,
            project.projectId,
            creatorAssetPlacementId_);
        if (!prepared.IsReady())
        {
            studioChrome_.SetStatusText(
                "PLACE ASSET // PREPARE FAILED // " +
                prepared.Result().error);
            return true;
        }

        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        const float scale = bridge::HasCreatorAuthoredTransform(*preparedScene)
            ? 1.0f
            : bridge::ImportService::ResolveScaleFactor(
                bridge::ModelScaleMode::Automatic,
                *preparedScene);
        const bridge::ModelBounds bounds =
            bridge::ImportService::MeasureModelBounds(*preparedScene);
        XMFLOAT3 position = surfacePosition;
        if (bounds.valid)
        {
            position.y = bridge::ImportService::ResolveGroundedPlacementY(
                surfacePosition.y,
                bounds,
                scale);
        }

        const bridge::StableId assetId = creatorAssetPlacementId_;
        auto command = std::make_unique<bridge::PlaceReusableModelCommand>(
            scene,
            prepared.ReleaseScene(),
            assetId,
            position,
            scale);
        auto* placed = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            studioChrome_.SetStatusText("PLACE ASSET // FAILED");
            return true;
        }

        const std::string label = creatorAssetPlacementLabel_;
        creatorAssetPlacementActive_ = false;
        creatorAssetPlacementId_.clear();
        creatorAssetPlacementLabel_.clear();
        creatorAssetDropPending_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        session_->Selection().Select(placed->PlacedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        studioChrome_.SetStatusText(
            "PLACE ASSET // " + label +
            " // SURFACE GROUNDED // STABLE RASSET INSTANCE");
        return true;
    }

'''
regex_once(
    app,
    r"    bool StudioRenderPath::HandleCreatorAssetPlacement\(\n        const XMFLOAT4& pointer\)\n    \{.*?\n    \}\n\n    bool StudioRenderPath::HandleLightPlacement",
    handler + "    bool StudioRenderPath::HandleLightPlacement",
)

text = load(app)
if text.count("workflow.PrepareModelPlacement(") != 1:
    raise RuntimeError(
        "Studio should prepare only the direct PLACE path; drag/drop must consume preview")
if "ConsumeCreatorAssetDragPreview" not in text:
    raise RuntimeError("retained drag preview consume seam missing")

print("PR57_DRAG_PATCH_APPLIED")
