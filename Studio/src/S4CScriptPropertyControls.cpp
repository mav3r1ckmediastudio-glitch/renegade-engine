#include "S4CScriptPropertyControls.h"

#include "renegade/bridge/ScriptPropertyAuthoringService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace
{
    using namespace renegade;

    std::string Trim(std::string value)
    {
        const auto first = std::find_if_not(
            value.begin(), value.end(),
            [](const unsigned char c) { return std::isspace(c) != 0; });
        const auto last = std::find_if_not(
            value.rbegin(), value.rend(),
            [](const unsigned char c) { return std::isspace(c) != 0; }).base();
        if (first >= last)
            return {};
        return std::string(first, last);
    }

    std::string Number(const float value)
    {
        std::ostringstream stream;
        stream << std::setprecision(6) << value;
        return stream.str();
    }

    int ComponentCount(const bridge::ScriptPropertyType type) noexcept
    {
        switch (type)
        {
        case bridge::ScriptPropertyType::Colour:
            return 4;
        case bridge::ScriptPropertyType::Vector2:
            return 2;
        case bridge::ScriptPropertyType::Vector3:
            return 3;
        default:
            return 0;
        }
    }

    const char* ComponentLabel(
        const bridge::ScriptPropertyType type,
        const std::size_t component) noexcept
    {
        if (type == bridge::ScriptPropertyType::Colour)
        {
            static constexpr const char* labels[] = {"R", "G", "B", "A"};
            return component < 4 ? labels[component] : "";
        }
        static constexpr const char* labels[] = {"X", "Y", "Z", "W"};
        return component < 4 ? labels[component] : "";
    }

    bool IsDeferredReference(const bridge::ScriptPropertyType type) noexcept
    {
        switch (type)
        {
        case bridge::ScriptPropertyType::EntityReference:
        case bridge::ScriptPropertyType::AssetReference:
        case bridge::ScriptPropertyType::Animation:
        case bridge::ScriptPropertyType::Audio:
            return true;
        default:
            return false;
        }
    }
}

namespace renegade::studio
{
    S4CScriptPropertyEditor::S4CScriptPropertyEditor(
        wi::gui::Window& panel,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
        : panel_(&panel)
        , requestRefresh_(std::move(requestRefresh))
        , setStatus_(std::move(setStatus))
    {
    }

    void S4CScriptPropertyEditor::Refresh(
        const bridge::StableId& scriptInstanceId,
        const bridge::ScriptAttachment& attachment,
        const bridge::ScriptMetadataDescriptor* metadata)
    {
        scriptInstanceId_ = scriptInstanceId;
        usedRows_ = 0;
        if (metadata == nullptr)
        {
            SetVisible(visible_);
            return;
        }

        EnsureRows(metadata->properties.size());
        for (std::size_t index = 0; index < metadata->properties.size(); ++index)
        {
            const auto& descriptor = metadata->properties[index];
            bridge::ScriptPropertyValue value;
            const auto existing = std::find_if(
                attachment.properties.begin(), attachment.properties.end(),
                [&](const bridge::ScriptPropertyValue& candidate)
                {
                    return candidate.name == descriptor.name;
                });
            if (existing != attachment.properties.end())
            {
                value = *existing;
            }
            else if (descriptor.hasDefault)
            {
                value = descriptor.defaultValue;
                value.name = descriptor.name;
                value.type = descriptor.type;
            }
            else
            {
                value.name = descriptor.name;
                value.type = descriptor.type;
            }
            ConfigureRow(index, descriptor, value);
        }
        usedRows_ = metadata->properties.size();
        SetVisible(visible_);
    }

    float S4CScriptPropertyEditor::MeasureHeight() const noexcept
    {
        return usedRows_ == 0
            ? 0.0f
            : static_cast<float>(usedRows_) * 34.0f + 4.0f;
    }

    void S4CScriptPropertyEditor::Layout(
        const float x,
        const float y,
        const float width)
    {
        if (!visible_)
            return;

        const float labelWidth = std::clamp(width * 0.32f, 92.0f, 132.0f);
        const float gap = 6.0f;
        const float controlX = x + labelWidth + gap;
        const float controlWidth = std::max(70.0f, width - labelWidth - gap);

        for (std::size_t index = 0; index < usedRows_; ++index)
        {
            auto& row = *rows_[index];
            const float rowY = y + static_cast<float>(index) * 34.0f;
            row.label.SetPos(XMFLOAT2(x, rowY));
            row.label.SetSize(XMFLOAT2(labelWidth, 28.0f));

            row.booleanValue.SetPos(XMFLOAT2(controlX, rowY));
            row.booleanValue.SetSize(XMFLOAT2(std::min(82.0f, controlWidth), 28.0f));
            row.textValue.SetPos(XMFLOAT2(controlX, rowY));
            row.textValue.SetSize(XMFLOAT2(controlWidth, 28.0f));
            row.enumValue.SetPos(XMFLOAT2(controlX, rowY));
            row.enumValue.SetSize(XMFLOAT2(controlWidth, 28.0f));
            row.deferredValue.SetPos(XMFLOAT2(controlX, rowY));
            row.deferredValue.SetSize(XMFLOAT2(controlWidth, 28.0f));

            if (row.componentCount > 0)
            {
                const float componentGap = 4.0f;
                const float componentWidth = std::max(
                    42.0f,
                    (controlWidth - componentGap *
                        static_cast<float>(row.componentCount - 1)) /
                        static_cast<float>(row.componentCount));
                for (int component = 0; component < row.componentCount; ++component)
                {
                    row.components[static_cast<std::size_t>(component)].SetPos(
                        XMFLOAT2(
                            controlX + static_cast<float>(component) *
                                (componentWidth + componentGap),
                            rowY));
                    row.components[static_cast<std::size_t>(component)].SetSize(
                        XMFLOAT2(componentWidth, 28.0f));
                }
            }
        }
    }

    void S4CScriptPropertyEditor::SetVisible(const bool visible)
    {
        visible_ = visible;
        for (std::size_t index = 0; index < rows_.size(); ++index)
            SetRowVisible(*rows_[index], visible && index < usedRows_);
    }

    void S4CScriptPropertyEditor::EnsureRows(const std::size_t count)
    {
        while (rows_.size() < count)
            CreateRow(rows_.size());
    }

    void S4CScriptPropertyEditor::CreateRow(const std::size_t index)
    {
        auto row = std::make_unique<PropertyControls>();
        const std::string suffix = std::to_string(index);

        row->label.Create("S4C Property Label " + suffix);
        panel_->AddWidget(&row->label);

        row->booleanValue.Create("ON");
        row->booleanValue.OnClick(
            [this, index](const wi::gui::EventArgs& args)
            {
                CommitBoolean(index, args.bValue);
            });
        panel_->AddWidget(&row->booleanValue);

        row->textValue.Create("S4C Property Value " + suffix);
        row->textValue.SetCancelInputEnabled(false);
        row->textValue.OnInputAccepted(
            [this, index](const wi::gui::EventArgs& args)
            {
                CommitText(index, args.sValue);
            });
        panel_->AddWidget(&row->textValue);

        row->enumValue.Create("S4C Property Enum " + suffix);
        row->enumValue.OnSelect(
            [this, index](const wi::gui::EventArgs& args)
            {
                CommitEnum(index, static_cast<std::size_t>(args.userdata));
            });
        panel_->AddWidget(&row->enumValue);

        for (std::size_t component = 0; component < row->components.size(); ++component)
        {
            auto& input = row->components[component];
            input.Create(
                "S4C Property Component " + suffix + " " +
                std::to_string(component));
            input.SetCancelInputEnabled(false);
            input.OnInputAccepted(
                [this, index, component](const wi::gui::EventArgs& args)
                {
                    CommitComponent(index, component, args.sValue);
                });
            panel_->AddWidget(&input);
        }

        row->deferredValue.Create("S4C Deferred Reference " + suffix);
        panel_->AddWidget(&row->deferredValue);

        SetRowVisible(*row, false);
        rows_.push_back(std::move(row));
    }

    void S4CScriptPropertyEditor::ConfigureRow(
        const std::size_t index,
        const bridge::ScriptMetadataPropertyDescriptor& metadata,
        const bridge::ScriptPropertyValue& value)
    {
        if (index >= rows_.size())
            return;
        auto& row = *rows_[index];
        row.metadata = metadata;
        row.value = value;
        row.componentCount = ComponentCount(metadata.type);
        row.configured = true;

        row.label.SetText(metadata.label.empty() ? metadata.name : metadata.label);
        row.label.SetTooltip(metadata.description);
        row.booleanValue.SetTooltip(metadata.description);
        row.textValue.SetTooltip(metadata.description);
        row.enumValue.SetTooltip(metadata.description);
        row.deferredValue.SetTooltip(
            metadata.description.empty()
                ? std::string("Reference picker arrives in S4D.")
                : metadata.description + " Reference picker arrives in S4D.");

        row.enumValue.ClearItems();
        for (std::size_t option = 0; option < metadata.enumOptions.size(); ++option)
        {
            const auto& item = metadata.enumOptions[option];
            row.enumValue.AddItem(
                item.label.empty() ? item.value : item.label,
                static_cast<std::uint64_t>(option));
        }
        for (std::size_t component = 0; component < row.components.size(); ++component)
        {
            row.components[component].SetDescription(
                std::string(ComponentLabel(metadata.type, component)) + "  ");
            row.components[component].SetTooltip(metadata.description);
        }

        ApplyValueToWidgets(index);
    }

    void S4CScriptPropertyEditor::ApplyValueToWidgets(const std::size_t index)
    {
        if (index >= rows_.size())
            return;
        auto& row = *rows_[index];
        const auto type = row.metadata.type;

        switch (type)
        {
        case bridge::ScriptPropertyType::Boolean:
            row.booleanValue.SetCheck(row.value.booleanValue);
            break;
        case bridge::ScriptPropertyType::Integer:
            row.textValue.SetValue(std::to_string(row.value.integerValue));
            break;
        case bridge::ScriptPropertyType::Float:
            row.textValue.SetValue(Number(row.value.numberValue));
            break;
        case bridge::ScriptPropertyType::String:
            row.textValue.SetValue(row.value.textValue);
            break;
        case bridge::ScriptPropertyType::Enum:
        {
            std::size_t selected = 0;
            for (std::size_t option = 0; option < row.metadata.enumOptions.size(); ++option)
            {
                if (row.metadata.enumOptions[option].value == row.value.textValue)
                {
                    selected = option;
                    break;
                }
            }
            if (!row.metadata.enumOptions.empty())
                row.enumValue.SetSelectedWithoutCallback(static_cast<int>(selected));
            break;
        }
        case bridge::ScriptPropertyType::Colour:
        case bridge::ScriptPropertyType::Vector2:
        case bridge::ScriptPropertyType::Vector3:
        {
            const float values[4] = {
                row.value.x, row.value.y, row.value.z, row.value.w};
            for (int component = 0; component < row.componentCount; ++component)
            {
                row.components[static_cast<std::size_t>(component)].SetValue(
                    Number(values[component]));
            }
            break;
        }
        case bridge::ScriptPropertyType::EntityReference:
        case bridge::ScriptPropertyType::AssetReference:
        case bridge::ScriptPropertyType::Animation:
        case bridge::ScriptPropertyType::Audio:
            row.deferredValue.SetText(
                bridge::IsValidStableId(row.value.referenceId) || !row.value.pathHint.empty()
                    ? "Assigned — picker in S4D"
                    : "Unresolved — picker in S4D");
            break;
        }
        SetRowVisible(row, visible_ && index < usedRows_);
    }

    void S4CScriptPropertyEditor::SetRowVisible(
        PropertyControls& row,
        const bool visible)
    {
        row.label.SetVisible(visible);
        row.booleanValue.SetVisible(false);
        row.textValue.SetVisible(false);
        row.enumValue.SetVisible(false);
        row.deferredValue.SetVisible(false);
        for (auto& component : row.components)
            component.SetVisible(false);
        if (!visible || !row.configured)
            return;

        switch (row.metadata.type)
        {
        case bridge::ScriptPropertyType::Boolean:
            row.booleanValue.SetVisible(true);
            break;
        case bridge::ScriptPropertyType::Integer:
        case bridge::ScriptPropertyType::Float:
        case bridge::ScriptPropertyType::String:
            row.textValue.SetVisible(true);
            break;
        case bridge::ScriptPropertyType::Enum:
            row.enumValue.SetVisible(true);
            break;
        case bridge::ScriptPropertyType::Colour:
        case bridge::ScriptPropertyType::Vector2:
        case bridge::ScriptPropertyType::Vector3:
            for (int component = 0; component < row.componentCount; ++component)
                row.components[static_cast<std::size_t>(component)].SetVisible(true);
            break;
        case bridge::ScriptPropertyType::EntityReference:
        case bridge::ScriptPropertyType::AssetReference:
        case bridge::ScriptPropertyType::Animation:
        case bridge::ScriptPropertyType::Audio:
            row.deferredValue.SetVisible(true);
            break;
        }
    }

    void S4CScriptPropertyEditor::CommitBoolean(
        const std::size_t index,
        const bool value)
    {
        if (index >= usedRows_)
            return;
        auto property = rows_[index]->value;
        property.booleanValue = value;
        CommitValue(index, std::move(property));
    }

    void S4CScriptPropertyEditor::CommitText(
        const std::size_t index,
        const std::string& text)
    {
        if (index >= usedRows_)
            return;
        auto property = rows_[index]->value;
        const std::string trimmed = Trim(text);
        try
        {
            switch (rows_[index]->metadata.type)
            {
            case bridge::ScriptPropertyType::Integer:
            {
                std::size_t consumed = 0;
                const long long value = std::stoll(trimmed, &consumed, 10);
                if (consumed != trimmed.size())
                    throw std::invalid_argument("trailing characters");
                property.integerValue = static_cast<std::int64_t>(value);
                break;
            }
            case bridge::ScriptPropertyType::Float:
            {
                std::size_t consumed = 0;
                const float value = std::stof(trimmed, &consumed);
                if (consumed != trimmed.size())
                    throw std::invalid_argument("trailing characters");
                property.numberValue = value;
                break;
            }
            case bridge::ScriptPropertyType::String:
                property.textValue = text;
                break;
            default:
                return;
            }
        }
        catch (const std::exception&)
        {
            FailInput(index, "Enter a valid numeric value.");
            return;
        }
        CommitValue(index, std::move(property));
    }

    void S4CScriptPropertyEditor::CommitEnum(
        const std::size_t index,
        const std::size_t optionIndex)
    {
        if (index >= usedRows_ ||
            optionIndex >= rows_[index]->metadata.enumOptions.size())
        {
            return;
        }
        auto property = rows_[index]->value;
        property.textValue = rows_[index]->metadata.enumOptions[optionIndex].value;
        CommitValue(index, std::move(property));
    }

    void S4CScriptPropertyEditor::CommitComponent(
        const std::size_t index,
        const std::size_t component,
        const std::string& text)
    {
        if (index >= usedRows_ || component >= 4 ||
            component >= static_cast<std::size_t>(rows_[index]->componentCount))
        {
            return;
        }
        const std::string trimmed = Trim(text);
        float parsed = 0.0f;
        try
        {
            std::size_t consumed = 0;
            parsed = std::stof(trimmed, &consumed);
            if (consumed != trimmed.size())
                throw std::invalid_argument("trailing characters");
        }
        catch (const std::exception&)
        {
            FailInput(index, "Enter a valid component value.");
            return;
        }

        auto property = rows_[index]->value;
        float* values[4] = {&property.x, &property.y, &property.z, &property.w};
        *values[component] = parsed;
        CommitValue(index, std::move(property));
    }

    void S4CScriptPropertyEditor::CommitValue(
        const std::size_t index,
        bridge::ScriptPropertyValue value)
    {
        if (index >= usedRows_)
            return;
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr)
            return;

        std::string error;
        if (!bridge::NormalizeScriptPropertyForAuthoring(
                rows_[index]->metadata, value, error))
        {
            FailInput(index, std::move(error));
            return;
        }
        if (!bridge::CommitScriptPropertyAuthoringEdit(
                session->Scripts(),
                session->Commands(),
                scriptInstanceId_,
                rows_[index]->metadata,
                value,
                error))
        {
            FailInput(index, std::move(error));
            return;
        }

        rows_[index]->value = std::move(value);
        ApplyValueToWidgets(index);
        SetStatus(
            "S4C PROPERTIES // " +
            (rows_[index]->metadata.label.empty()
                ? rows_[index]->metadata.name
                : rows_[index]->metadata.label) +
            " updated");
        RequestRefresh();
    }

    void S4CScriptPropertyEditor::FailInput(
        const std::size_t index,
        std::string error)
    {
        if (index < rows_.size())
            ApplyValueToWidgets(index);
        SetStatus("S4C PROPERTIES // " + std::move(error));
    }

    void S4CScriptPropertyEditor::RequestRefresh()
    {
        if (requestRefresh_)
            requestRefresh_();
    }

    void S4CScriptPropertyEditor::SetStatus(std::string status)
    {
        if (setStatus_)
            setStatus_(std::move(status));
    }
}
