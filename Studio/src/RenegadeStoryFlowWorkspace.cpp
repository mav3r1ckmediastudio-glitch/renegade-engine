#include "RenegadeStoryFlowWorkspace.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    constexpr float HeaderHeight = 48.0f;
    constexpr float Padding = 28.0f;
    constexpr float NodeWidth = 210.0f;
    constexpr float NodeHeight = 112.0f;
    constexpr float MinZoom = 0.20f;
    constexpr float MaxZoom = 2.50f;

    constexpr wi::Color Surface(7, 11, 14, 255);
    constexpr wi::Color Raised(13, 19, 23, 255);
    constexpr wi::Color Border(38, 52, 61, 255);
    constexpr wi::Color Text(244, 244, 244, 255);
    constexpr wi::Color Muted(132, 143, 149, 255);
    constexpr wi::Color Accent(210, 91, 29, 255);
    constexpr wi::Color Route(76, 96, 106, 255);

    void Rect(float x, float y, float w, float h, wi::Color color,
        wi::graphics::CommandList cmd)
    {
        if (w <= 0 || h <= 0) return;
        wi::image::Params params(x, y, w, h, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void Label(const std::string& value, float x, float y, int size,
        wi::Color color, wi::graphics::CommandList cmd)
    {
        wi::font::Params params(x, y, size, wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
        params.bolden = 0.14f;
        wi::font::Draw(value, params, cmd);
    }

    void Line(XMFLOAT2 start, XMFLOAT2 end, wi::Color color)
    {
        wi::renderer::RenderableLine2D line;
        line.start = start;
        line.end = end;
        line.color_start = color;
        line.color_end = color;
        wi::renderer::DrawLine(line);
    }

    const char* KindLabel(renegade::bridge::FlowNodeKind kind)
    {
        using renegade::bridge::FlowNodeKind;
        switch (kind)
        {
        case FlowNodeKind::GameStart: return "GAME START";
        case FlowNodeKind::Level: return "LEVEL";
        case FlowNodeKind::CompleteGame: return "COMPLETE GAME";
        case FlowNodeKind::ReturnToMainMenu: return "RETURN TO MENU";
        case FlowNodeKind::Quit: return "QUIT";
        default: return "UNKNOWN";
        }
    }

    bool Contains(const XMFLOAT4& bounds, const XMFLOAT4& pointer)
    {
        return pointer.x >= bounds.x && pointer.y >= bounds.y &&
            pointer.x < bounds.x + bounds.z && pointer.y < bounds.y + bounds.w;
    }
}

namespace renegade::studio
{
    void RenegadeStoryFlowWorkspace::Create()
    {
        SetName("Renegade Story Flow workspace");
        SetShadowRadius(0.0f);
        SetLayout(width_, height_);
    }

    void RenegadeStoryFlowWorkspace::SetLayout(float width, float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        SetSize(XMFLOAT2(width_, height_));
    }

    void RenegadeStoryFlowWorkspace::Bind(
        const bridge::StoryFlowAuthoringModel* model,
        bridge::StoryFlowLayoutDocument* layout)
    {
        model_ = model;
        layout_ = layout;
        if (model_ && layout_ && model_->IsLoaded())
            selectedNodeId_ = model_->GameStartNodeId();
        else
            selectedNodeId_.clear();
    }

    void RenegadeStoryFlowWorkspace::Clear() noexcept
    {
        model_ = nullptr;
        layout_ = nullptr;
        selectedNodeId_.clear();
        panning_ = false;
    }

    void RenegadeStoryFlowWorkspace::OnSelectionChanged(
        std::function<void(const bridge::StableId&)> callback)
    {
        selectionChanged_ = std::move(callback);
    }

    void RenegadeStoryFlowWorkspace::OnLayoutChanged(
        std::function<void()> callback)
    {
        layoutChanged_ = std::move(callback);
    }

    const bridge::StoryFlowNodeLayout* RenegadeStoryFlowWorkspace::FindLayout(
        const bridge::StableId& id) const noexcept
    {
        if (!layout_) return nullptr;
        const auto it = std::find_if(layout_->nodes.begin(), layout_->nodes.end(),
            [&](const bridge::StoryFlowNodeLayout& item) { return item.nodeId == id; });
        return it == layout_->nodes.end() ? nullptr : &*it;
    }

    bridge::StoryFlowNodeLayout* RenegadeStoryFlowWorkspace::FindLayout(
        const bridge::StableId& id) noexcept
    {
        if (!layout_) return nullptr;
        const auto it = std::find_if(layout_->nodes.begin(), layout_->nodes.end(),
            [&](const bridge::StoryFlowNodeLayout& item) { return item.nodeId == id; });
        return it == layout_->nodes.end() ? nullptr : &*it;
    }

    XMFLOAT2 RenegadeStoryFlowWorkspace::CanvasToScreen(float x, float y) const noexcept
    {
        const float zoom = layout_ ? layout_->canvas.zoom : 1.0f;
        return XMFLOAT2(translation.x + Padding + (layout_ ? layout_->canvas.panX : 0) + x * zoom,
            translation.y + HeaderHeight + Padding + (layout_ ? layout_->canvas.panY : 0) + y * zoom);
    }

    XMFLOAT2 RenegadeStoryFlowWorkspace::ScreenToCanvas(float x, float y) const noexcept
    {
        const float zoom = layout_ ? std::max(0.001f, layout_->canvas.zoom) : 1.0f;
        return XMFLOAT2((x - translation.x - Padding - (layout_ ? layout_->canvas.panX : 0)) / zoom,
            (y - translation.y - HeaderHeight - Padding - (layout_ ? layout_->canvas.panY : 0)) / zoom);
    }

    XMFLOAT4 RenegadeStoryFlowWorkspace::NodeScreenBounds(
        const bridge::StoryFlowNodeLayout& node) const noexcept
    {
        const XMFLOAT2 p = CanvasToScreen(node.x, node.y);
        const float zoom = layout_ ? layout_->canvas.zoom : 1.0f;
        return XMFLOAT4(p.x, p.y, NodeWidth * zoom, NodeHeight * zoom);
    }

    bool RenegadeStoryFlowWorkspace::PointerInsideWorkspace(const XMFLOAT4& p) const noexcept
    {
        return p.x >= translation.x && p.y >= translation.y + HeaderHeight &&
            p.x < translation.x + scale.x && p.y < translation.y + scale.y;
    }

    void RenegadeStoryFlowWorkspace::SelectNode(const bridge::StableId& id)
    {
        if (selectedNodeId_ == id) return;
        selectedNodeId_ = id;
        if (selectionChanged_) selectionChanged_(selectedNodeId_);
    }

    void RenegadeStoryFlowWorkspace::NotifyLayoutChanged()
    {
        if (layoutChanged_) layoutChanged_();
    }

    void RenegadeStoryFlowWorkspace::FitToContent()
    {
        if (!model_ || !layout_ || layout_->nodes.empty()) return;
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (const auto& node : layout_->nodes)
        {
            if (!model_->FindNode(node.nodeId)) continue;
            minX = std::min(minX, node.x); minY = std::min(minY, node.y);
            maxX = std::max(maxX, node.x + NodeWidth); maxY = std::max(maxY, node.y + NodeHeight);
        }
        if (minX > maxX || minY > maxY) return;
        const float cw = std::max(1.0f, maxX - minX);
        const float ch = std::max(1.0f, maxY - minY);
        layout_->canvas.zoom = std::clamp(std::min((width_ - 4 * Padding) / cw,
            (height_ - HeaderHeight - 4 * Padding) / ch), MinZoom, MaxZoom);
        layout_->canvas.panX = (width_ - cw * layout_->canvas.zoom) * 0.5f - Padding - minX * layout_->canvas.zoom;
        layout_->canvas.panY = (height_ - HeaderHeight - ch * layout_->canvas.zoom) * 0.5f - Padding - minY * layout_->canvas.zoom;
        NotifyLayoutChanged();
    }

    void RenegadeStoryFlowWorkspace::CenterOnGameStart()
    {
        if (!model_ || !layout_) return;
        const auto* node = FindLayout(model_->GameStartNodeId());
        if (!node) return;
        layout_->canvas.panX = width_ * 0.5f - Padding - (node->x + NodeWidth * 0.5f) * layout_->canvas.zoom;
        layout_->canvas.panY = (height_ - HeaderHeight) * 0.5f - Padding - (node->y + NodeHeight * 0.5f) * layout_->canvas.zoom;
        NotifyLayoutChanged();
    }

    void RenegadeStoryFlowWorkspace::Update(const wi::Canvas& canvas, float dt)
    {
        Widget::Update(canvas, dt);
        pointerConsumed_ = false;
        if (!IsVisible() || !IsEnabled() || !model_ || !layout_) return;
        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool inside = PointerInsideWorkspace(pointer);
        pointerConsumed_ = inside;
        if (inside && std::abs(pointer.z) > 0.001f)
        {
            const XMFLOAT2 before = ScreenToCanvas(pointer.x, pointer.y);
            layout_->canvas.zoom = std::clamp(layout_->canvas.zoom * (pointer.z > 0 ? 1.1f : 1.0f / 1.1f), MinZoom, MaxZoom);
            const XMFLOAT2 after = CanvasToScreen(before.x, before.y);
            layout_->canvas.panX += pointer.x - after.x;
            layout_->canvas.panY += pointer.y - after.y;
            NotifyLayoutChanged();
        }
        if (inside && wi::input::Press(wi::input::MOUSE_BUTTON_MIDDLE))
        {
            panning_ = true;
            panPointerAnchor_ = XMFLOAT2(pointer.x, pointer.y);
            panValueAnchor_ = XMFLOAT2(layout_->canvas.panX, layout_->canvas.panY);
        }
        if (panning_ && wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE))
        {
            layout_->canvas.panX = panValueAnchor_.x + pointer.x - panPointerAnchor_.x;
            layout_->canvas.panY = panValueAnchor_.y + pointer.y - panPointerAnchor_.y;
            NotifyLayoutChanged();
        }
        else if (panning_) panning_ = false;
        if (!inside || !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) return;
        for (const auto& node : model_->Nodes())
        {
            const auto* pos = FindLayout(node.id);
            if (pos && Contains(NodeScreenBounds(*pos), pointer)) { SelectNode(node.id); return; }
        }
        SelectNode({});
    }

    void RenegadeStoryFlowWorkspace::Render(const wi::Canvas& canvas,
        wi::graphics::CommandList cmd) const
    {
        if (!IsVisible()) return;
        ApplyScissor(canvas, scissorRect, cmd);
        Rect(translation.x, translation.y, scale.x, scale.y, Surface, cmd);
        Rect(translation.x, translation.y, scale.x, HeaderHeight, Raised, cmd);
        Label("STORY FLOW", translation.x + 18, translation.y + 13, 14, Text, cmd);
        Label("READ-ONLY FOUNDATION", translation.x + 130, translation.y + 16, 9, Muted, cmd);
        if (!model_ || !layout_) return;
        for (const auto& route : model_->Routes())
        {
            const auto* a = FindLayout(route.sourceNodeId); const auto* b = FindLayout(route.destinationNodeId);
            if (!a || !b) continue;
            const XMFLOAT4 ab = NodeScreenBounds(*a); const XMFLOAT4 bb = NodeScreenBounds(*b);
            Line(XMFLOAT2(ab.x + ab.z, ab.y + ab.w * 0.5f), XMFLOAT2(bb.x, bb.y + bb.w * 0.5f),
                route.sourceNodeId == selectedNodeId_ || route.destinationNodeId == selectedNodeId_ ? Accent : Route);
        }
        for (const auto& node : model_->Nodes())
        {
            const auto* pos = FindLayout(node.id); if (!pos) continue;
            const XMFLOAT4 b = NodeScreenBounds(*pos);
            const bool selected = node.id == selectedNodeId_;
            const bool start = node.kind == bridge::FlowNodeKind::GameStart;
            Rect(b.x, b.y, b.z, b.w, selected ? wi::Color(28,20,16,255) : Raised, cmd);
            Rect(b.x, b.y, b.z, 1.0f, selected || start ? Accent : Border, cmd);
            Rect(b.x, b.y + b.w - 1.0f, b.z, 1.0f, selected || start ? Accent : Border, cmd);
            Rect(b.x, b.y, 1.0f, b.w, selected || start ? Accent : Border, cmd);
            Rect(b.x + b.z - 1.0f, b.y, 1.0f, b.w, selected || start ? Accent : Border, cmd);
            if (layout_->canvas.zoom >= 0.45f)
            {
                Label(KindLabel(node.kind), b.x + 10, b.y + 10, 8, start ? Accent : Muted, cmd);
                Label(node.name, b.x + 10, b.y + 31, 11, Text, cmd);
            }
        }
    }
}
