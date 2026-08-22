#pragma once

#include <functional>
#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/screen/ScreenRenderer.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    // Gate 8C's visible Screen authoring shell. It owns editor selection and
    // controls only; document mutations stay in ScreenAuthoringSession and the
    // central preview is always the shared Gate 8B ScreenRenderer.
    class RenegadeScreenEditorWorkspace final : public wi::gui::Widget
    {
    public:
        void Create();
        void SetLayout(float width, float height);
        void Bind(
            bridge::ScreenAuthoringSession* session,
            screen::ScreenRenderer* renderer);
        void Clear() noexcept;
        void SelectWidget(const bridge::StableId& widgetId);

        void OnDocumentChanged(std::function<void()> callback);
        void OnReturnRequested(std::function<void()> callback);

        [[nodiscard]] bridge::ScreenRect PreviewBounds() const noexcept;
        [[nodiscard]] const bridge::StableId& SelectedWidgetId() const noexcept
        {
            return selectedWidgetId_;
        }

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeScreenEditorWorkspace";
        }

    private:
        void CreateControls();
        void LayoutControls();
        void UpdateControls(const wi::Canvas& canvas, float dt);
        void RenderControls(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const;
        void RefreshInspector();
        void EnsureSelection();
        void ApplySelectedWidget();
        void SaveScreen();
        void UndoScreen();
        void RedoScreen();
        void ToggleVisible();
        void ToggleEnabled();
        void ReturnToStoryFlow();
        void RefreshAfterMutation(std::string status);
        void SetStatus(std::string status);

        bridge::ScreenAuthoringSession* session_ = nullptr;
        screen::ScreenRenderer* renderer_ = nullptr;
        bridge::StableId selectedWidgetId_;
        std::function<void()> documentChanged_;
        std::function<void()> returnRequested_;
        std::string status_ = "NO SCREEN OPEN";
        float width_ = 1.0f;
        float height_ = 1.0f;
        std::size_t hierarchyScroll_ = 0;

        RenegadeButton returnButton_;
        RenegadeButton saveButton_;
        RenegadeButton undoButton_;
        RenegadeButton redoButton_;
        RenegadeButton applyButton_;
        RenegadeButton visibleButton_;
        RenegadeButton enabledButton_;
        RenegadeTextInputField nameInput_;
        RenegadeTextInputField textInput_;
        RenegadeTextInputField xInput_;
        RenegadeTextInputField yInput_;
        RenegadeTextInputField widthInput_;
        RenegadeTextInputField heightInput_;
    };
}
