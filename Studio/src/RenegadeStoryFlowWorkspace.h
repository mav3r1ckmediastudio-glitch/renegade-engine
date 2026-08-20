#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowLayoutService.h"

namespace renegade::studio
{
    // Gate 1 native read-only Story Flow surface. This widget renders the
    // authoritative LP02 Flow through the shared presentation model and owns
    // only editor interaction state. It deliberately exposes no semantic
    // mutation callbacks.
    class RenegadeStoryFlowWorkspace final : public wi::gui::Widget
    {
    public:
        void Create();
        void SetLayout(float width, float height);
        void Bind(
            const bridge::StoryFlowAuthoringModel* model,
            bridge::StoryFlowLayoutDocument* layout);
        void Clear() noexcept;

        void FitToContent();
        void CenterOnGameStart();

        void OnSelectionChanged(
            std::function<void(const bridge::StableId&)> callback);
        void OnLayoutChanged(std::function<void()> callback);

        [[nodiscard]] const bridge::StableId& SelectedNodeId() const noexcept
        {
            return selectedNodeId_;
        }

        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept
        {
            return pointerConsumed_;
        }

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowWorkspace";
        }

    private:
        [[nodiscard]] const bridge::StoryFlowNodeLayout* FindLayout(
            const bridge::StableId& nodeId) const noexcept;
        [[nodiscard]] bridge::StoryFlowNodeLayout* FindLayout(
            const bridge::StableId& nodeId) noexcept;
        [[nodiscard]] XMFLOAT2 CanvasToScreen(float x, float y) const noexcept;
        [[nodiscard]] XMFLOAT2 ScreenToCanvas(float x, float y) const noexcept;
        [[nodiscard]] XMFLOAT4 NodeScreenBounds(
            const bridge::StoryFlowNodeLayout& node) const noexcept;
        [[nodiscard]] bool PointerInsideWorkspace(const XMFLOAT4& pointer) const noexcept;
        void SelectNode(const bridge::StableId& nodeId);
        void NotifyLayoutChanged();

        const bridge::StoryFlowAuthoringModel* model_ = nullptr;
        bridge::StoryFlowLayoutDocument* layout_ = nullptr;
        bridge::StableId selectedNodeId_;
        std::function<void(const bridge::StableId&)> selectionChanged_;
        std::function<void()> layoutChanged_;
        float width_ = 1.0f;
        float height_ = 1.0f;
        bool panning_ = false;
        XMFLOAT2 panPointerAnchor_ = {};
        XMFLOAT2 panValueAnchor_ = {};
        bool pointerConsumed_ = false;
    };
}
