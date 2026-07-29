#include "StudioApplication.h"

#include <memory>
#include <utility>

namespace renegade::studio
{
    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);

        wi::scene::TransformComponent cameraTransform;
        cameraTransform.Translate(XMFLOAT3(0.0f, 1.5f, -4.0f));
        cameraTransform.UpdateTransform();
        camera->TransformCamera(cameraTransform);

        statusLabel_.Create("RenegadeStudioStatus");
        statusLabel_.SetSize(XMFLOAT2(720.0f, 24.0f));
        statusLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        GetGUI().AddWidget(&statusLabel_);

        hierarchyLabel_.Create("Scene Hierarchy");
        hierarchyLabel_.SetText("SCENE HIERARCHY");
        hierarchyLabel_.SetSize(XMFLOAT2(280.0f, 24.0f));
        GetGUI().AddWidget(&hierarchyLabel_);

        hierarchyTree_.Create("RenegadeHierarchy");
        hierarchyTree_.SetSize(XMFLOAT2(280.0f, 520.0f));
        hierarchyTree_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
            {
                return;
            }
            session_->Selection().Select(static_cast<wi::ecs::Entity>(args.userdata));
            RefreshInspector();
            RefreshStatus();
        });
        GetGUI().AddWidget(&hierarchyTree_);

        inspectorLabel_.Create("Transform Inspector");
        inspectorLabel_.SetText("TRANSFORM — select an entity");
        inspectorLabel_.SetSize(XMFLOAT2(300.0f, 24.0f));
        GetGUI().AddWidget(&inspectorLabel_);

        const auto createTranslationInput = [this](
            wi::gui::TextInputField& input,
            const char* name,
            const char* description,
            const int axis)
        {
            input.Create(name);
            input.SetDescription(description);
            input.SetValue(0.0f);
            input.SetSize(XMFLOAT2(90.0f, 24.0f));
            input.OnInputAccepted([this, axis](const wi::gui::EventArgs& args)
            {
                ApplySelectedTranslation(axis, args.fValue);
            });
            GetGUI().AddWidget(&input);
        };
        createTranslationInput(translationX_, "TranslationX", "X: ", 0);
        createTranslationInput(translationY_, "TranslationY", "Y: ", 1);
        createTranslationInput(translationZ_, "TranslationZ", "Z: ", 2);

        undoButton_.Create("Undo Transform");
        undoButton_.SetText("Undo");
        undoButton_.SetSize(XMFLOAT2(90.0f, 24.0f));
        undoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingHistoryAction_ = HistoryAction::Undo;
        });
        GetGUI().AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("Redo");
        redoButton_.SetSize(XMFLOAT2(90.0f, 24.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingHistoryAction_ = HistoryAction::Redo;
        });
        GetGUI().AddWidget(&redoButton_);

        saveAsButton_.Create("Save Scene As");
        saveAsButton_.SetText("Save As...");
        saveAsButton_.SetSize(XMFLOAT2(100.0f, 24.0f));
        saveAsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SaveSceneAs();
        });
        GetGUI().AddWidget(&saveAsButton_);

        reopenButton_.Create("Reopen Scene");
        reopenButton_.SetText("Reopen");
        reopenButton_.SetSize(XMFLOAT2(90.0f, 24.0f));
        reopenButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReopenScene();
        });
        GetGUI().AddWidget(&reopenButton_);

        gizmo_.isTranslator = true;
        gizmo_.isRotator = false;
        gizmo_.isScalator = false;
        gizmo_.translate_snap = 0.1f;

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();

        RenderPath3D::Load();
    }

    void StudioRenderPath::Update(const float dt)
    {
        RenderPath3D::Update(dt);

        if (session_ == nullptr)
        {
            return;
        }

        // wiGUI invokes OnClick while Button::Update is still active. If a
        // callback disables its own button when a history stack becomes empty,
        // the button can remain focused and force-disable widgets after it.
        // Apply history only after the complete GUI update has returned.
        if (pendingHistoryAction_ != HistoryAction::None)
        {
            const HistoryAction action = pendingHistoryAction_;
            pendingHistoryAction_ = HistoryAction::None;

            const bool changed = action == HistoryAction::Undo
                ? session_->Commands().Undo()
                : session_->Commands().Redo();
            if (changed)
            {
                RefreshInspector();
                RefreshStatus();
            }
            return;
        }

        if (gizmoEntity_ != session_->Selection().SelectedEntity())
        {
            SyncGizmoSelection();
        }

        if (gizmoEntity_ == wi::ecs::INVALID_ENTITY)
        {
            return;
        }

        // wiGUI processes button callbacks in RenderPath3D::Update(). Do not
        // let the same mouse press also start a viewport translation behind a
        // focused widget. An existing gizmo drag is allowed to finish if the
        // pointer crosses a panel before release.
        if (GetGUI().HasFocus() && !gizmoDragActive_)
        {
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        gizmo_.Update(*camera, pointer, *this);

        if (gizmo_.IsDragStarted())
        {
            gizmoDragActive_ = true;
        }

        if (gizmo_.IsDragEnded())
        {
            gizmoDragActive_ = false;
            auto* transform =
                session_->Scenes().GetScene().transforms.GetComponent(gizmoEntity_);
            if (transform == nullptr)
            {
                return;
            }

            const XMFLOAT3 translationAfter = transform->translation_local;
            transform->translation_local = gizmoTranslationBefore_;
            transform->SetDirty();
            transform->UpdateTransform();

            session_->Commands().Execute(
                std::make_unique<bridge::SetTranslationCommand>(
                    session_->Scenes().GetScene(),
                    gizmoEntity_,
                    gizmoTranslationBefore_,
                    translationAfter));
            gizmoTranslationBefore_ = translationAfter;
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::Compose(const wi::graphics::CommandList cmd) const
    {
        RenderPath3D::Compose(cmd);
        if (gizmoEntity_ != wi::ecs::INVALID_ENTITY)
        {
            gizmo_.Draw(*camera, wi::input::GetPointer(), cmd);
        }
    }

    void StudioRenderPath::ResizeLayout()
    {
        RenderPath3D::ResizeLayout();
        statusLabel_.SetPos(XMFLOAT2(16.0f, GetLogicalHeight() - 40.0f));
        hierarchyLabel_.SetPos(XMFLOAT2(16.0f, 16.0f));
        hierarchyTree_.SetPos(XMFLOAT2(16.0f, 44.0f));
        hierarchyTree_.SetSize(XMFLOAT2(280.0f, GetLogicalHeight() - 100.0f));

        const float inspectorX = GetLogicalWidth() - 330.0f;
        inspectorLabel_.SetPos(XMFLOAT2(inspectorX, 16.0f));
        translationX_.SetPos(XMFLOAT2(inspectorX, 48.0f));
        translationY_.SetPos(XMFLOAT2(inspectorX + 100.0f, 48.0f));
        translationZ_.SetPos(XMFLOAT2(inspectorX + 200.0f, 48.0f));
        undoButton_.SetPos(XMFLOAT2(inspectorX, 82.0f));
        redoButton_.SetPos(XMFLOAT2(inspectorX + 100.0f, 82.0f));
        saveAsButton_.SetPos(XMFLOAT2(inspectorX, 116.0f));
        reopenButton_.SetPos(XMFLOAT2(inspectorX + 110.0f, 116.0f));
    }

    void StudioRenderPath::RefreshStatus()
    {
        if (session_ == nullptr)
        {
            statusLabel_.SetText("Renegade Studio | EngineBridge session unavailable");
            return;
        }

        const auto& scenes = session_->Scenes();
        if (!scenes.LastError().empty())
        {
            statusLabel_.SetText("Renegade Studio | " + scenes.LastError());
            return;
        }

        statusLabel_.SetText(
            "Renegade Studio | " +
            std::to_string(scenes.EntityCount()) +
            " entities | " +
            (scenes.CurrentPath().empty() ? "Untitled" : scenes.CurrentPath()) +
            " | Undo " + std::to_string(session_->Commands().UndoCount()) +
            " / Redo " + std::to_string(session_->Commands().RedoCount()));
    }

    void StudioRenderPath::RefreshHierarchy()
    {
        hierarchyTree_.ClearItems();
        if (session_ == nullptr)
        {
            return;
        }

        const auto selected = session_->Selection().SelectedEntity();
        for (const auto& entity : session_->Scenes().ListEntities())
        {
            wi::gui::TreeList::Item item;
            item.name = entity.name;
            item.level = entity.depth;
            item.userdata = entity.entity;
            item.open = true;
            item.selected = entity.entity == selected;
            hierarchyTree_.AddItem(item);
        }
    }

    void StudioRenderPath::RefreshInspector()
    {
        const bool hasSession = session_ != nullptr;
        const auto entity = hasSession
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        auto* transform = hasSession
            ? session_->Scenes().GetScene().transforms.GetComponent(entity)
            : nullptr;

        const bool hasTransform = transform != nullptr;
        translationX_.SetEnabled(hasTransform);
        translationY_.SetEnabled(hasTransform);
        translationZ_.SetEnabled(hasTransform);
        undoButton_.SetEnabled(hasSession && session_->Commands().CanUndo());
        redoButton_.SetEnabled(hasSession && session_->Commands().CanRedo());
        reopenButton_.SetEnabled(
            hasSession && !session_->Scenes().CurrentPath().empty());

        if (!hasTransform)
        {
            inspectorLabel_.SetText("TRANSFORM — select an entity");
            translationX_.SetValue(0.0f);
            translationY_.SetValue(0.0f);
            translationZ_.SetValue(0.0f);
            return;
        }

        inspectorLabel_.SetText("TRANSFORM — entity " + std::to_string(entity));
        translationX_.SetValue(transform->translation_local.x);
        translationY_.SetValue(transform->translation_local.y);
        translationZ_.SetValue(transform->translation_local.z);
        SyncGizmoSelection();
    }

    void StudioRenderPath::ApplySelectedTranslation(const int axis, const float value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto* transform = session_->Scenes().GetScene().transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        XMFLOAT3 translation = transform->translation_local;
        if (axis == 0)
        {
            translation.x = value;
        }
        else if (axis == 1)
        {
            translation.y = value;
        }
        else
        {
            translation.z = value;
        }

        session_->Commands().Execute(std::make_unique<bridge::SetTranslationCommand>(
            session_->Scenes().GetScene(),
            entity,
            translation));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::SyncGizmoSelection()
    {
        gizmo_.selected.clear();
        gizmo_.selectedEntitiesNonRecursive.clear();
        gizmoEntity_ = wi::ecs::INVALID_ENTITY;
        gizmoDragActive_ = false;

        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        wi::scene::PickResult selected;
        selected.entity = entity;
        gizmo_.scene = &scene;
        gizmo_.selected.push_back(selected);
        gizmo_.selectedEntitiesNonRecursive.push_back(entity);
        gizmo_.PreTranslate();
        gizmoEntity_ = entity;
        gizmoTranslationBefore_ = transform->translation_local;
    }

    void StudioRenderPath::SaveSceneAs()
    {
        if (session_ == nullptr)
        {
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::SAVE;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(params, [this](const std::string& selectedPath)
        {
            const std::string scenePath =
                wi::helper::ForceExtension(selectedPath, "wiscene");
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, scenePath](uint64_t)
                {
                    session_->SaveScene(scenePath);
                    RefreshStatus();
                    RefreshInspector();
                });
        });
    }

    void StudioRenderPath::ReopenScene()
    {
        if (session_ == nullptr)
        {
            return;
        }

        session_->ReloadScene();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioApplication::SetStartupScene(std::string filePath)
    {
        if (!filePath.empty())
        {
            startupScene_ = std::move(filePath);
        }
    }

    void StudioApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.device_name = true;
        infoDisplay.resolution = true;
        infoDisplay.logical_size = true;
        infoDisplay.colorspace = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindSession(session_);
        renderer_.init(canvas);
        session_.LoadScene(startupScene_);
        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
