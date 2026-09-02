#include "RenegadeStudioChrome.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <utility>

namespace
{
    constexpr float TopBarHeight = 64.0f;
    constexpr float SceneTabsHeight = 34.0f;
    constexpr float BottomTabsHeight = 32.0f;
    constexpr float StatusBarHeight = 28.0f;
    constexpr float PanelHeaderHeight = 43.0f;
    constexpr float HierarchyRowHeight = 28.0f;
    constexpr float AssetFolderRowHeight = 24.0f;
    constexpr float AssetCardWidth = 148.0f;
    constexpr float AssetCardHeight = 112.0f;
    constexpr float AssetCardGap = 10.0f;

    // Scene UI Gate 3 typography. Creator-critical Scene chrome no longer
    // falls below 10 px; hierarchy entity names are given the strongest
    // readable treatment while compact secondary shell text remains 10 px.
    constexpr int SceneChromeSmallTextSize = 10;
    constexpr int SceneChromeStandardTextSize = 11;
    constexpr int SceneHierarchyEntityTextSize = 12;

    std::size_t HierarchyRowCapacity(const float height) noexcept
    {
        const float searchY = TopBarHeight + PanelHeaderHeight + 10.0f;
        const float rowsTop = searchY + 42.0f;
        const float rowsBottom = height - StatusBarHeight - 8.0f;
        return static_cast<std::size_t>(std::max(
            1.0f,
            std::floor((rowsBottom - rowsTop) / HierarchyRowHeight)));
    }

    std::size_t HierarchyTextCapacity(
        const float hierarchyWidth,
        const float textX) noexcept
    {
        constexpr float RightSideGutter = 36.0f;
        constexpr float ApproximateGlyphWidth =
            static_cast<float>(SceneHierarchyEntityTextSize) * 0.72f;
        const float available = std::max(
            24.0f,
            hierarchyWidth - RightSideGutter - textX);
        return std::max<std::size_t>(
            3,
            static_cast<std::size_t>(
                std::floor(available / ApproximateGlyphWidth)));
    }

    using HierarchyCategory =
        renegade::studio::RenegadeStudioChrome::HierarchyCategory;

    constexpr std::array<HierarchyCategory, 8> HierarchyCategoryOrder = {
        HierarchyCategory::Lights,
        HierarchyCategory::Models,
        HierarchyCategory::Characters,
        HierarchyCategory::Cameras,
        HierarchyCategory::Terrain,
        HierarchyCategory::Effects,
        HierarchyCategory::Audio,
        HierarchyCategory::Other,
    };

    const char* HierarchyCategoryLabel(
        const HierarchyCategory category) noexcept
    {
        switch (category)
        {
        case HierarchyCategory::Lights:
            return "LIGHTS";
        case HierarchyCategory::Models:
            return "MODELS";
        case HierarchyCategory::Characters:
            return "CHARACTERS";
        case HierarchyCategory::Cameras:
            return "CAMERAS";
        case HierarchyCategory::Terrain:
            return "TERRAIN";
        case HierarchyCategory::Effects:
            return "EFFECTS";
        case HierarchyCategory::Audio:
            return "AUDIO";
        case HierarchyCategory::Other:
        default:
            return "SCENE OBJECTS";
        }
    }

    constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
    constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
    constexpr wi::Color Text = wi::Color(214, 214, 214, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color TextSecondary = wi::Color(226, 226, 226, 255);
    constexpr wi::Color Muted = wi::Color(142, 151, 156, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
    constexpr wi::Color HoverEdge = wi::Color(72, 78, 82, 255);
    constexpr wi::Color Success = wi::Color(76, 195, 138, 255);

    void DrawRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawText(
        const std::string& text,
        const float x,
        const float y,
        const int size,
        const wi::Color color,
        const wi::graphics::CommandList cmd,
        const float tracking = 0.0f,
        const float bolden = 0.12f)
    {
        wi::font::Params params(
            x,
            y,
            size,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.spacingX = tracking;
        params.bolden = bolden;
        wi::font::Draw(text, params, cmd);
    }

    void DrawBorderedRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color fill,
        const wi::Color border,
        const wi::graphics::CommandList cmd)
    {
        DrawRect(x, y, width, height, border, cmd);
        DrawRect(x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f, fill, cmd);
    }

    void DrawRoundedRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const float radius,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        params.enableCornerRounding();
        for (auto& corner : params.corners_rounding)
        {
            corner.radius = radius;
            corner.segments = 8;
        }
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawDiamond(
        const float x,
        const float y,
        const float size,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::image::Params params;
        params.pos = XMFLOAT3(x, y, 0.0f);
        params.siz = XMFLOAT2(size, size);
        params.pivot = XMFLOAT2(0.5f, 0.5f);
        params.rotation = XM_PIDIV4;
        params.color = color;
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    std::string Ellipsize(std::string value, const std::size_t maximum)
    {
        if (value.size() <= maximum)
        {
            return value;
        }
        if (maximum <= 3)
        {
            return value.substr(0, maximum);
        }
        value.resize(maximum - 3);
        value += "...";
        return value;
    }

    std::vector<std::string> WrapTextLines(
        const std::string& value,
        const std::size_t maximumCharacters)
    {
        std::vector<std::string> lines;
        if (value.empty())
        {
            return lines;
        }

        const std::size_t limit = std::max<std::size_t>(1, maximumCharacters);
        std::size_t begin = 0;
        while (begin < value.size())
        {
            const std::size_t remaining = value.size() - begin;
            if (remaining <= limit)
            {
                lines.push_back(value.substr(begin));
                break;
            }

            const std::size_t hardEnd = begin + limit;
            std::size_t breakAt = value.rfind(' ', hardEnd);
            if (breakAt == std::string::npos || breakAt < begin)
            {
                breakAt = hardEnd;
            }
            lines.push_back(value.substr(begin, breakAt - begin));
            begin = breakAt;
            while (begin < value.size() && value[begin] == ' ')
            {
                ++begin;
            }
        }
        return lines;
    }

    std::size_t StatusTextCapacity(const float width) noexcept
    {
        // The FPS/ownership group starts at width - 402. Leave a 12 px gutter
        // before that divider and derive a conservative character budget for
        // the primary status text. The complete status string remains stored
        // and is also available in the Output drawer; only the single-line
        // footer summary is ellipsized.
        constexpr float StatusTextX = 105.0f;
        constexpr float RightGroupOffset = 402.0f;
        constexpr float SafetyGutter = 12.0f;
        constexpr float ApproximateGlyphWidth = 7.2f;
        const float available = std::max(
            24.0f,
            width - RightGroupOffset - SafetyGutter - StatusTextX);
        return std::max<std::size_t>(
            3,
            static_cast<std::size_t>(
                std::floor(available / ApproximateGlyphWidth)));
    }

    bool IsAssetDescendant(
        const std::string& path,
        const std::string& parent)
    {
        return path.size() > parent.size() &&
            path.compare(0, parent.size(), parent) == 0 &&
            path[parent.size()] == '/';
    }
}

namespace renegade::studio
{
    void RenegadeTextInputField::SetPlaceholder(std::string placeholder)
    {
        placeholder_ = std::move(placeholder);
    }

    void RenegadeTextInputField::SetRenderTextSize(const int size) noexcept
    {
        renderTextSize_ = std::clamp(size, 8, 18);
    }

    void RenegadeTextInputField::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }
        const wi::Color edge = state == wi::gui::ACTIVE
            ? Forge
            : state == wi::gui::FOCUS ? HoverEdge : Border;
        DrawBorderedRect(
            translation.x,
            translation.y,
            scale.x,
            scale.y,
            wi::Color(6, 10, 12, 248),
            IsEnabled() ? edge : BorderSoft,
            cmd);
        ApplyScissor(canvas, scissorRect, cmd);
        const std::string description = GetDescription();
        const std::string value = state == wi::gui::ACTIVE
            ? font_input.GetTextA()
            : font.GetTextA();
        if (!description.empty())
        {
            DrawText(
                description,
                translation.x + 8.0f,
                translation.y + std::max(4.0f,
                    (scale.y - static_cast<float>(renderTextSize_)) * 0.5f),
                renderTextSize_,
                TextStrong,
                cmd,
                0.2f,
                0.16f);
        }
        const float valueX = translation.x + 8.0f +
            (description.empty() ? 0.0f :
                static_cast<float>(description.size()) *
                    static_cast<float>(renderTextSize_) * 0.65f + 5.0f);
        if (!value.empty())
        {
            DrawText(
                value,
                valueX,
                translation.y + std::max(4.0f,
                    (scale.y - static_cast<float>(renderTextSize_)) * 0.5f),
                renderTextSize_,
                TextStrong,
                cmd,
                0.15f,
                0.14f);
        }
        else if (state != wi::gui::ACTIVE && !placeholder_.empty())
        {
            DrawText(
                placeholder_,
                valueX,
                translation.y + std::max(4.0f,
                    (scale.y - static_cast<float>(renderTextSize_)) * 0.5f),
                renderTextSize_,
                TextSecondary,
                cmd,
                0.35f,
                0.14f);
        }
    }

    void RenegadeButton::SetRenderTextSize(const int size) noexcept
    {
        renderTextSize_ = std::clamp(size, 8, 18);
    }

    void RenegadeButton::Render(
        const wi::Canvas&,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }
        const bool engaged =
            state == wi::gui::ACTIVE || state == wi::gui::FOCUS;
        DrawBorderedRect(
            translation.x,
            translation.y,
            scale.x,
            scale.y,
            engaged ? wi::Color(28, 20, 16, 255) : Surface2,
            engaged ? Forge : Border,
            cmd);
        const std::string text = GetText();
        const float textWidth = static_cast<float>(text.size()) *
            static_cast<float>(renderTextSize_) * 0.7f;
        DrawText(
            text,
            translation.x + std::max(8.0f, (scale.x - textWidth) * 0.5f),
            translation.y + std::max(4.0f,
                (scale.y - static_cast<float>(renderTextSize_)) * 0.5f),
            renderTextSize_,
            IsEnabled() ? TextStrong : Muted,
            cmd,
            0.25f,
            0.16f);
    }

    void RenegadeCheckBox::SetRenderTextSize(const int size) noexcept
    {
        renderTextSize_ = std::clamp(size, 8, 18);
    }

    void RenegadeCheckBox::Render(
        const wi::Canvas&,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }
        const float box = std::min(18.0f, scale.y - 4.0f);
        DrawBorderedRect(
            translation.x,
            translation.y + (scale.y - box) * 0.5f,
            box,
            box,
            wi::Color(6, 10, 12, 255),
            state == wi::gui::FOCUS ? HoverEdge : Border,
            cmd);
        if (checked)
        {
            DrawRect(
                translation.x + 4.0f,
                translation.y + (scale.y - box) * 0.5f + 4.0f,
                box - 8.0f,
                box - 8.0f,
                Forge,
                cmd);
        }
        DrawText(
            GetName(),
            translation.x + box + 9.0f,
            translation.y + std::max(4.0f,
                (scale.y - static_cast<float>(renderTextSize_)) * 0.5f - 1.0f),
            renderTextSize_,
            IsEnabled() ? TextStrong : Muted,
            cmd,
            0.25f,
            0.16f);
    }

    void RenegadeComboBox::SetRenderTextSize(const int size) noexcept
    {
        renderTextSize_ = std::clamp(size, 8, 18);
    }

    void RenegadeComboBox::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }
        const bool open = state == wi::gui::ACTIVE;
        DrawBorderedRect(
            translation.x,
            translation.y,
            scale.x,
            scale.y,
            wi::Color(6, 10, 12, 255),
            open || state == wi::gui::FOCUS ? Forge : Border,
            cmd);
        const std::string value = selected >= 0 &&
            selected < static_cast<int>(items.size())
            ? items[static_cast<std::size_t>(selected)].name
            : "SELECT...";
        DrawText(
            value,
            translation.x + 10.0f,
            translation.y + std::max(4.0f,
                (scale.y - static_cast<float>(renderTextSize_)) * 0.5f),
            renderTextSize_,
            TextStrong,
            cmd,
            0.35f,
            0.16f);
        DrawText(
            open ? "▲" : "▼",
            translation.x + scale.x - 21.0f,
            translation.y + std::max(4.0f,
                (scale.y - static_cast<float>(renderTextSize_)) * 0.5f),
            std::max(9, renderTextSize_ - 1),
            open ? Forge : Muted,
            cmd);
        if (!open)
        {
            return;
        }

        // Wicked's ComboBox interaction code owns a fixed 20 px item hitbox
        // plus its 20 px filter row. Render against those exact native
        // coordinates rather than stretching visible rows to this widget's
        // taller closed-control height. This keeps what the creator sees and
        // what the pointer can actually select in perfect agreement.
        constexpr float NativeDropItemHeight = 20.0f;
        const float dropWidth = fixed_drop_width > 0.0f
            ? fixed_drop_width
            : std::max(1.0f, scale.x - 1.0f - scale.y);
        const float dropX = GetDropX(canvas);
        const float dropOffset = GetDropOffset(canvas);
        const float filterY = translation.y + scale.y +
            dropOffset - NativeDropItemHeight;
        DrawBorderedRect(
            dropX,
            filterY,
            dropWidth,
            NativeDropItemHeight,
            Surface0,
            BorderSoft,
            cmd);
        DrawText(
            filterText.empty() ? "FILTER..." : filterText,
            dropX + 8.0f,
            filterY + 5.0f,
            std::max(9, renderTextSize_ - 1),
            filterText.empty() ? Muted : TextSecondary,
            cmd,
            0.2f,
            0.12f);

        int visibleItems = 0;
        for (int index = firstItemVisible;
            index < static_cast<int>(items.size()) &&
            visibleItems < maxVisibleItemCount;
            ++index)
        {
            if (!filterText.empty())
            {
                std::string upperName = items[static_cast<std::size_t>(index)].name;
                std::transform(
                    upperName.begin(), upperName.end(), upperName.begin(),
                    [](const unsigned char character)
                    {
                        return static_cast<char>(std::toupper(character));
                    });
                if (upperName.find(filterText) == std::string::npos)
                {
                    continue;
                }
            }
            ++visibleItems;
            const float y = translation.y + GetItemOffset(canvas, index);
            DrawBorderedRect(
                dropX,
                y,
                dropWidth,
                NativeDropItemHeight,
                index == hovered ? Surface2 : Surface0,
                index == hovered ? HoverEdge : BorderSoft,
                cmd);
            DrawText(
                items[static_cast<std::size_t>(index)].name,
                dropX + 8.0f,
                y + std::max(3.0f,
                    (NativeDropItemHeight -
                        static_cast<float>(renderTextSize_)) * 0.5f),
                renderTextSize_,
                index == hovered ? TextStrong : TextSecondary,
                cmd,
                0.2f,
                0.16f);
        }
    }

    void RenegadeSlider::Create(
        const float minimum,
        const float maximum,
        const float defaultValue,
        const float steps,
        const std::string& name,
        std::string label)
    {
        Slider::Create(
            minimum,
            maximum,
            defaultValue,
            steps,
            name);
        label_ = std::move(label);
        SetShadowRadius(0.0f);
        valueInputField.SetShadowRadius(0.0f);
        Slider::OnSlide([this](const wi::gui::EventArgs& args)
        {
            const bool mouseDrag =
                wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) &&
                valueInputField.GetState() != wi::gui::ACTIVE;
            if (!dragging_ && dragStarted_)
            {
                dragStarted_(valueBeforeUpdate_);
            }
            dragging_ = mouseDrag;
            if (valuePreview_)
            {
                valuePreview_(args.fValue);
            }
            if (!mouseDrag && valueCommitted_)
            {
                valueCommitted_(args.fValue);
            }
        });
    }

    void RenegadeSlider::SetRenderTextSizes(
        const int labelSize,
        const int valueSize) noexcept
    {
        labelTextSize_ = std::clamp(labelSize, 8, 18);
        valueTextSize_ = std::clamp(valueSize, 8, 18);
    }

    void RenegadeSlider::OnDragStarted(
        std::function<void(float)> callback)
    {
        dragStarted_ = std::move(callback);
    }

    void RenegadeSlider::OnValuePreview(
        std::function<void(float)> callback)
    {
        valuePreview_ = std::move(callback);
    }

    void RenegadeSlider::OnValueCommitted(
        std::function<void(float)> callback)
    {
        valueCommitted_ = std::move(callback);
    }

    void RenegadeSlider::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        valueBeforeUpdate_ = value;
        Slider::Update(canvas, dt);
        if (dragging_ && !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            dragging_ = false;
            if (valueCommitted_)
            {
                valueCommitted_(value);
            }
        }
    }

    void RenegadeSlider::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }

        ApplyScissor(canvas, scissorRect, cmd);
        const XMFLOAT2 inputPos = valueInputField.GetPos();
        const XMFLOAT2 inputSize = valueInputField.GetSize();
        const float trackX = translation.x + 3.0f;
        const float trackWidth = std::max(
            24.0f,
            inputPos.x - trackX - 9.0f);
        // Every continuous Studio control uses the same compact two-line
        // treatment: an unobstructed label above a short rail, with the
        // editable numeric field kept in its own right-hand column. This is
        // deliberately contained inside the widget's normal 28/34px row so
        // absolute-layout inspectors cannot overlap their following row.
        const float trackY = translation.y + scale.y - 9.0f;
        const float normalized = end > start
            ? std::clamp((value - start) / (end - start), 0.0f, 1.0f)
            : 0.0f;

        DrawText(
            label_,
            translation.x + 3.0f,
            translation.y + 3.0f,
            labelTextSize_,
            TextStrong,
            cmd,
            0.2f,
            0.18f);
        DrawRoundedRect(
            trackX,
            trackY,
            trackWidth,
            4.0f,
            2.0f,
            BorderSoft,
            cmd);
        DrawRoundedRect(
            trackX,
            trackY,
            trackWidth * normalized,
            4.0f,
            2.0f,
            Forge,
            cmd);
        const float knobX = trackX + trackWidth * normalized;
        const float knobY = trackY + 2.0f;
        DrawDiamond(knobX + 1.0f, knobY + 1.0f, 9.0f,
            wi::Color(0, 0, 0, 100), cmd);
        DrawDiamond(knobX, knobY, 8.0f,
            state == wi::gui::ACTIVE ? TextStrong : TextSecondary, cmd);
        DrawDiamond(knobX, knobY, 5.0f, Forge, cmd);
        DrawDiamond(knobX, knobY, 2.0f, TextStrong, cmd);

        const wi::Color inputBorder =
            valueInputField.GetState() == wi::gui::ACTIVE
                ? Forge
                : valueInputField.GetState() == wi::gui::FOCUS
                    ? HoverEdge
                    : Border;
        DrawBorderedRect(
            inputPos.x,
            inputPos.y,
            inputSize.x,
            inputSize.y,
            wi::Color(6, 10, 12, 255),
            inputBorder,
            cmd);
        DrawText(
            valueInputField.GetText(),
            inputPos.x + 7.0f,
            inputPos.y + std::max(4.0f,
                (inputSize.y - static_cast<float>(valueTextSize_)) * 0.5f - 1.0f),
            valueTextSize_,
            TextStrong,
            cmd,
            0.0f,
            0.18f);
    }

    void RenegadeTextureMapList::ClearSlots()
    {
        resources_ = {};
        paths_ = {};
    }

    void RenegadeTextureMapList::SetSlot(
        const std::size_t index,
        const wi::Resource& resource,
        std::string path)
    {
        if (index < resources_.size())
        {
            resources_[index] = resource;
            paths_[index] = std::move(path);
        }
    }

    void RenegadeTextureMapList::SetSelectedSlot(
        const std::size_t index)
    {
        selectedSlot_ = std::min(index, resources_.size() - 1);
    }

    void RenegadeTextureMapList::OnSlotSelected(
        std::function<void(std::size_t)> callback)
    {
        slotSelected_ = std::move(callback);
    }

    void RenegadeTextureMapList::OnBrowseRequested(
        std::function<void(std::size_t)> callback)
    {
        browseRequested_ = std::move(callback);
    }

    void RenegadeTextureMapList::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        Widget::Update(canvas, dt);
        hoveredSlot_ = SlotCount;
        if (!IsVisible() || !IsEnabled())
        {
            return;
        }
        const XMFLOAT4 pointer = wi::input::GetPointer();
        if (pointer.x < translation.x || pointer.x >= translation.x + scale.x ||
            pointer.y < translation.y || pointer.y >= translation.y + scale.y)
        {
            return;
        }
        const float rowHeight = scale.y / static_cast<float>(SlotCount);
        const std::size_t index = std::min(
            SlotCount - 1,
            static_cast<std::size_t>((pointer.y - translation.y) / rowHeight));
        hoveredSlot_ = index;
        SetTooltip(paths_[index].empty()
            ? std::string("No texture assigned")
            : paths_[index]);
        if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            return;
        }
        selectedSlot_ = index;
        if (slotSelected_)
        {
            slotSelected_(index);
        }
        if (pointer.x >= translation.x + scale.x - 30.0f && browseRequested_)
        {
            browseRequested_(index);
        }
    }

    void RenegadeTextureMapList::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }
        ApplyScissor(canvas, scissorRect, cmd);

        constexpr std::array<const char*, SlotCount> labels = {
            "BASE COLOR", "NORMAL", "SURFACE", "ROUGHNESS",
            "METALNESS", "AO", "EMISSIVE"};
        const float rowHeight = scale.y / static_cast<float>(SlotCount);
        const float thumbnail = std::max(18.0f, rowHeight - 17.0f);

        for (std::size_t index = 0; index < SlotCount; ++index)
        {
            const float y = translation.y + static_cast<float>(index) * rowHeight;
            DrawBorderedRect(
                translation.x,
                y,
                scale.x,
                rowHeight - 3.0f,
                wi::Color(6, 10, 12, 255),
                index == selectedSlot_
                    ? Forge
                    : index == hoveredSlot_ ? HoverEdge : Border,
                cmd);
            DrawText(
                labels[index],
                translation.x + 5.0f,
                y + 3.0f,
                8,
                index == selectedSlot_ ? TextStrong : TextSecondary,
                cmd,
                0.0f,
                0.14f);

            const auto& resource = resources_[index];
            if (resource.IsValid() && resource.GetTexture().IsValid())
            {
                wi::image::Params image(
                    translation.x + 5.0f,
                    y + 14.0f,
                    thumbnail,
                    thumbnail);
                image.blendFlag = wi::enums::BLENDMODE_ALPHA;
                image.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
                wi::image::Draw(&resource.GetTexture(), image, cmd);
            }
            else
            {
                DrawText(
                    "--",
                    translation.x + 10.0f,
                    y + 20.0f,
                    9,
                    Muted,
                    cmd);
            }

            const float pathX = translation.x + thumbnail + 12.0f;
            DrawBorderedRect(
                pathX,
                y + 14.0f,
                scale.x - thumbnail - 47.0f,
                thumbnail,
                Surface0,
                BorderSoft,
                cmd);
            std::string displayedPath = "<NONE>";
            if (!paths_[index].empty())
            {
                const std::filesystem::path path =
                    std::filesystem::u8path(paths_[index]);
                // The slot row is for identifying the assigned map, so keep
                // the filename readable instead of showing an unusable slice
                // of a long absolute path. Hovering the row and the editable
                // selected-slot field both expose the complete source path.
                displayedPath = path.filename().generic_u8string();
            }
            DrawText(
                Ellipsize(displayedPath, 36),
                pathX + 5.0f,
                y + 20.0f,
                8,
                paths_[index].empty() ? Muted : TextStrong,
                cmd);
            DrawBorderedRect(
                translation.x + scale.x - 30.0f,
                y + 14.0f,
                25.0f,
                thumbnail,
                Surface2,
                index == selectedSlot_ ? Forge : Border,
                cmd);
            DrawText(
                "...",
                translation.x + scale.x - 25.0f,
                y + 20.0f,
                9,
                TextStrong,
                cmd);
        }
    }

    void RenegadeStudioChrome::Create()
    {
        SetName("Renegade-owned Studio chrome");
        brandLockup_ = wi::resourcemanager::Load(
            "Content/ui/renegade-engine-fractured-crest-logo.png");
        SetLayout(width_, height_);
        SetShadowRadius(0.0f);
    }

    void RenegadeStudioChrome::SetLayout(
        const float width,
        const float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        if (!layoutInitialized_)
        {
            hierarchyWidth_ = width_ < 1350.0f ? 260.0f : 320.0f;
            inspectorWidth_ = width_ < 1350.0f ? 310.0f : 360.0f;
            layoutInitialized_ = true;
        }
        SetPanelSizes(hierarchyWidth_, inspectorWidth_, drawerHeight_);
        SetPos(XMFLOAT2(0.0f, 0.0f));
        SetSize(XMFLOAT2(width_, height_));
    }

    void RenegadeStudioChrome::SetHierarchyRows(
        std::vector<HierarchyRow> rows)
    {
        const auto selected = std::find_if(
            rows.begin(),
            rows.end(),
            [](const HierarchyRow& row)
            {
                return row.selected;
            });
        const std::uint64_t selectedEntity = selected != rows.end()
            ? selected->entity
            : 0;
        if (selectedEntity != 0 && selectedEntity != lastHierarchySelection_)
        {
            collapsedHierarchyCategories_[
                static_cast<std::size_t>(selected->category)] = false;
        }

        // Preserve disclosure state by stable scene entity while rows refresh.
        // Reusable Asset Instance roots are the one hierarchy product that is
        // intentionally collapsed on first sight: their generated mesh/material
        // descendants are useful when requested, but should not flood the scene
        // tree by default.
        std::unordered_set<std::uint64_t> parentEntities;
        for (std::size_t index = 0; index + 1 < rows.size(); ++index)
        {
            const auto& row = rows[index];
            const auto& next = rows[index + 1];
            if (row.entity == 0 || row.category != next.category ||
                next.depth <= row.depth)
            {
                continue;
            }
            parentEntities.insert(row.entity);
            if (row.name == "Reusable Asset Instance" &&
                initializedHierarchyDisclosureEntities_.insert(row.entity).second)
            {
                collapsedHierarchyEntities_.insert(row.entity);
            }
        }
        for (auto item = collapsedHierarchyEntities_.begin();
            item != collapsedHierarchyEntities_.end();)
        {
            item = parentEntities.count(*item) == 0
                ? collapsedHierarchyEntities_.erase(item)
                : std::next(item);
        }
        for (auto item = initializedHierarchyDisclosureEntities_.begin();
            item != initializedHierarchyDisclosureEntities_.end();)
        {
            item = parentEntities.count(*item) == 0
                ? initializedHierarchyDisclosureEntities_.erase(item)
                : std::next(item);
        }

        // A viewport-selected descendant must remain reachable. Open only its
        // ancestor chain; selecting the instance root itself does not expand it.
        if (selected != rows.end() && selectedEntity != lastHierarchySelection_ &&
            selected->depth > 0)
        {
            const std::size_t selectedIndex = static_cast<std::size_t>(
                std::distance(rows.begin(), selected));
            int ancestorDepth = selected->depth;
            for (std::size_t index = selectedIndex; index-- > 0 && ancestorDepth > 0;)
            {
                const auto& ancestor = rows[index];
                if (ancestor.category == selected->category &&
                    ancestor.depth < ancestorDepth)
                {
                    collapsedHierarchyEntities_.erase(ancestor.entity);
                    ancestorDepth = ancestor.depth;
                }
            }
        }

        lastHierarchySelection_ = selectedEntity;
        hierarchyRows_ = std::move(rows);
        SetHierarchyFilter(hierarchyFilter_);
    }

    bool RenegadeStudioChrome::HierarchyRowHasChildren(
        const std::size_t rowIndex) const noexcept
    {
        if (rowIndex + 1 >= hierarchyRows_.size())
        {
            return false;
        }
        const auto& row = hierarchyRows_[rowIndex];
        const auto& next = hierarchyRows_[rowIndex + 1];
        return row.category == next.category && next.depth > row.depth;
    }

    void RenegadeStudioChrome::SetAssetBrowserData(
        std::vector<AssetFolderRow> folders,
        std::vector<AssetCard> assets,
        std::string currentPath)
    {
        assetBrowserFolders_ = std::move(folders);
        assetBrowserAssets_ = std::move(assets);
        assetBrowserCurrentPath_ = std::move(currentPath);
        assetBrowserSelectedPath_.clear();
        assetBrowserFolderScrollRow_ = 0;
        assetBrowserAssetScrollRow_ = 0;

        for (const auto& folder : assetBrowserFolders_)
        {
            if (!folder.selected)
            {
                continue;
            }
            for (auto collapsed = collapsedAssetFolders_.begin();
                collapsed != collapsedAssetFolders_.end();)
            {
                if (folder.relativePath == *collapsed ||
                    IsAssetDescendant(folder.relativePath, *collapsed))
                {
                    collapsed = collapsedAssetFolders_.erase(collapsed);
                }
                else
                {
                    ++collapsed;
                }
            }
            break;
        }
        RebuildVisibleAssetFolders();
    }

    void RenegadeStudioChrome::SetAssetBrowserSelectedPath(
        std::string relativePath)
    {
        assetBrowserSelectedPath_ = std::move(relativePath);
    }

    void RenegadeStudioChrome::SetSceneName(std::string sceneName)
    {
        sceneName_ = std::move(sceneName);
    }

    void RenegadeStudioChrome::SetSceneDirty(const bool dirty) noexcept
    {
        sceneDirty_ = dirty;
    }

    void RenegadeStudioChrome::SetStatusText(std::string statusText)
    {
        statusText_ = std::move(statusText);
    }

    void RenegadeStudioChrome::SetSelectionName(std::string selectionName)
    {
        selectionName_ = std::move(selectionName);
    }

    void RenegadeStudioChrome::SetActiveTool(const int toolIndex) noexcept
    {
        activeTool_ = toolIndex;
    }

    void RenegadeStudioChrome::SetGridVisible(const bool visible) noexcept
    {
        gridVisible_ = visible;
    }

    void RenegadeStudioChrome::SetEnvironmentWorkspaceActive(
        const bool active) noexcept
    {
        environmentWorkspaceActive_ = active;
        if (active)
        {
            terrainWorkspaceActive_ = false;
            renderWorkspaceActive_ = false;
        }
    }

    void RenegadeStudioChrome::SetTerrainWorkspaceActive(
        const bool active) noexcept
    {
        terrainWorkspaceActive_ = active;
        if (active)
        {
            environmentWorkspaceActive_ = false;
            renderWorkspaceActive_ = false;
        }
    }

    void RenegadeStudioChrome::SetRenderWorkspaceActive(
        const bool active) noexcept
    {
        renderWorkspaceActive_ = active;
        if (active)
        {
            environmentWorkspaceActive_ = false;
            terrainWorkspaceActive_ = false;
        }
    }

    void RenegadeStudioChrome::SetTestLevelState(
        const TestLevelState state) noexcept
    {
        testLevelState_ = state;
    }

    void RenegadeStudioChrome::SetPanelSizes(
        const float hierarchyWidth,
        const float inspectorWidth,
        const float drawerHeight) noexcept
    {
        const float availableWidth = std::max(760.0f, width_);
        const float maximumHierarchy = std::max(
            220.0f,
            std::min(480.0f, availableWidth - 760.0f));
        hierarchyWidth_ = std::clamp(
            hierarchyWidth,
            220.0f,
            maximumHierarchy);
        const float maximumInspector = std::max(
            280.0f,
            std::min(
                520.0f,
                availableWidth - hierarchyWidth_ - 420.0f));
        inspectorWidth_ = std::clamp(
            inspectorWidth,
            280.0f,
            maximumInspector);
        // Keep enough vertical room for the browser toolbar plus one complete
        // thumbnail card. A shorter drawer looked open but clipped every card,
        // which made successfully imported assets appear to be missing.
        constexpr float MinimumDrawerHeight = 200.0f;
        const float maximumDrawer = std::max(
            MinimumDrawerHeight,
            height_ - TopBarHeight - SceneTabsHeight -
                BottomTabsHeight - StatusBarHeight - 180.0f);
        drawerHeight_ = std::clamp(
            drawerHeight,
            MinimumDrawerHeight,
            std::min(480.0f, maximumDrawer));
    }

    void RenegadeStudioChrome::SetActiveBottomTab(
        const int tab,
        const bool notify)
    {
        activeBottomTab_ = std::clamp(tab, -1, 3);
        if (notify && drawerChanged_)
        {
            drawerChanged_(activeBottomTab_);
        }
    }

    void RenegadeStudioChrome::SetHierarchyFilter(std::string filter)
    {
        std::transform(
            filter.begin(),
            filter.end(),
            filter.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        const bool filterChanged = filter != hierarchyFilter_;
        hierarchyFilter_ = std::move(filter);
        visibleHierarchyRows_.clear();
        for (const auto category : HierarchyCategoryOrder)
        {
            std::vector<std::size_t> matchingRows;
            for (std::size_t index = 0; index < hierarchyRows_.size(); ++index)
            {
                if (hierarchyRows_[index].category != category)
                {
                    continue;
                }
                std::string name = hierarchyRows_[index].name;
                std::transform(
                    name.begin(),
                    name.end(),
                    name.begin(),
                    [](const unsigned char character)
                    {
                        return static_cast<char>(std::tolower(character));
                    });
                if (hierarchyFilter_.empty() ||
                    name.find(hierarchyFilter_) != std::string::npos)
                {
                    matchingRows.push_back(index);
                }
            }
            if (matchingRows.empty())
            {
                continue;
            }

            visibleHierarchyRows_.push_back({true, category, 0});
            const auto categoryIndex = static_cast<std::size_t>(category);
            if (!hierarchyFilter_.empty() ||
                !collapsedHierarchyCategories_[categoryIndex])
            {
                int hiddenBelowDepth = -1;
                for (const auto rowIndex : matchingRows)
                {
                    const auto& row = hierarchyRows_[rowIndex];
                    if (hierarchyFilter_.empty() && hiddenBelowDepth >= 0)
                    {
                        if (row.depth > hiddenBelowDepth)
                        {
                            continue;
                        }
                        hiddenBelowDepth = -1;
                    }

                    visibleHierarchyRows_.push_back(
                        {false, category, rowIndex});
                    if (hierarchyFilter_.empty() &&
                        HierarchyRowHasChildren(rowIndex) &&
                        collapsedHierarchyEntities_.count(row.entity) != 0)
                    {
                        hiddenBelowDepth = row.depth;
                    }
                }
            }
        }

        const std::size_t capacity = HierarchyRowCapacity(height_);
        const std::size_t maximumScroll = visibleHierarchyRows_.size() > capacity
            ? visibleHierarchyRows_.size() - capacity
            : 0;
        hierarchyScrollRow_ = filterChanged
            ? 0
            : std::min(hierarchyScrollRow_, maximumScroll);

        // A newly created or viewport-selected entity must be revealed even
        // when its alphabetic position is below the first screenful.
        const auto selected = std::find_if(
            visibleHierarchyRows_.begin(),
            visibleHierarchyRows_.end(),
            [this](const VisibleHierarchyItem& item)
            {
                return !item.header &&
                    hierarchyRows_[item.rowIndex].selected;
            });
        if (selected != visibleHierarchyRows_.end())
        {
            const std::size_t selectedRow = static_cast<std::size_t>(
                std::distance(visibleHierarchyRows_.begin(), selected));
            if (selectedRow < hierarchyScrollRow_)
            {
                hierarchyScrollRow_ = selectedRow;
            }
            else if (selectedRow >= hierarchyScrollRow_ + capacity)
            {
                hierarchyScrollRow_ = selectedRow - capacity + 1;
            }
        }
    }

    void RenegadeStudioChrome::RebuildVisibleAssetFolders()
    {
        visibleAssetFolderRows_.clear();
        for (std::size_t index = 0;
            index < assetBrowserFolders_.size();
            ++index)
        {
            const auto& folder = assetBrowserFolders_[index];
            bool hidden = false;
            for (const auto& collapsed : collapsedAssetFolders_)
            {
                if (IsAssetDescendant(folder.relativePath, collapsed))
                {
                    hidden = true;
                    break;
                }
            }
            if (!hidden)
            {
                visibleAssetFolderRows_.push_back(index);
            }
        }
        assetBrowserFolderScrollRow_ = std::min(
            assetBrowserFolderScrollRow_,
            visibleAssetFolderRows_.empty()
                ? std::size_t(0)
                : visibleAssetFolderRows_.size() - 1);
    }

    void RenegadeStudioChrome::OnHierarchySelected(
        std::function<void(std::uint64_t)> callback)
    {
        hierarchySelected_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnToolSelected(
        std::function<void(int)> callback)
    {
        toolSelected_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnAction(
        std::function<void(Action)> callback)
    {
        action_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnDrawerChanged(
        std::function<void(int)> callback)
    {
        drawerChanged_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnAssetBrowserFolderSelected(
        std::function<void(const std::string&)> callback)
    {
        assetBrowserFolderSelected_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnAssetBrowserItemSelected(
        std::function<void(const std::string&)> callback)
    {
        assetBrowserItemSelected_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnAssetBrowserItemDropped(
        std::function<void(const std::string&, float, float)> callback)
    {
        assetBrowserItemDropped_ = std::move(callback);
    }

    void RenegadeStudioChrome::OnLayoutChanged(
        std::function<void(float, float, float, bool)> callback)
    {
        layoutChanged_ = std::move(callback);
    }

    XMFLOAT4 RenegadeStudioChrome::ViewportBounds() const noexcept
    {
        return XMFLOAT4(
            hierarchyWidth_,
            TopBarHeight + SceneTabsHeight,
            width_ - inspectorWidth_,
            height_ - BottomTabsHeight - StatusBarHeight -
                (activeBottomTab_ >= 0 ? drawerHeight_ : 0.0f));
    }

    float RenegadeStudioChrome::HierarchyWidth() const noexcept
    {
        return hierarchyWidth_;
    }

    float RenegadeStudioChrome::InspectorWidth() const noexcept
    {
        return inspectorWidth_;
    }

    float RenegadeStudioChrome::DrawerHeight() const noexcept
    {
        return drawerHeight_;
    }

    bool RenegadeStudioChrome::ConsumedPointerThisFrame() const noexcept
    {
        return pointerConsumed_;
    }

    void RenegadeStudioChrome::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        Widget::Update(canvas, dt);
        if (dt > 0.0f)
        {
            fpsSampleTime_ += dt;
            ++fpsSampleFrames_;
            if (fpsSampleTime_ >= 0.25f)
            {
                displayedFps_ = static_cast<float>(fpsSampleFrames_) /
                    fpsSampleTime_;
                fpsSampleTime_ = 0.0f;
                fpsSampleFrames_ = 0;
            }
        }
        pointerConsumed_ = false;
        const XMFLOAT4 layoutPointer = wi::input::GetPointer();
        const float inspectorEdge = width_ - inspectorWidth_;
        const float bottomTabsEdge =
            height_ - BottomTabsHeight - StatusBarHeight;
        const float drawerEdge = bottomTabsEdge - drawerHeight_;
        hoveredSplitter_ = 0;
        if (layoutPointer.y >= TopBarHeight &&
            layoutPointer.y < height_ - StatusBarHeight)
        {
            if (std::abs(layoutPointer.x - hierarchyWidth_) <= 4.0f)
            {
                hoveredSplitter_ = 1;
            }
            else if (std::abs(layoutPointer.x - inspectorEdge) <= 4.0f)
            {
                hoveredSplitter_ = 2;
            }
        }
        if (activeBottomTab_ >= 0 &&
            layoutPointer.x > hierarchyWidth_ &&
            layoutPointer.x < inspectorEdge &&
            std::abs(layoutPointer.y - drawerEdge) <= 4.0f)
        {
            hoveredSplitter_ = 3;
        }

        if (resizingPanel_ != 0)
        {
            const int activeSplitter = resizingPanel_;
            if (wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
            {
                if (resizingPanel_ == 1)
                {
                    SetPanelSizes(
                        layoutPointer.x,
                        inspectorWidth_,
                        drawerHeight_);
                }
                else if (resizingPanel_ == 2)
                {
                    SetPanelSizes(
                        hierarchyWidth_,
                        width_ - layoutPointer.x,
                        drawerHeight_);
                }
                else
                {
                    SetPanelSizes(
                        hierarchyWidth_,
                        inspectorWidth_,
                        bottomTabsEdge - layoutPointer.y);
                }
                if (layoutChanged_)
                {
                    layoutChanged_(
                        hierarchyWidth_,
                        inspectorWidth_,
                        drawerHeight_,
                        false);
                }
            }
            else
            {
                if (layoutChanged_)
                {
                    layoutChanged_(
                        hierarchyWidth_,
                        inspectorWidth_,
                        drawerHeight_,
                        true);
                }
                resizingPanel_ = 0;
            }
            wi::input::SetCursor(
                activeSplitter == 3
                    ? wi::input::CURSOR_RESIZE_NS
                    : wi::input::CURSOR_RESIZE_EW);
            pointerConsumed_ = true;
            hitBox = wi::primitive::Hitbox2D(
                XMFLOAT2(-1.0f, -1.0f),
                XMFLOAT2(0.0f, 0.0f));
            return;
        }
        if (hoveredSplitter_ != 0)
        {
            wi::input::SetCursor(
                hoveredSplitter_ == 3
                    ? wi::input::CURSOR_RESIZE_NS
                    : wi::input::CURSOR_RESIZE_EW);
            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                resizingPanel_ = hoveredSplitter_;
                pointerConsumed_ = true;
                hitBox = wi::primitive::Hitbox2D(
                    XMFLOAT2(-1.0f, -1.0f),
                    XMFLOAT2(0.0f, 0.0f));
                return;
            }
        }

        const float hierarchySearchY =
            TopBarHeight + PanelHeaderHeight + 10.0f;
        const float hierarchyRowsTop = hierarchySearchY + 42.0f;
        const float hierarchyRowsBottom = height_ - StatusBarHeight - 8.0f;
        const std::size_t hierarchyCapacity = HierarchyRowCapacity(height_);
        const std::size_t hierarchyMaximumScroll =
            visibleHierarchyRows_.size() > hierarchyCapacity
                ? visibleHierarchyRows_.size() - hierarchyCapacity
                : 0;

        // The visual hierarchy thumb is intentionally narrow, but its drag hit
        // target is wider. It stays clear of the panel-resize splitter and owns
        // the pointer for the complete LMB gesture so row selection cannot fire
        // underneath a scrollbar drag.
        if (hierarchyMaximumScroll > 0)
        {
            const float trackHeight = hierarchyRowsBottom - hierarchyRowsTop;
            const float thumbHeight = std::max(
                24.0f,
                trackHeight * static_cast<float>(hierarchyCapacity) /
                    static_cast<float>(visibleHierarchyRows_.size()));
            const float thumbY = hierarchyRowsTop +
                (trackHeight - thumbHeight) *
                    static_cast<float>(hierarchyScrollRow_) /
                    static_cast<float>(hierarchyMaximumScroll);
            constexpr float scrollbarHitWidth = 10.0f;
            const float scrollbarHitLeft = hierarchyWidth_ - 16.0f;
            const float scrollbarHitRight =
                scrollbarHitLeft + scrollbarHitWidth;

            if (hierarchyScrollbarDragging_)
            {
                if (wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
                {
                    const float travel = std::max(1.0f,
                        trackHeight - thumbHeight);
                    const float localY = std::clamp(
                        layoutPointer.y - hierarchyScrollbarDragOffsetY_ -
                            hierarchyRowsTop,
                        0.0f,
                        travel);
                    hierarchyScrollRow_ = std::min(
                        hierarchyMaximumScroll,
                        static_cast<std::size_t>(std::lround(
                            localY / travel *
                            static_cast<float>(hierarchyMaximumScroll))));
                    wi::input::SetCursor(wi::input::CURSOR_HAND);
                    pointerConsumed_ = true;
                    hitBox = wi::primitive::Hitbox2D(
                        XMFLOAT2(-1.0f, -1.0f),
                        XMFLOAT2(0.0f, 0.0f));
                    return;
                }
                hierarchyScrollbarDragging_ = false;
                pointerConsumed_ = true;
                hitBox = wi::primitive::Hitbox2D(
                    XMFLOAT2(-1.0f, -1.0f),
                    XMFLOAT2(0.0f, 0.0f));
                return;
            }

            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
                layoutPointer.x >= scrollbarHitLeft &&
                layoutPointer.x < scrollbarHitRight &&
                layoutPointer.y >= thumbY &&
                layoutPointer.y < thumbY + thumbHeight)
            {
                hierarchyScrollbarDragging_ = true;
                hierarchyScrollbarDragOffsetY_ = layoutPointer.y - thumbY;
                wi::input::SetCursor(wi::input::CURSOR_HAND);
                pointerConsumed_ = true;
                hitBox = wi::primitive::Hitbox2D(
                    XMFLOAT2(-1.0f, -1.0f),
                    XMFLOAT2(0.0f, 0.0f));
                return;
            }
        }
        else
        {
            hierarchyScrollbarDragging_ = false;
        }

        if (layoutPointer.x >= 8.0f &&
            layoutPointer.x < hierarchyWidth_ - 8.0f &&
            layoutPointer.y >= hierarchyRowsTop &&
            layoutPointer.y < hierarchyRowsBottom &&
            std::abs(layoutPointer.z) > 0.1f)
        {
            if (layoutPointer.z > 0.0f)
            {
                hierarchyScrollRow_ = hierarchyScrollRow_ > 0
                    ? hierarchyScrollRow_ - 1
                    : 0;
            }
            else
            {
                hierarchyScrollRow_ = std::min(
                    hierarchyScrollRow_ + 1,
                    hierarchyMaximumScroll);
            }
            pointerConsumed_ = true;
        }

        if (activeBottomTab_ == 0 && std::abs(layoutPointer.z) > 0.1f)
        {
            const float drawerTop = bottomTabsEdge - drawerHeight_;
            const float bodyTop = drawerTop + 79.0f;
            const float bodyBottom = bottomTabsEdge - 6.0f;
            const float browserWidth = inspectorEdge - hierarchyWidth_;
            const float folderPaneWidth = assetBrowserFoldersVisible_
                ? std::clamp(browserWidth * 0.28f, 190.0f, 270.0f)
                : 28.0f;
            if (layoutPointer.y >= bodyTop &&
                layoutPointer.y < bodyBottom &&
                layoutPointer.x >= hierarchyWidth_ &&
                layoutPointer.x < inspectorEdge)
            {
                const bool up = layoutPointer.z > 0.0f;
                if (assetBrowserFoldersVisible_ &&
                    layoutPointer.x < hierarchyWidth_ + folderPaneWidth)
                {
                    const std::size_t capacity = static_cast<std::size_t>(
                        std::max(
                            1.0f,
                            std::floor(
                                (bodyBottom - bodyTop) /
                                AssetFolderRowHeight)));
                    const std::size_t maximum =
                        visibleAssetFolderRows_.size() > capacity
                            ? visibleAssetFolderRows_.size() - capacity
                            : 0;
                    assetBrowserFolderScrollRow_ = up
                        ? (assetBrowserFolderScrollRow_ > 0
                            ? assetBrowserFolderScrollRow_ - 1
                            : 0)
                        : std::min(
                            assetBrowserFolderScrollRow_ + 1,
                            maximum);
                }
                else
                {
                    const float gridX =
                        hierarchyWidth_ + folderPaneWidth + 12.0f;
                    const float gridWidth = inspectorEdge - gridX - 12.0f;
                    const int columns = std::max(
                        1,
                        static_cast<int>(
                            (gridWidth + AssetCardGap) /
                            (AssetCardWidth + AssetCardGap)));
                    const std::size_t rowCount =
                        (assetBrowserAssets_.size() +
                            static_cast<std::size_t>(columns) - 1) /
                        static_cast<std::size_t>(columns);
                    const std::size_t visibleRows =
                        static_cast<std::size_t>(std::max(
                            1.0f,
                            std::floor(
                                (bodyBottom - bodyTop) /
                                (AssetCardHeight + AssetCardGap))));
                    const std::size_t maximum =
                        rowCount > visibleRows
                            ? rowCount - visibleRows
                            : 0;
                    assetBrowserAssetScrollRow_ = up
                        ? (assetBrowserAssetScrollRow_ > 0
                            ? assetBrowserAssetScrollRow_ - 1
                            : 0)
                        : std::min(
                            assetBrowserAssetScrollRow_ + 1,
                            maximum);
                }
                pointerConsumed_ = true;
            }
        }

        if (assetBrowserDragCandidate_)
        {
            const XMFLOAT4 dragPointer = wi::input::GetPointer();
            if (wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
            {
                const float dx = dragPointer.x - assetBrowserDragStart_.x;
                const float dy = dragPointer.y - assetBrowserDragStart_.y;
                if (!assetBrowserDragging_ && dx * dx + dy * dy >= 36.0f)
                    assetBrowserDragging_ = true;
                if (assetBrowserDragging_)
                {
                    wi::input::SetCursor(wi::input::CURSOR_HAND);
                    pointerConsumed_ = true;
                }
            }
            else if (wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
            {
                const bool dropped = assetBrowserDragging_;
                const std::string path = assetBrowserDragPath_;
                assetBrowserDragCandidate_ = false;
                assetBrowserDragging_ = false;
                assetBrowserDragPath_.clear();
                if (dropped && assetBrowserItemDropped_)
                    assetBrowserItemDropped_(
                        path, dragPointer.x, dragPointer.y);
                pointerConsumed_ = dropped;
            }
            else
            {
                assetBrowserDragCandidate_ = false;
                assetBrowserDragging_ = false;
                assetBrowserDragPath_.clear();
            }
        }
        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
        {
            activeMenu_ = -1;
            activeViewportMenu_ = -1;
            if (activeBottomTab_ >= 0)
            {
                SetActiveBottomTab(-1, true);
            }
        }
        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            const XMFLOAT4 pointer = wi::input::GetPointer();
            const float x = pointer.x;
            const float y = pointer.y;
            const float viewportTop = TopBarHeight + SceneTabsHeight;
            const float inspectorX = width_ - inspectorWidth_;
            const float bottomTabsTop =
                height_ - BottomTabsHeight - StatusBarHeight;
            const float drawerTop = bottomTabsTop - drawerHeight_;
            bool consumed = false;
            bool drawerTabInteraction = false;

            constexpr std::array<const char*, 6> menus = {
                "FILE", "EDIT", "ADD", "VIEW", "BUILD", "WINDOW"};
            std::array<float, 6> menuPositions = {};
            float menuX = 222.0f;
            for (std::size_t index = 0; index < menus.size(); ++index)
            {
                menuPositions[index] = menuX;
                const float menuWidth =
                    std::string(menus[index]).size() * 8.0f + 25.0f;
                if (x >= menuX - 8.0f && x < menuX + menuWidth - 8.0f &&
                    y >= 0.0f && y < TopBarHeight)
                {
                    activeMenu_ = activeMenu_ == static_cast<int>(index)
                        ? -1
                        : static_cast<int>(index);
                    activeViewportMenu_ = -1;
                    consumed = true;
                }
                menuX += menuWidth;
            }

            if (!consumed && activeMenu_ >= 0)
            {
                constexpr float popupWidth = 226.0f;
                constexpr float itemHeight = 30.0f;
                const float popupX = menuPositions[activeMenu_] - 8.0f;
                constexpr std::array<int, 6> popupItemCounts = {
                    5, 4, 10, 4, 2, 3};
                const int item = static_cast<int>(
                    (y - TopBarHeight) / itemHeight);
                const bool inPopup = x >= popupX &&
                    x < popupX + popupWidth && y >= TopBarHeight &&
                    y < TopBarHeight +
                        popupItemCounts[activeMenu_] * itemHeight;
                if (inPopup)
                {
                    const auto invoke = [this](const Action command)
                    {
                        if (action_)
                        {
                            action_(command);
                        }
                    };
                    if (activeMenu_ == 0 && item >= 0 && item < 5)
                    {
                        constexpr std::array<Action, 5> actions = {
                            Action::ProjectHub, Action::OpenScene,
                            Action::Save, Action::SaveAs, Action::Reopen};
                        invoke(actions[item]);
                    }
                    else if (activeMenu_ == 1 && item >= 0 && item < 4)
                    {
                        constexpr std::array<Action, 4> actions = {
                            Action::Undo, Action::Redo,
                            Action::Duplicate, Action::Delete};
                        invoke(actions[item]);
                    }
                    else if (activeMenu_ == 2 && item >= 0 && item < 10)
                    {
                        constexpr std::array<Action, 10> actions = {
                            Action::CreatePointLight,
                            Action::CreateSpotLight,
                            Action::CreateDirectionalLight,
                            Action::CreateRectangleLight,
                            Action::CreatePlayerStart,
                            Action::CreateGameplayScript,
                            Action::CreateCamera,
                            Action::CreateDecal,
                            Action::CreateEnvironmentProbe,
                            Action::ImportModel};
                        invoke(actions[item]);
                    }
                    else if (activeMenu_ == 3 && item >= 0 && item < 4)
                    {
                        if (item == 0)
                        {
                            invoke(Action::Focus);
                        }
                        else if (item == 1)
                        {
                            invoke(Action::ToggleGrid);
                        }
                        else
                        {
                            SetActiveBottomTab(item == 2 ? 0 : 3, true);
                        }
                    }
                    else if (activeMenu_ == 4 && item >= 0 && item < 2)
                    {
                        if (item == 0)
                        {
                            invoke(Action::BuildWindowsGame);
                        }
                        else
                        {
                            invoke(Action::ValidateModelImport);
                        }
                    }
                    else if (activeMenu_ == 5 && item >= 0 && item < 3)
                    {
                        if (item == 2)
                        {
                            SetActiveBottomTab(
                                activeBottomTab_ >= 0 ? -1 : 0,
                                true);
                        }
                    }
                    activeMenu_ = -1;
                    consumed = true;
                }
            }

            const float menuEnd = 580.0f;
            const float firstToolX = menuEnd + 13.0f;
            constexpr std::array<float, 4> widths = {
                82.0f, 102.0f, 82.0f, 82.0f};
            float toolX = firstToolX;
            for (int index = 0; index < 4; ++index)
            {
                if (x >= toolX && x < toolX + widths[index] &&
                    y >= 15.0f && y < 49.0f)
                {
                    if (toolSelected_)
                    {
                        toolSelected_(index);
                    }
                    activeMenu_ = -1;
                    activeViewportMenu_ = -1;
                    consumed = true;
                    break;
                }
                toolX += widths[index] + 5.0f;
            }

            const float sceneMetaX = width_ - 360.0f;
            if (!consumed &&
                x >= sceneMetaX - 108.0f && x < sceneMetaX - 8.0f &&
                y >= 15.0f && y < 49.0f)
            {
                if (action_)
                {
                    if (testLevelState_ == TestLevelState::Idle)
                    {
                        action_(Action::TestLevelPlay);
                    }
                    else if (testLevelState_ == TestLevelState::Running)
                    {
                        action_(Action::TestLevelStop);
                    }
                }
                activeMenu_ = -1;
                activeViewportMenu_ = -1;
                consumed = true;
            }
            if (!consumed && y >= 34.0f && y < 58.0f)
            {
                if (x >= sceneMetaX + 12.0f && x < sceneMetaX + 62.0f)
                {
                    if (action_)
                        action_(Action::SceneWorkspace);
                    consumed = true;
                }
                else if (x >= sceneMetaX + 64.0f && x < sceneMetaX + 162.0f)
                {
                    if (action_)
                        action_(Action::EnvironmentWorkspace);
                    consumed = true;
                }
                else if (x >= sceneMetaX + 164.0f && x < sceneMetaX + 224.0f)
                {
                    if (action_)
                        action_(Action::TerrainWorkspace);
                    consumed = true;
                }
                else if (x >= sceneMetaX + 226.0f && x < sceneMetaX + 286.0f)
                {
                    if (action_)
                        action_(Action::RenderWorkspace);
                    consumed = true;
                }
            }

            float chipX = hierarchyWidth_ + 12.0f;
            constexpr std::array<const char*, 3> chips = {
                "PERSPECTIVE", "LIT", "SHOW"};
            for (int index = 0; index < 3; ++index)
            {
                const float chipWidth =
                    std::string(chips[index]).size() * 7.5f + 24.0f;
                if (x >= chipX && x < chipX + chipWidth &&
                    y >= viewportTop + 10.0f && y < viewportTop + 38.0f)
                {
                    activeViewportMenu_ = activeViewportMenu_ == index
                        ? -1
                        : index;
                    activeMenu_ = -1;
                    consumed = true;
                }
                chipX += chipWidth + 6.0f;
            }
            if (!consumed && activeViewportMenu_ == 2 &&
                x >= hierarchyWidth_ + 12.0f &&
                x < hierarchyWidth_ + 225.0f &&
                y >= viewportTop + 42.0f && y < viewportTop + 72.0f)
            {
                if (action_)
                {
                    action_(Action::ToggleGrid);
                }
                activeViewportMenu_ = -1;
                consumed = true;
            }

            const float searchY = TopBarHeight + PanelHeaderHeight + 10.0f;
            const float rowsTop = searchY + 42.0f;
            const float rowsBottom = height_ - StatusBarHeight - 8.0f;
            if (x >= 8.0f && x < hierarchyWidth_ - 8.0f &&
                y >= rowsTop && y < rowsBottom)
            {
                const auto visibleIndex = hierarchyScrollRow_ +
                    static_cast<std::size_t>(
                        (y - rowsTop) / HierarchyRowHeight);
                if (visibleIndex < visibleHierarchyRows_.size())
                {
                    const auto item = visibleHierarchyRows_[visibleIndex];
                    if (item.header)
                    {
                        const auto categoryIndex =
                            static_cast<std::size_t>(item.category);
                        collapsedHierarchyCategories_[categoryIndex] =
                            !collapsedHierarchyCategories_[categoryIndex];
                        SetHierarchyFilter(hierarchyFilter_);
                    }
                    else
                    {
                        const auto& row = hierarchyRows_[item.rowIndex];
                        if (HierarchyRowHasChildren(item.rowIndex))
                        {
                            if (collapsedHierarchyEntities_.count(row.entity) != 0)
                            {
                                collapsedHierarchyEntities_.erase(row.entity);
                            }
                            else
                            {
                                collapsedHierarchyEntities_.insert(row.entity);
                            }
                            SetHierarchyFilter(hierarchyFilter_);
                        }
                        if (hierarchySelected_)
                        {
                            hierarchySelected_(row.entity);
                        }
                    }
                    consumed = true;
                }
            }

            if (x >= hierarchyWidth_ &&
                x < inspectorX &&
                y >= bottomTabsTop && y < bottomTabsTop + BottomTabsHeight)
            {
                if (x >= inspectorX - 34.0f)
                {
                    SetActiveBottomTab(
                        activeBottomTab_ >= 0 ? -1 : 0,
                        true);
                    consumed = true;
                    drawerTabInteraction = true;
                }
                constexpr std::array<const char*, 4> tabs = {
                    "ASSET BROWSER", "CONSOLE", "OUTPUT", "DIAGNOSTICS"};
                float tabX = hierarchyWidth_;
                for (int index = 0; !drawerTabInteraction && index < 4; ++index)
                {
                    const float tabWidth =
                        std::string(tabs[index]).size() * 7.2f + 31.0f;
                    if (x >= tabX && x < tabX + tabWidth)
                    {
                        SetActiveBottomTab(
                            activeBottomTab_ == index ? -1 : index,
                            true);
                        activeMenu_ = -1;
                        activeViewportMenu_ = -1;
                        consumed = true;
                        drawerTabInteraction = true;
                        break;
                    }
                    tabX += tabWidth;
                }
            }

            if (activeBottomTab_ >= 0 &&
                x >= inspectorX - 38.0f && x < inspectorX - 8.0f &&
                y >= drawerTop + 7.0f && y < drawerTop + 35.0f)
            {
                SetActiveBottomTab(-1, true);
                consumed = true;
                drawerTabInteraction = true;
            }

            if (!consumed && activeBottomTab_ == 0 &&
                x >= hierarchyWidth_ && x < inspectorX &&
                y >= drawerTop + 43.0f && y < bottomTabsTop)
            {
                const float browserWidth = inspectorX - hierarchyWidth_;
                const float folderPaneWidth = assetBrowserFoldersVisible_
                    ? std::clamp(browserWidth * 0.28f, 190.0f, 270.0f)
                    : 28.0f;
                const float bodyTop = drawerTop + 79.0f;
                const float bodyBottom = bottomTabsTop - 6.0f;

                if (x >= hierarchyWidth_ + 13.0f &&
                    x < hierarchyWidth_ + 35.0f &&
                    y >= drawerTop + 47.0f && y < drawerTop + 70.0f)
                {
                    assetBrowserFoldersVisible_ =
                        !assetBrowserFoldersVisible_;
                    consumed = true;
                }
                else if (assetBrowserFoldersVisible_ &&
                    x < hierarchyWidth_ + folderPaneWidth &&
                    y >= bodyTop && y < bodyBottom)
                {
                    const std::size_t visibleIndex =
                        assetBrowserFolderScrollRow_ +
                        static_cast<std::size_t>(
                            (y - bodyTop) / AssetFolderRowHeight);
                    if (visibleIndex < visibleAssetFolderRows_.size())
                    {
                        const std::size_t folderIndex =
                            visibleAssetFolderRows_[visibleIndex];
                        const auto& folder =
                            assetBrowserFolders_[folderIndex];
                        const float arrowEdge =
                            hierarchyWidth_ + 18.0f +
                            std::max(0, folder.depth) * 14.0f;
                        if (x < arrowEdge + 15.0f)
                        {
                            if (collapsedAssetFolders_.count(
                                    folder.relativePath) != 0)
                            {
                                collapsedAssetFolders_.erase(
                                    folder.relativePath);
                            }
                            else
                            {
                                collapsedAssetFolders_.insert(
                                    folder.relativePath);
                            }
                            RebuildVisibleAssetFolders();
                        }
                        else if (assetBrowserFolderSelected_)
                        {
                            assetBrowserFolderSelected_(
                                folder.relativePath);
                        }
                        consumed = true;
                    }
                }
                else if (y >= bodyTop && y < bodyBottom)
                {
                    const float gridX =
                        hierarchyWidth_ + folderPaneWidth + 12.0f;
                    const float gridWidth = inspectorX - gridX - 12.0f;
                    const int columns = std::max(
                        1,
                        static_cast<int>(
                            (gridWidth + AssetCardGap) /
                            (AssetCardWidth + AssetCardGap)));
                    if (x >= gridX)
                    {
                        const int column = static_cast<int>(
                            (x - gridX) /
                            (AssetCardWidth + AssetCardGap));
                        const int visibleRow = static_cast<int>(
                            (y - bodyTop) /
                            (AssetCardHeight + AssetCardGap));
                        if (column >= 0 && column < columns &&
                            visibleRow >= 0)
                        {
                            const std::size_t assetIndex =
                                (assetBrowserAssetScrollRow_ +
                                    static_cast<std::size_t>(visibleRow)) *
                                    static_cast<std::size_t>(columns) +
                                static_cast<std::size_t>(column);
                            if (assetIndex < assetBrowserAssets_.size())
                            {
                                const auto& asset =
                                    assetBrowserAssets_[assetIndex];
                                assetBrowserSelectedPath_ =
                                    asset.relativePath;
                                if (!asset.directory)
                                {
                                    assetBrowserDragCandidate_ = true;
                                    assetBrowserDragging_ = false;
                                    assetBrowserDragPath_ = asset.relativePath;
                                    assetBrowserDragStart_ = XMFLOAT2(x, y);
                                }
                                if (asset.directory &&
                                    assetBrowserFolderSelected_)
                                {
                                    assetBrowserFolderSelected_(
                                        asset.relativePath);
                                }
                                else if (assetBrowserItemSelected_)
                                {
                                    assetBrowserItemSelected_(
                                        asset.relativePath);
                                }
                                consumed = true;
                            }
                        }
                    }
                }
            }

            const bool insideDrawer = activeBottomTab_ >= 0 &&
                x >= hierarchyWidth_ && x < inspectorX &&
                y >= drawerTop && y < bottomTabsTop;
            if (!insideDrawer && !drawerTabInteraction &&
                activeBottomTab_ >= 0)
            {
                SetActiveBottomTab(-1, true);
            }
            if (!consumed)
            {
                activeMenu_ = -1;
                activeViewportMenu_ = -1;
            }
            pointerConsumed_ = consumed || drawerTabInteraction ||
                insideDrawer;
        }
        hitBox = wi::primitive::Hitbox2D(
            XMFLOAT2(-1.0f, -1.0f),
            XMFLOAT2(0.0f, 0.0f));
    }

    void RenegadeStudioChrome::RenderAssetBrowser(
        const float drawerTop,
        const float inspectorX,
        const wi::graphics::CommandList cmd) const
    {
        const float browserWidth = inspectorX - hierarchyWidth_;
        const float toolbarY = drawerTop + 46.0f;
        const float bodyTop = drawerTop + 79.0f;
        const float bodyBottom =
            height_ - BottomTabsHeight - StatusBarHeight - 6.0f;
        const float folderPaneWidth = assetBrowserFoldersVisible_
            ? std::clamp(browserWidth * 0.28f, 190.0f, 270.0f)
            : 28.0f;

        DrawBorderedRect(
            hierarchyWidth_ + 13.0f,
            toolbarY,
            22.0f,
            23.0f,
            Surface2,
            Border,
            cmd);
        DrawText(
            assetBrowserFoldersVisible_ ? "<" : ">",
            hierarchyWidth_ + 20.0f,
            toolbarY + 7.0f,
            9,
            TextStrong,
            cmd);

        const float statusWidth = 82.0f;
        const float searchWidth = std::clamp(
            browserWidth * 0.22f,
            150.0f,
            260.0f);
        const float searchX =
            inspectorX - searchWidth - statusWidth - 26.0f;
        DrawBorderedRect(
            hierarchyWidth_ + 45.0f,
            toolbarY,
            std::max(120.0f, searchX - hierarchyWidth_ - 55.0f),
            23.0f,
            wi::Color(6, 10, 12, 255),
            Border,
            cmd);
        DrawText(
            Ellipsize(assetBrowserCurrentPath_, 64),
            hierarchyWidth_ + 55.0f,
            toolbarY + 7.0f,
            9,
            TextSecondary,
            cmd,
            0.2f,
            0.16f);

        DrawBorderedRect(
            searchX,
            toolbarY,
            searchWidth,
            23.0f,
            wi::Color(6, 10, 12, 255),
            Border,
            cmd);
        DrawText(
            "SEARCH // NEXT SLICE",
            searchX + 9.0f,
            toolbarY + 7.0f,
            9,
            Muted,
            cmd,
            0.2f,
            0.12f);

        DrawBorderedRect(
            inspectorX - statusWidth - 13.0f,
            toolbarY,
            statusWidth,
            23.0f,
            Surface2,
            Border,
            cmd);
        DrawText(
            "LOCAL CONTENT",
            inspectorX - statusWidth - 7.0f,
            toolbarY + 7.0f,
            8,
            TextSecondary,
            cmd,
            0.2f,
            0.16f);

        if (assetBrowserFoldersVisible_)
        {
            DrawRect(
                hierarchyWidth_,
                bodyTop,
                folderPaneWidth,
                bodyBottom - bodyTop,
                wi::Color(6, 10, 12, 245),
                cmd);
            DrawRect(
                hierarchyWidth_ + folderPaneWidth - 1.0f,
                bodyTop,
                1.0f,
                bodyBottom - bodyTop,
                Border,
                cmd);

            const std::size_t capacity =
                static_cast<std::size_t>(std::max(
                    1.0f,
                    std::floor(
                        (bodyBottom - bodyTop) /
                        AssetFolderRowHeight)));
            const std::size_t end = std::min(
                visibleAssetFolderRows_.size(),
                assetBrowserFolderScrollRow_ + capacity);
            for (std::size_t visible = assetBrowserFolderScrollRow_;
                visible < end;
                ++visible)
            {
                const auto& folder = assetBrowserFolders_[
                    visibleAssetFolderRows_[visible]];
                const float y = bodyTop +
                    static_cast<float>(
                        visible - assetBrowserFolderScrollRow_) *
                        AssetFolderRowHeight;
                if (folder.selected)
                {
                    DrawRect(
                        hierarchyWidth_ + 5.0f,
                        y,
                        folderPaneWidth - 10.0f,
                        AssetFolderRowHeight,
                        wi::Color(67, 28, 13, 145),
                        cmd);
                    DrawRect(
                        hierarchyWidth_ + 5.0f,
                        y,
                        2.0f,
                        AssetFolderRowHeight,
                        Forge,
                        cmd);
                }

                const float indent =
                    hierarchyWidth_ + 14.0f +
                    std::max(0, folder.depth) * 14.0f;
                const bool collapsed =
                    collapsedAssetFolders_.count(
                        folder.relativePath) != 0;
                DrawText(
                    collapsed ? ">" : "v",
                    indent,
                    y + 7.0f,
                    8,
                    collapsed ? Muted : Forge,
                    cmd);
                DrawText(
                    Ellipsize(folder.name, 26),
                    indent + 17.0f,
                    y + 6.0f,
                    9,
                    folder.selected ? TextStrong : TextSecondary,
                    cmd,
                    0.1f,
                    0.15f);
            }
        }

        const float gridX =
            hierarchyWidth_ + folderPaneWidth + 12.0f;
        const float gridWidth = inspectorX - gridX - 12.0f;
        const int columns = std::max(
            1,
            static_cast<int>(
                (gridWidth + AssetCardGap) /
                (AssetCardWidth + AssetCardGap)));

        if (assetBrowserAssets_.empty())
        {
            DrawText(
                "THIS FOLDER IS EMPTY",
                gridX + 6.0f,
                bodyTop + 10.0f,
                10,
                Muted,
                cmd,
                0.8f,
                0.14f);
            return;
        }

        const std::size_t first =
            assetBrowserAssetScrollRow_ *
            static_cast<std::size_t>(columns);
        for (std::size_t index = first;
            index < assetBrowserAssets_.size();
            ++index)
        {
            const std::size_t local = index - first;
            const int row = static_cast<int>(
                local / static_cast<std::size_t>(columns));
            const int column = static_cast<int>(
                local % static_cast<std::size_t>(columns));
            const float x = gridX +
                column * (AssetCardWidth + AssetCardGap);
            const float y = bodyTop +
                row * (AssetCardHeight + AssetCardGap);
            if (y + AssetCardHeight > bodyBottom)
            {
                break;
            }

            const auto& asset = assetBrowserAssets_[index];
            const bool selected =
                asset.relativePath == assetBrowserSelectedPath_;
            DrawBorderedRect(
                x,
                y,
                AssetCardWidth,
                AssetCardHeight,
                selected
                    ? wi::Color(38, 22, 16, 255)
                    : Surface2,
                selected ? Forge : Border,
                cmd);
            DrawRect(
                x + 7.0f,
                y + 7.0f,
                AssetCardWidth - 14.0f,
                68.0f,
                wi::Color(8, 16, 21, 255),
                cmd);
            if (asset.thumbnail.IsValid())
            {
                wi::image::Params thumbnailParams(
                    x + 7.0f,
                    y + 7.0f,
                    AssetCardWidth - 14.0f,
                    68.0f);
                thumbnailParams.blendFlag = wi::enums::BLENDMODE_OPAQUE;
                wi::image::Draw(
                    &asset.thumbnail.GetTexture(),
                    thumbnailParams,
                    cmd);
            }
            if (!asset.thumbnail.IsValid())
            {
                DrawText(
                    asset.directory ? "DIR" : "FILE",
                    x + 12.0f,
                    y + 15.0f,
                    8,
                    asset.directory ? TextStrong : Forge,
                    cmd,
                    0.4f,
                    0.16f);
            }
            DrawText(
                Ellipsize(asset.name, 18),
                x + 8.0f,
                y + 82.0f,
                9,
                selected ? TextStrong : TextSecondary,
                cmd,
                0.1f,
                0.16f);
            DrawText(
                Ellipsize(asset.typeLabel, 22),
                x + 8.0f,
                y + 97.0f,
                7,
                Muted,
                cmd,
                0.2f,
                0.12f);
        }
    }

    void RenegadeStudioChrome::Render(
        const wi::Canvas&,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }

        const float viewportTop = TopBarHeight + SceneTabsHeight;
        const float inspectorX = width_ - inspectorWidth_;
        const float bottomTabsTop =
            height_ - BottomTabsHeight - StatusBarHeight;
        const float viewportBottom = bottomTabsTop -
            (activeBottomTab_ >= 0 ? drawerHeight_ : 0.0f);
        const float statusTop = height_ - StatusBarHeight;

        // App frame and low-noise industrial shell:
        DrawRect(0.0f, 0.0f, width_, TopBarHeight, Surface0, cmd);
        DrawRect(
            0.0f,
            TopBarHeight - 1.0f,
            width_,
            1.0f,
            Border,
            cmd);
        DrawRect(7.0f, 7.0f, width_ - 14.0f, 1.0f, BorderSoft, cmd);
        DrawRect(7.0f, height_ - 8.0f, width_ - 14.0f, 1.0f, BorderSoft, cmd);
        DrawRect(7.0f, 7.0f, 1.0f, height_ - 14.0f, BorderSoft, cmd);
        DrawRect(width_ - 8.0f, 7.0f, 1.0f, height_ - 14.0f, BorderSoft, cmd);

        // Owner-supplied transparent fractured-crest lockup. Preserve its
        // aspect ratio and alpha; the logo never inherits additive UI glow.
        if (brandLockup_.IsValid())
        {
            const auto desc = brandLockup_.GetTexture().GetDesc();
            constexpr float maxWidth = 168.0f;
            constexpr float maxHeight = 62.0f;
            float drawWidth = maxWidth;
            float drawHeight = maxHeight;
            if (desc.width > 0 && desc.height > 0)
            {
                const float aspect =
                    static_cast<float>(desc.width) /
                    static_cast<float>(desc.height);
                drawWidth = drawHeight * aspect;
                if (drawWidth > maxWidth)
                {
                    drawWidth = maxWidth;
                    drawHeight = drawWidth / aspect;
                }
            }
            wi::image::Params logo(
                18.0f,
                (TopBarHeight - drawHeight) * 0.5f,
                drawWidth,
                drawHeight);
            logo.blendFlag = wi::enums::BLENDMODE_ALPHA;
            logo.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            wi::image::Draw(&brandLockup_.GetTexture(), logo, cmd);
        }
        DrawRect(204.0f, 0.0f, 1.0f, TopBarHeight, BorderSoft, cmd);

        // Menus are deliberately quiet. Ember is reserved for active state.
        constexpr std::array<const char*, 6> menus = {
            "FILE", "EDIT", "ADD", "VIEW", "BUILD", "WINDOW"};
        float menuX = 222.0f;
        int menuIndex = 0;
        for (const char* menu : menus)
        {
            const bool active = activeMenu_ == menuIndex;
            DrawText(
                menu,
                menuX,
                25.0f,
                SceneChromeStandardTextSize,
                active ? TextStrong : TextSecondary,
                cmd,
                0.55f,
                0.16f);
            if (active)
            {
                DrawRect(
                    menuX - 8.0f,
                    TopBarHeight - 2.0f,
                    std::string(menu).size() * 8.0f + 17.0f,
                    2.0f,
                    Forge,
                    cmd);
            }
            menuX += std::string(menu).size() * 8.0f + 25.0f;
            ++menuIndex;
        }
        DrawRect(menuX, 0.0f, 1.0f, TopBarHeight, BorderSoft, cmd);

        // Transform tools use squared controls and a single state line.
        constexpr std::array<const char*, 4> tools = {
            "SELECT", "TRANSLATE", "ROTATE", "SCALE"};
        float toolX = menuX + 13.0f;
        for (int index = 0; index < static_cast<int>(tools.size()); ++index)
        {
            const float controlWidth = index == 1 ? 102.0f : 82.0f;
            const bool active = activeTool_ == index;
            if (active)
            {
                DrawRect(
                    toolX,
                    15.0f,
                    controlWidth,
                    34.0f,
                    wi::Color(16, 23, 28, 220),
                    cmd);
                DrawRect(
                    toolX,
                    47.0f,
                    controlWidth,
                    2.0f,
                    Forge,
                    cmd);
            }
            DrawText(
                tools[index],
                toolX + 11.0f,
                26.0f,
                SceneChromeSmallTextSize,
                active ? TextStrong : TextSecondary,
                cmd,
                0.45f,
                active ? 0.12f : 0.0f);
            toolX += controlWidth + 5.0f;
        }

        const float sceneMetaWidth = 360.0f;
        const float sceneMetaX = width_ - sceneMetaWidth;
        DrawRect(
            sceneMetaX - 116.0f,
            0.0f,
            1.0f,
            TopBarHeight,
            BorderSoft,
            cmd);
        const bool testLevelStarting =
            testLevelState_ == TestLevelState::Starting;
        const bool testLevelRunning =
            testLevelState_ == TestLevelState::Running;
        const char* transportGlyph = testLevelRunning
            ? "■"
            : testLevelStarting ? "…" : "▶";
        const char* transportLabel = testLevelRunning
            ? "STOP"
            : testLevelStarting ? "STARTING" : "PLAY";
        const wi::Color transportColor = testLevelStarting
            ? Muted
            : testLevelRunning ? Forge : Success;
        DrawText(
            transportGlyph,
            sceneMetaX - 96.0f,
            20.0f,
            16,
            transportColor,
            cmd,
            0.0f,
            0.1f);
        DrawText(
            transportLabel,
            sceneMetaX - 70.0f,
            26.0f,
            SceneChromeSmallTextSize,
            testLevelStarting ? Muted : TextStrong,
            cmd,
            0.75f,
            0.12f);
        DrawRect(
            sceneMetaX,
            0.0f,
            1.0f,
            TopBarHeight,
            BorderSoft,
            cmd);
        DrawText(
            sceneName_,
            sceneMetaX + 20.0f,
            17.0f,
            SceneChromeStandardTextSize,
            Text,
            cmd,
            1.25f,
            0.08f);
        const bool sceneWorkspaceActive =
            !environmentWorkspaceActive_ && !terrainWorkspaceActive_ &&
            !renderWorkspaceActive_;
        const wi::Color sceneWorkspaceColor =
            sceneWorkspaceActive ? TextStrong : TextSecondary;
        const wi::Color environmentWorkspaceColor =
            environmentWorkspaceActive_ ? TextStrong : TextSecondary;
        const wi::Color terrainWorkspaceColor =
            terrainWorkspaceActive_ ? TextStrong : TextSecondary;
        const wi::Color renderWorkspaceColor =
            renderWorkspaceActive_ ? TextStrong : TextSecondary;
        DrawText(
            "SCENE",
            sceneMetaX + 14.0f,
            38.0f,
            SceneChromeSmallTextSize,
            sceneWorkspaceColor,
            cmd,
            1.1f,
            sceneWorkspaceActive ? 0.12f : 0.0f);
        DrawText(
            "ENVIRONMENT",
            sceneMetaX + 66.0f,
            38.0f,
            SceneChromeSmallTextSize,
            environmentWorkspaceColor,
            cmd,
            1.1f,
            environmentWorkspaceActive_ ? 0.12f : 0.0f);
        DrawText(
            "TERRAIN",
            sceneMetaX + 166.0f,
            38.0f,
            SceneChromeSmallTextSize,
            terrainWorkspaceColor,
            cmd,
            1.05f,
            terrainWorkspaceActive_ ? 0.12f : 0.0f);
        DrawText(
            "RENDER",
            sceneMetaX + 228.0f,
            38.0f,
            SceneChromeSmallTextSize,
            renderWorkspaceColor,
            cmd,
            1.05f,
            renderWorkspaceActive_ ? 0.12f : 0.0f);
        DrawRect(
            renderWorkspaceActive_
                ? sceneMetaX + 226.0f
                : terrainWorkspaceActive_
                    ? sceneMetaX + 164.0f
                    : environmentWorkspaceActive_
                        ? sceneMetaX + 64.0f
                        : sceneMetaX + 12.0f,
            57.0f,
            renderWorkspaceActive_
                ? 60.0f
                : terrainWorkspaceActive_
                    ? 60.0f
                    : environmentWorkspaceActive_ ? 98.0f : 50.0f,
            2.0f,
            Forge,
            cmd);

        // Left hierarchy panel:
        DrawRect(
            0.0f,
            TopBarHeight,
            hierarchyWidth_,
            height_ - TopBarHeight - StatusBarHeight,
            Surface0,
            cmd);
        DrawRect(
            hierarchyWidth_ - 1.0f,
            TopBarHeight,
            1.0f,
            height_ - TopBarHeight - StatusBarHeight,
            Border,
            cmd);
        DrawRect(
            0.0f,
            TopBarHeight,
            hierarchyWidth_,
            PanelHeaderHeight,
            Surface2,
            cmd);
        DrawRect(
            0.0f,
            TopBarHeight + PanelHeaderHeight - 1.0f,
            hierarchyWidth_,
            1.0f,
            BorderSoft,
            cmd);
        DrawText(
            "SCENE HIERARCHY",
            14.0f,
            TopBarHeight + 15.0f,
            SceneHierarchyEntityTextSize,
            TextSecondary,
            cmd,
            1.45f,
            0.18f);
        DrawText(
            "+   ...",
            hierarchyWidth_ - 62.0f,
            TopBarHeight + 15.0f,
            SceneChromeStandardTextSize,
            Muted,
            cmd,
            1.0f);

        const float searchY = TopBarHeight + PanelHeaderHeight + 10.0f;
        DrawBorderedRect(
            12.0f,
            searchY,
            hierarchyWidth_ - 24.0f,
            31.0f,
            wi::Color(8, 12, 15, 255),
            BorderSoft,
            cmd);
        DrawText(
            "SEARCH SCENE...",
            40.0f,
            searchY + 9.0f,
            SceneChromeStandardTextSize,
            TextSecondary,
            cmd,
            0.7f,
            0.18f);
        DrawText("⌕", 22.0f, searchY + 7.0f, 13, Muted, cmd);

        const float rowsTop = searchY + 42.0f;
        const float rowsBottom = statusTop - 8.0f;
        std::size_t displayIndex = 0;
        for (std::size_t visibleIndex = hierarchyScrollRow_;
             visibleIndex < visibleHierarchyRows_.size();
             ++visibleIndex, ++displayIndex)
        {
            const float rowY =
                rowsTop + displayIndex * HierarchyRowHeight;
            if (rowY + HierarchyRowHeight > rowsBottom)
            {
                break;
            }
            const auto& item = visibleHierarchyRows_[visibleIndex];
            if (item.header)
            {
                const auto categoryIndex =
                    static_cast<std::size_t>(item.category);
                const bool expanded = !hierarchyFilter_.empty() ||
                    !collapsedHierarchyCategories_[categoryIndex];
                const auto itemCount = static_cast<std::size_t>(std::count_if(
                    hierarchyRows_.begin(),
                    hierarchyRows_.end(),
                    [&item](const HierarchyRow& row)
                    {
                        return row.category == item.category;
                    }));
                DrawRect(
                    8.0f,
                    rowY,
                    hierarchyWidth_ - 16.0f,
                    HierarchyRowHeight,
                    wi::Color(16, 23, 28, 235),
                    cmd);
                DrawText(
                    expanded ? "▼" : "▶",
                    16.0f,
                    rowY + 8.0f,
                    SceneChromeSmallTextSize,
                    Forge,
                    cmd);
                DrawText(
                    HierarchyCategoryLabel(item.category),
                    34.0f,
                    rowY + 7.0f,
                    SceneChromeStandardTextSize,
                    TextStrong,
                    cmd,
                    1.0f,
                    0.16f);
                DrawText(
                    std::to_string(itemCount),
                    hierarchyWidth_ - 35.0f,
                    rowY + 7.0f,
                    SceneChromeSmallTextSize,
                    Muted,
                    cmd);
                continue;
            }

            const auto& row = hierarchyRows_[item.rowIndex];
            if (row.selected)
            {
                DrawRect(
                    8.0f,
                    rowY,
                    hierarchyWidth_ - 16.0f,
                    HierarchyRowHeight,
                    wi::Color(67, 28, 13, 150),
                    cmd);
                DrawRect(
                    8.0f,
                    rowY,
                    2.0f,
                    HierarchyRowHeight,
                    Forge,
                    cmd);
            }
            const float indent = 14.0f + std::max(0, row.depth) * 16.0f;
            const bool hasChildren = HierarchyRowHasChildren(item.rowIndex);
            const bool collapsed = hasChildren &&
                collapsedHierarchyEntities_.count(row.entity) != 0;
            DrawText(
                hasChildren ? (collapsed ? "▶" : "▼") : "",
                indent,
                rowY + 8.0f,
                SceneChromeSmallTextSize,
                collapsed ? Muted : Forge,
                cmd);
            DrawText(
                "◇",
                indent + 15.0f,
                rowY + 7.0f,
                SceneChromeSmallTextSize,
                row.selected ? Forge : wi::Color(198, 118, 41, 255),
                cmd);
            const float hierarchyNameX = indent + 36.0f;
            DrawText(
                Ellipsize(
                    row.name,
                    HierarchyTextCapacity(hierarchyWidth_, hierarchyNameX)),
                hierarchyNameX,
                rowY + 6.0f,
                SceneHierarchyEntityTextSize,
                row.selected ? TextStrong : TextSecondary,
                cmd,
                0.0f,
                0.18f);
            DrawText(
                "◉",
                hierarchyWidth_ - 25.0f,
                rowY + 7.0f,
                SceneChromeSmallTextSize,
                Muted,
                cmd);
        }

        const std::size_t capacity = HierarchyRowCapacity(height_);
        if (visibleHierarchyRows_.size() > capacity)
        {
            const float trackX = hierarchyWidth_ - 10.0f;
            const float trackHeight = rowsBottom - rowsTop;
            const float thumbHeight = std::max(
                24.0f,
                trackHeight * static_cast<float>(capacity) /
                    static_cast<float>(visibleHierarchyRows_.size()));
            const std::size_t maximumScroll =
                visibleHierarchyRows_.size() - capacity;
            const float thumbY = rowsTop +
                (trackHeight - thumbHeight) *
                    static_cast<float>(hierarchyScrollRow_) /
                    static_cast<float>(maximumScroll);
            DrawRect(trackX, rowsTop, 3.0f, trackHeight, BorderSoft, cmd);
            DrawRect(trackX, thumbY, 3.0f, thumbHeight, Forge, cmd);
        }

        // Scene tab strip and viewport framing:
        DrawRect(
            hierarchyWidth_,
            TopBarHeight,
            inspectorX - hierarchyWidth_,
            SceneTabsHeight,
            wi::Color(8, 12, 15, 255),
            cmd);
        DrawRect(
            hierarchyWidth_,
            viewportTop - 1.0f,
            inspectorX - hierarchyWidth_,
            1.0f,
            Border,
            cmd);
        DrawRect(
            hierarchyWidth_,
            TopBarHeight,
            230.0f,
            SceneTabsHeight,
            Surface2,
            cmd);
        DrawRect(
            hierarchyWidth_,
            TopBarHeight,
            230.0f,
            2.0f,
            Forge,
            cmd);
        DrawText(
            sceneName_ + ".WISCENE" + (sceneDirty_ ? " *" : ""),
            hierarchyWidth_ + 14.0f,
            TopBarHeight + 12.0f,
            SceneChromeStandardTextSize,
            Text,
            cmd,
            0.55f);
        DrawText(
            "×",
            hierarchyWidth_ + 211.0f,
            TopBarHeight + 10.0f,
            SceneChromeStandardTextSize,
            Muted,
            cmd);
        DrawRect(
            hierarchyWidth_,
            viewportTop,
            1.0f,
            viewportBottom - viewportTop,
            Border,
            cmd);
        DrawRect(
            hierarchyWidth_,
            viewportTop,
            inspectorX - hierarchyWidth_,
            1.0f,
            BorderSoft,
            cmd);

        // Viewport overlays are chrome, not scene content.
        float chipX = hierarchyWidth_ + 12.0f;
        int chipIndex = 0;
        for (const char* chip : {"PERSPECTIVE", "LIT", "SHOW"})
        {
            const float chipWidth = std::string(chip).size() * 7.5f + 24.0f;
            DrawBorderedRect(
                chipX,
                viewportTop + 10.0f,
                chipWidth,
                28.0f,
                wi::Color(9, 14, 17, 220),
                activeViewportMenu_ == chipIndex ? Forge : Border,
                cmd);
            DrawText(
                chip,
                chipX + 10.0f,
                viewportTop + 18.0f,
                SceneChromeSmallTextSize,
                TextSecondary,
                cmd,
                0.65f,
                0.16f);
            chipX += chipWidth + 6.0f;
            ++chipIndex;
        }
        if (!selectionName_.empty())
        {
            const float tagY = viewportBottom - 39.0f;
            DrawRect(
                hierarchyWidth_ + 14.0f,
                tagY,
                2.0f,
                25.0f,
                Forge,
                cmd);
            DrawRect(
                hierarchyWidth_ + 16.0f,
                tagY,
                230.0f,
                25.0f,
                wi::Color(4, 7, 9, 180),
                cmd);
            DrawText(
                "SELECTED: " + selectionName_,
                hierarchyWidth_ + 25.0f,
                tagY + 7.0f,
                SceneChromeSmallTextSize,
                TextSecondary,
                cmd,
                1.0f);
        }

        if (activeBottomTab_ >= 0)
        {
            const float drawerTop = bottomTabsTop - drawerHeight_;
            DrawRect(
                hierarchyWidth_, drawerTop,
                inspectorX - hierarchyWidth_, drawerHeight_,
                wi::Color(8, 13, 16, 252), cmd);
            DrawRect(
                hierarchyWidth_, drawerTop,
                inspectorX - hierarchyWidth_, 1.0f, Border, cmd);
            constexpr std::array<const char*, 4> drawerTitles = {
                "PROJECT ASSETS", "CONSOLE", "BUILD OUTPUT", "DIAGNOSTICS"};
            DrawText(
                drawerTitles[activeBottomTab_],
                hierarchyWidth_ + 16.0f, drawerTop + 15.0f,
                SceneChromeStandardTextSize, TextStrong, cmd, 1.1f, 0.18f);
            DrawText(
                "▼",
                inspectorX - 31.0f,
                drawerTop + 15.0f,
                SceneChromeStandardTextSize,
                TextStrong,
                cmd,
                0.0f,
                0.18f);
            DrawRect(
                hierarchyWidth_ + 16.0f, drawerTop + 38.0f,
                inspectorX - hierarchyWidth_ - 32.0f,
                1.0f, BorderSoft, cmd);
            if (activeBottomTab_ == 0)
            {
                RenderAssetBrowser(drawerTop, inspectorX, cmd);
            }
            else if (activeBottomTab_ == 2)
            {
                const std::string outputStatus = statusText_.empty()
                    ? "Build output will appear here."
                    : statusText_;
                const float outputWidth = std::max(
                    120.0f,
                    inspectorX - hierarchyWidth_ - 32.0f);
                const std::size_t outputCharacters =
                    std::max<std::size_t>(
                        20,
                        static_cast<std::size_t>(
                            std::floor(outputWidth / 7.2f)));
                const auto lines = WrapTextLines(
                    outputStatus,
                    outputCharacters);
                float lineY = drawerTop + 55.0f;
                for (const auto& line : lines)
                {
                    if (lineY + 14.0f >= bottomTabsTop - 6.0f)
                    {
                        break;
                    }
                    DrawText(
                        line,
                        hierarchyWidth_ + 16.0f,
                        lineY,
                        SceneChromeSmallTextSize,
                        TextSecondary,
                        cmd,
                        0.0f,
                        0.18f);
                    lineY += 14.0f;
                }
            }
            else
            {
                const std::string message = activeBottomTab_ == 1
                    ? "Console connected. No messages in this session."
                    : "Runtime diagnostics are shown here when available.";
                DrawText(
                    message,
                    hierarchyWidth_ + 16.0f, drawerTop + 55.0f,
                    SceneChromeSmallTextSize, TextSecondary, cmd, 0.0f, 0.18f);
            }
        }

        // Hidden-until-needed bottom drawer tabs.
        DrawRect(
            hierarchyWidth_,
            bottomTabsTop,
            inspectorX - hierarchyWidth_,
            BottomTabsHeight,
            wi::Color(8, 16, 21, 250),
            cmd);
        DrawRect(
            hierarchyWidth_,
            bottomTabsTop,
            inspectorX - hierarchyWidth_,
            1.0f,
            Border,
            cmd);
        float bottomX = hierarchyWidth_ + 15.0f;
        int tabIndex = 0;
        for (const char* tab : {
                 "ASSET BROWSER", "CONSOLE", "OUTPUT", "DIAGNOSTICS"})
        {
            if (activeBottomTab_ == tabIndex)
            {
                DrawRect(
                    bottomX - 7.0f, bottomTabsTop,
                    std::string(tab).size() * 7.2f + 18.0f,
                    2.0f, Forge, cmd);
            }
            DrawText(
                tab,
                bottomX,
                bottomTabsTop + 10.0f,
                SceneChromeSmallTextSize,
                TextSecondary,
                cmd,
                0.9f,
                0.18f);
            bottomX += std::string(tab).size() * 7.2f + 31.0f;
            DrawRect(
                bottomX - 16.0f,
                bottomTabsTop,
                1.0f,
                BottomTabsHeight,
                BorderSoft,
                cmd);
            ++tabIndex;
        }
        DrawText(
            activeBottomTab_ >= 0 ? "▼" : "▲",
            inspectorX - 27.0f,
            bottomTabsTop + 10.0f,
            SceneChromeSmallTextSize,
            TextStrong,
            cmd,
            0.0f,
            0.18f);

        // Context Inspector background. Interactive Renegade controls render
        // over this surface and retain the existing command/service wiring.
        DrawRect(
            inspectorX, TopBarHeight,
            inspectorWidth_, statusTop - TopBarHeight,
            Surface0, cmd);
        DrawRect(
            inspectorX, TopBarHeight,
            1.0f, statusTop - TopBarHeight,
            Border, cmd);
        DrawRect(
            inspectorX, TopBarHeight,
            inspectorWidth_, PanelHeaderHeight,
            Surface2, cmd);
        DrawRect(
            inspectorX, TopBarHeight + PanelHeaderHeight - 1.0f,
            inspectorWidth_, 1.0f,
            BorderSoft, cmd);

        // Renegade-owned splitters replace the resize affordances lost when
        // the stock Wicked windows were removed. They remain quiet until
        // hovered or dragged.
        const wi::Color leftSplitter =
            hoveredSplitter_ == 1 || resizingPanel_ == 1 ? Forge : Border;
        const wi::Color rightSplitter =
            hoveredSplitter_ == 2 || resizingPanel_ == 2 ? Forge : Border;
        DrawRect(
            hierarchyWidth_ - 1.0f,
            TopBarHeight,
            2.0f,
            statusTop - TopBarHeight,
            leftSplitter,
            cmd);
        DrawRect(
            inspectorX - 1.0f,
            TopBarHeight,
            2.0f,
            statusTop - TopBarHeight,
            rightSplitter,
            cmd);
        if (activeBottomTab_ >= 0)
        {
            const float drawerTop = bottomTabsTop - drawerHeight_;
            DrawRect(
                hierarchyWidth_,
                drawerTop - 1.0f,
                inspectorX - hierarchyWidth_,
                2.0f,
                hoveredSplitter_ == 3 || resizingPanel_ == 3
                    ? Forge
                    : Border,
                cmd);
        }

        const auto drawPopup = [cmd](
            const float x,
            const float y,
            const std::vector<std::pair<std::string, bool>>& items)
        {
            constexpr float popupWidth = 226.0f;
            constexpr float itemHeight = 30.0f;
            DrawBorderedRect(
                x,
                y,
                popupWidth,
                itemHeight * static_cast<float>(items.size()),
                wi::Color(7, 11, 13, 252),
                Border,
                cmd);
            for (std::size_t index = 0; index < items.size(); ++index)
            {
                const float itemY = y + index * itemHeight;
                if (index > 0)
                {
                    DrawRect(
                        x + 8.0f,
                        itemY,
                        popupWidth - 16.0f,
                        1.0f,
                        BorderSoft,
                        cmd);
                }
                DrawText(
                    items[index].first,
                    x + 12.0f,
                    itemY + 10.0f,
                    SceneChromeSmallTextSize,
                    items[index].second ? TextStrong : Muted,
                    cmd,
                    0.35f,
                    0.16f);
            }
        };

        if (activeMenu_ >= 0)
        {
            constexpr std::array<const char*, 6> menuLabels = {
                "FILE", "EDIT", "ADD", "VIEW", "BUILD", "WINDOW"};
            float popupX = 214.0f;
            for (int index = 0; index < activeMenu_; ++index)
            {
                popupX +=
                    std::string(menuLabels[index]).size() * 8.0f + 25.0f;
            }
            std::vector<std::pair<std::string, bool>> items;
            if (activeMenu_ == 0)
            {
                items = {{"PROJECT HUB", true}, {"OPEN SCENE...", true},
                    {"SAVE", true}, {"SAVE AS...", true},
                    {"REOPEN SCENE", true}};
            }
            else if (activeMenu_ == 1)
            {
                items = {{"UNDO", true}, {"REDO", true},
                    {"DUPLICATE", true}, {"DELETE", true}};
            }
            else if (activeMenu_ == 2)
            {
                items = {{"POINT LIGHT", true}, {"SPOT LIGHT", true},
                    {"DIRECTIONAL LIGHT", true}, {"RECTANGLE LIGHT", true},
                    {"PLAYER START", true}, {"GAMEPLAY SCRIPT...", true},
                    {"CAMERA", true}, {"DECAL", true},
                    {"ENVIRONMENT PROBE", true}, {"IMPORT MODEL...", true}};
            }
            else if (activeMenu_ == 3)
            {
                items = {{"FOCUS SELECTION", true},
                    {gridVisible_ ? "HIDE EDITOR GRID" : "SHOW EDITOR GRID", true},
                    {"ASSET BROWSER", true}, {"DIAGNOSTICS", true}};
            }
            else if (activeMenu_ == 4)
            {
                items = {{"BUILD WINDOWS GAME...", true},
                    {"VALIDATE GLB/GLTF IMPORT...", true}};
            }
            else
            {
                items = {{"HIERARCHY // FIXED", false},
                    {"INSPECTOR // FIXED", false},
                    {activeBottomTab_ >= 0 ? "CLOSE BOTTOM DRAWER" :
                        "OPEN BOTTOM DRAWER", true}};
            }
            drawPopup(popupX, TopBarHeight, items);
        }

        if (activeViewportMenu_ >= 0)
        {
            const float popupX = hierarchyWidth_ + 12.0f;
            if (activeViewportMenu_ == 0)
            {
                drawPopup(
                    popupX,
                    viewportTop + 42.0f,
                    {{"PERSPECTIVE", true},
                        {"ORTHOGRAPHIC // NOT EXPOSED", false}});
            }
            else if (activeViewportMenu_ == 1)
            {
                drawPopup(
                    popupX,
                    viewportTop + 42.0f,
                    {{"LIT", true}, {"UNLIT // NOT EXPOSED", false}});
            }
            else
            {
                drawPopup(
                    popupX,
                    viewportTop + 42.0f,
                    {{gridVisible_ ? "✓  EDITOR GRID" : "EDITOR GRID", true}});
            }
        }

        // Status is secondary information, never a second toolbar. The footer
        // summary is bounded before the FPS group so long errors cannot collide
        // with right-side status. The complete status remains in statusText_
        // and is presented unabridged by the wrapped Output drawer.
        DrawRect(0.0f, statusTop, width_, StatusBarHeight, Surface0, cmd);
        DrawRect(0.0f, statusTop, width_, 1.0f, Border, cmd);
        DrawRect(16.0f, statusTop + 10.0f, 7.0f, 7.0f, Success, cmd);
        DrawText(
            "READY",
            31.0f,
            statusTop + 8.0f,
            SceneChromeSmallTextSize,
            Muted,
            cmd,
            1.15f);
        DrawText(
            Ellipsize(statusText_, StatusTextCapacity(width_)),
            105.0f,
            statusTop + 8.0f,
            SceneChromeSmallTextSize,
            Muted,
            cmd,
            0.75f);
        const std::string fpsText = displayedFps_ > 0.0f
            ? std::to_string(static_cast<int>(std::lround(displayedFps_))) +
                " FPS"
            : "-- FPS";
        DrawRect(
            width_ - 402.0f,
            statusTop + 7.0f,
            1.0f,
            14.0f,
            Border,
            cmd);
        DrawText(
            fpsText,
            width_ - 386.0f,
            statusTop + 8.0f,
            SceneChromeSmallTextSize,
            TextStrong,
            cmd,
            0.75f);
        DrawText(
            "RENEGADE STUDIO // OWNED CHROME",
            width_ - 285.0f,
            statusTop + 8.0f,
            SceneChromeSmallTextSize,
            TextSecondary,
            cmd,
            0.9f);
    }
}
