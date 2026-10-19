#pragma once
#include "renegade/bridge/ScreenService.h"
#include <map>

namespace renegade::bridge
{
    struct ScreenBindingContext
    {
        std::uint64_t session = 0;
        using Target = std::pair<ScreenBindingScope, StableId>;
        std::map<Target, std::uint64_t> generations;
        std::uint64_t Generation(const ScreenBindingSource& source) const
        {
            const auto found = generations.find({source.scope, source.targetId});
            return found == generations.end() ? 0 : found->second;
        }
    };

    struct ScreenBindingIssue
    {
        StableId widgetId;
        ScreenBindingProperty property = ScreenBindingProperty::Text;
        ScreenBindingDiagnostic diagnostic = ScreenBindingDiagnostic::None;
        bool required = true;
        std::string message;
    };

    struct ScreenResolvedWidget
    {
        StableId id;
        std::string text;
        float fill = .5f;
        bool visible = true;
        bool enabled = true;
    };

    using ScreenWidgetStateOverrides = std::map<StableId, std::pair<bool, bool>>;

    struct ScreenBindingFrame
    {
        StableId documentId;
        std::uint64_t session = 0;
        std::uint64_t revision = 0;
        bool ready = true;
        std::vector<ScreenResolvedWidget> widgets;
        std::vector<ScreenBindingIssue> issues;
    };

    // Produces transient presentation values. Authored fallback values and
    // reference records remain unchanged, including when a live source fails.
    inline bool BuildScreenBindingFrame(const ScreenDocument& document,
        const ScreenBindingSnapshot& snapshot, const ScreenBindingContext& context,
        ScreenBindingFrame& frame, std::string& error,
        const ScreenWidgetStateOverrides* stateOverrides = nullptr)
    {
        if (!ValidateScreenDocument(document, document.envelope.projectId, error)) return false;
        if (context.generations.size() > 4096)
        { error = "Binding generation context exceeds its budget."; return false; }
        ScreenBindingFrame next;
        next.documentId = document.envelope.documentId;
        next.session = context.session;
        next.revision = snapshot.revision;
        std::map<StableId, std::size_t> indices;
        for (const auto& widget : document.widgets)
        {
            ScreenResolvedWidget value{widget.id,widget.text,widget.fill,widget.visible,widget.enabled};
            if (stateOverrides)
            {
                const auto state = stateOverrides->find(widget.id);
                if (state != stateOverrides->end())
                { value.visible = state->second.first; value.enabled = state->second.second; }
            }
            bool hideForMissingSource = false;
            for (const auto& binding : widget.bindings)
            {
                GameplayAttributeValue fallback;
                switch (binding.property)
                {
                case ScreenBindingProperty::Text: fallback = value.text; break;
                case ScreenBindingProperty::Fill: fallback = static_cast<double>(value.fill); break;
                case ScreenBindingProperty::Visible: fallback = value.visible; break;
                case ScreenBindingProperty::Enabled: fallback = value.enabled; break;
                }
                const auto resolved = EvaluateScreenBinding(binding, snapshot, context.session,
                    context.Generation(binding.source), std::move(fallback),
                    context.Generation(binding.denominator));
                if (!resolved.resolved)
                {
                    next.issues.push_back({widget.id,binding.property,resolved.diagnostic,
                        binding.required,resolved.message});
                    if (binding.required) next.ready = false;
                    hideForMissingSource = hideForMissingSource || resolved.hide;
                    continue;
                }
                switch (binding.property)
                {
                case ScreenBindingProperty::Text: value.text = std::get<std::string>(resolved.value); break;
                case ScreenBindingProperty::Fill: value.fill = static_cast<float>(std::get<double>(resolved.value)); break;
                case ScreenBindingProperty::Visible: value.visible = std::get<bool>(resolved.value); break;
                case ScreenBindingProperty::Enabled: value.enabled = std::get<bool>(resolved.value); break;
                }
            }
            if (hideForMissingSource) value.visible = false;
            indices.emplace(widget.id,next.widgets.size());
            next.widgets.push_back(std::move(value));
        }
        // Visibility is a hierarchy property. A child's true condition cannot
        // reveal it through a hidden parent. Document validation proves acyclicity.
        for (std::size_t i = 0; i < document.widgets.size(); ++i)
        {
            auto parent = document.widgets[i].parentId;
            std::size_t remaining = document.widgets.size();
            while (!parent.empty() && remaining-- > 0)
            {
                const auto found = indices.find(parent);
                if (found == indices.end())
                { error = "Resolved binding frame lost an authored parent."; return false; }
                next.widgets[i].visible = next.widgets[i].visible && next.widgets[found->second].visible;
                parent = document.widgets[found->second].parentId;
            }
            if (!parent.empty())
            { error = "Resolved binding visibility exceeded its hierarchy budget."; return false; }
        }
        frame = std::move(next);
        error.clear();
        return true;
    }
}
