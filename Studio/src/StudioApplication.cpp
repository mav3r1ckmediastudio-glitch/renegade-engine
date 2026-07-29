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
            if (session_ != nullptr && session_->Commands().Undo())
            {
                RefreshInspector();
                RefreshStatus();
            }
        });
        GetGUI().AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("Redo");
        redoButton_.SetSize(XMFLOAT2(90.0f, 24.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (session_ != nullptr && session_->Commands().Redo())
            {
                RefreshInspector();
                RefreshStatus();
            }
        });
        GetGUI().AddWidget(&redoButton_);

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();

        RenderPath3D::Load();
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
        infoDisplay.resolution = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindSession(session_);
        renderer_.init(canvas);
        session_.LoadScene(startupScene_);
        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
