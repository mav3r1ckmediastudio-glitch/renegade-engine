#include "InspectorSectionFramework.h"

#include "renegade/bridge/ProjectService.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace renegade::studio
{
    namespace
    {
        bool IsFiniteNonNegative(const float value) noexcept
        {
            return std::isfinite(value) && value >= 0.0f;
        }
    }

    ProjectInspectorSectionPreferenceStore::ProjectInspectorSectionPreferenceStore(
        bridge::ProjectService& projects) noexcept
        : projects_(&projects)
    {
    }

    bool ProjectInspectorSectionPreferenceStore::ReadExpanded(
        const std::string_view sectionId,
        const bool fallback) const
    {
        return projects_ != nullptr
            ? projects_->GetEditorPreference(PreferenceKey(sectionId), fallback)
            : fallback;
    }

    void ProjectInspectorSectionPreferenceStore::WriteExpanded(
        const std::string_view sectionId,
        const bool expanded)
    {
        if (projects_ != nullptr)
            projects_->SetEditorPreference(PreferenceKey(sectionId), expanded);
    }

    std::string ProjectInspectorSectionPreferenceStore::PreferenceKey(
        const std::string_view sectionId)
    {
        return "inspector.section." + std::string(sectionId) + ".expanded";
    }

    InspectorSectionRegistry::InspectorSectionRegistry(
        IInspectorSectionPreferenceStore* preferences) noexcept
        : preferences_(preferences)
    {
    }

    void InspectorSectionRegistry::SetPreferenceStore(
        IInspectorSectionPreferenceStore* preferences) noexcept
    {
        preferences_ = preferences;
        expansionCache_.clear();
    }

    bool InspectorSectionRegistry::Register(
        std::shared_ptr<IInspectorSectionProvider> provider,
        std::string& error)
    {
        error.clear();
        if (provider == nullptr)
        {
            error = "Inspector section provider is null.";
            return false;
        }

        const auto& descriptor = provider->Descriptor();
        if (!IsValidSectionId(descriptor.id))
        {
            error = "Inspector section ID must use lowercase ASCII letters, digits, '.', '_' or '-'.";
            return false;
        }
        if (descriptor.title.empty())
        {
            error = "Inspector section title is empty for '" + descriptor.id + "'.";
            return false;
        }
        if (!std::isfinite(descriptor.headerHeight) || descriptor.headerHeight <= 0.0f)
        {
            error = "Inspector section header height must be finite and positive for '" +
                descriptor.id + "'.";
            return false;
        }
        if (!IsFiniteNonNegative(descriptor.spacingAfter))
        {
            error = "Inspector section spacing must be finite and non-negative for '" +
                descriptor.id + "'.";
            return false;
        }
        if (Contains(descriptor.id))
        {
            error = "Inspector section ID is already registered: " + descriptor.id;
            return false;
        }

        providers_.push_back(std::move(provider));
        SortProviders();
        return true;
    }

    bool InspectorSectionRegistry::Unregister(
        const std::string_view sectionId) noexcept
    {
        const auto it = std::find_if(
            providers_.begin(), providers_.end(),
            [sectionId](const auto& provider)
            {
                return provider != nullptr && provider->Descriptor().id == sectionId;
            });
        if (it == providers_.end())
            return false;

        expansionCache_.erase((*it)->Descriptor().id);
        providers_.erase(it);
        return true;
    }

    void InspectorSectionRegistry::Clear() noexcept
    {
        providers_.clear();
        expansionCache_.clear();
    }

    bool InspectorSectionRegistry::Contains(
        const std::string_view sectionId) const noexcept
    {
        return Find(sectionId) != nullptr;
    }

    std::size_t InspectorSectionRegistry::Count() const noexcept
    {
        return providers_.size();
    }

    bool InspectorSectionRegistry::IsExpanded(
        const std::string_view sectionId) const
    {
        const auto* provider = Find(sectionId);
        return provider != nullptr
            ? ResolveExpanded(provider->Descriptor())
            : false;
    }

    bool InspectorSectionRegistry::SetExpanded(
        const std::string_view sectionId,
        const bool expanded)
    {
        const auto* provider = Find(sectionId);
        if (provider == nullptr)
            return false;

        const auto& id = provider->Descriptor().id;
        expansionCache_[id] = expanded;
        if (preferences_ != nullptr)
            preferences_->WriteExpanded(id, expanded);
        return true;
    }

    bool InspectorSectionRegistry::ToggleExpanded(
        const std::string_view sectionId)
    {
        const auto* provider = Find(sectionId);
        if (provider == nullptr)
            return false;
        return SetExpanded(sectionId, !ResolveExpanded(provider->Descriptor()));
    }

    std::vector<InspectorSectionLayout>
    InspectorSectionRegistry::LayoutVisibleSections(
        const InspectorSectionContext& context,
        const float top,
        const float availableWidth)
    {
        std::vector<InspectorSectionLayout> result;
        result.reserve(providers_.size());

        float cursor = std::isfinite(top) ? top : 0.0f;
        const float width = IsFiniteNonNegative(availableWidth)
            ? availableWidth
            : 0.0f;

        for (const auto& provider : providers_)
        {
            if (provider == nullptr || !provider->IsVisible(context))
                continue;

            const auto& descriptor = provider->Descriptor();
            const bool expanded = ResolveExpanded(descriptor);
            float contentHeight = 0.0f;
            if (expanded)
            {
                contentHeight = provider->MeasureContentHeight(context, width);
                if (!IsFiniteNonNegative(contentHeight))
                    contentHeight = 0.0f;
            }

            InspectorSectionLayout layout;
            layout.id = descriptor.id;
            layout.top = cursor;
            layout.width = width;
            layout.headerHeight = descriptor.headerHeight;
            layout.contentTop = cursor + descriptor.headerHeight;
            layout.contentHeight = contentHeight;
            layout.totalHeight = descriptor.headerHeight + contentHeight +
                descriptor.spacingAfter;
            layout.expanded = expanded;

            provider->ApplyLayout(context, layout);
            result.push_back(layout);
            cursor += layout.totalHeight;
        }

        return result;
    }

    void InspectorSectionRegistry::RefreshVisibleSections(
        const InspectorSectionContext& context)
    {
        for (const auto& provider : providers_)
        {
            if (provider != nullptr && provider->IsVisible(context))
                provider->Refresh(context);
        }
    }

    bool InspectorSectionRegistry::IsValidSectionId(
        const std::string_view sectionId) noexcept
    {
        if (sectionId.empty())
            return false;
        for (const char c : sectionId)
        {
            const bool lower = c >= 'a' && c <= 'z';
            const bool digit = c >= '0' && c <= '9';
            if (!lower && !digit && c != '.' && c != '_' && c != '-')
                return false;
        }
        return true;
    }

    const IInspectorSectionProvider* InspectorSectionRegistry::Find(
        const std::string_view sectionId) const noexcept
    {
        const auto it = std::find_if(
            providers_.begin(), providers_.end(),
            [sectionId](const auto& provider)
            {
                return provider != nullptr && provider->Descriptor().id == sectionId;
            });
        return it != providers_.end() ? it->get() : nullptr;
    }

    IInspectorSectionProvider* InspectorSectionRegistry::Find(
        const std::string_view sectionId) noexcept
    {
        const auto it = std::find_if(
            providers_.begin(), providers_.end(),
            [sectionId](const auto& provider)
            {
                return provider != nullptr && provider->Descriptor().id == sectionId;
            });
        return it != providers_.end() ? it->get() : nullptr;
    }

    bool InspectorSectionRegistry::ResolveExpanded(
        const InspectorSectionDescriptor& descriptor) const
    {
        const auto cached = expansionCache_.find(descriptor.id);
        if (cached != expansionCache_.end())
            return cached->second;

        const bool expanded = preferences_ != nullptr
            ? preferences_->ReadExpanded(
                descriptor.id,
                descriptor.defaultExpanded)
            : descriptor.defaultExpanded;
        expansionCache_.emplace(descriptor.id, expanded);
        return expanded;
    }

    void InspectorSectionRegistry::SortProviders()
    {
        std::stable_sort(
            providers_.begin(), providers_.end(),
            [](const auto& left, const auto& right)
            {
                const auto& lhs = left->Descriptor();
                const auto& rhs = right->Descriptor();
                if (lhs.order != rhs.order)
                    return lhs.order < rhs.order;
                return lhs.id < rhs.id;
            });
    }
}
