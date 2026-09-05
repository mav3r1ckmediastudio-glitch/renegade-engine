#pragma once

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ScriptMetadataService.h"
#include "renegade/bridge/ScriptReferenceAuthoringService.h"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace renegade::studio
{
    class S4CScriptPropertyEditor final
    {
    public:
        S4CScriptPropertyEditor(
            wi::gui::Window& panel,
            std::function<void()> requestRefresh,
            std::function<void(std::string)> setStatus);

        void Refresh(
            const bridge::StableId& scriptInstanceId,
            const bridge::ScriptAttachment& attachment,
            const bridge::ScriptMetadataDescriptor* metadata);

        [[nodiscard]] float MeasureHeight() const noexcept;
        void Layout(float x, float y, float width);
        void SetVisible(bool visible);

    private:
        struct PropertyControls
        {
            wi::gui::Label label;
            SceneInspectorCheckBox booleanValue;
            SceneInspectorTextInputField textValue;
            SceneInspectorComboBox enumValue;
            std::array<SceneInspectorTextInputField, 4> components;
            SceneInspectorComboBox referenceValue;
            std::vector<bridge::ScriptReferenceOption> referenceOptions;

            bridge::ScriptMetadataPropertyDescriptor metadata;
            bridge::ScriptPropertyValue value;
            int componentCount = 0;
            bool configured = false;
        };

        void EnsureRows(std::size_t count);
        void CreateRow(std::size_t index);
        void ConfigureRow(
            std::size_t index,
            const bridge::ScriptMetadataPropertyDescriptor& metadata,
            const bridge::ScriptPropertyValue& value);
        void ApplyValueToWidgets(std::size_t index);
        void SetRowVisible(PropertyControls& row, bool visible);
        void CommitBoolean(std::size_t index, bool value);
        void CommitText(std::size_t index, const std::string& text);
        void CommitEnum(std::size_t index, std::size_t optionIndex);
        void CommitComponent(
            std::size_t index,
            std::size_t component,
            const std::string& text);
        void CommitReference(std::size_t index, std::size_t optionIndex);
        void CommitValue(
            std::size_t index,
            bridge::ScriptPropertyValue value);
        void FailInput(std::size_t index, std::string error);
        void RequestRefresh();
        void SetStatus(std::string status);

        wi::gui::Window* panel_ = nullptr;
        std::function<void()> requestRefresh_;
        std::function<void(std::string)> setStatus_;
        bridge::StableId scriptInstanceId_;
        std::vector<std::unique_ptr<PropertyControls>> rows_;
        std::size_t usedRows_ = 0;
        bool visible_ = false;
    };
}
