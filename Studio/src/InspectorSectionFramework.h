#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace renegade::bridge
{
    class ProjectService;
}

namespace renegade::studio
{
    // S1A intentionally keeps the framework independent of Wicked widgets.
    // Providers may wrap wiGUI controls in S1B+, but the registry owns only
    // section identity, ordering, visibility, expansion and measured layout.
    struct InspectorSectionContext final
    {
        bool hasSelection = false;
        std::uint64_t selectionRevision = 0;
        const void* userData = nullptr;
    };

    struct InspectorSectionDescriptor final
    {
        std::string id;
        std::string title;
        int order = 0;
        bool defaultExpanded = true;
        float headerHeight = 28.0f;
        float spacingAfter = 4.0f;
    };

    struct InspectorSectionLayout final
    {
        std::string id;
        float top = 0.0f;
        float width = 0.0f;
        float headerHeight = 0.0f;
        float contentTop = 0.0f;
        float contentHeight = 0.0f;
        float totalHeight = 0.0f;
        bool expanded = false;
    };

    class IInspectorSectionPreferenceStore
    {
    public:
        virtual ~IInspectorSectionPreferenceStore() = default;

        [[nodiscard]] virtual bool ReadExpanded(
            std::string_view sectionId,
            bool fallback) const = 0;
        virtual void WriteExpanded(
            std::string_view sectionId,
            bool expanded) = 0;
    };

    // Adapter over the existing editor-preference authority. Expansion is a
    // creator Studio preference; it must never become scene/project content.
    class ProjectInspectorSectionPreferenceStore final
        : public IInspectorSectionPreferenceStore
    {
    public:
        explicit ProjectInspectorSectionPreferenceStore(
            bridge::ProjectService& projects) noexcept;

        [[nodiscard]] bool ReadExpanded(
            std::string_view sectionId,
            bool fallback) const override;
        void WriteExpanded(
            std::string_view sectionId,
            bool expanded) override;

        [[nodiscard]] static std::string PreferenceKey(
            std::string_view sectionId);

    private:
        bridge::ProjectService* projects_ = nullptr;
    };

    class IInspectorSectionProvider
    {
    public:
        virtual ~IInspectorSectionProvider() = default;

        [[nodiscard]] virtual const InspectorSectionDescriptor& Descriptor()
            const noexcept = 0;
        [[nodiscard]] virtual bool IsVisible(
            const InspectorSectionContext& context) const = 0;
        [[nodiscard]] virtual float MeasureContentHeight(
            const InspectorSectionContext& context,
            float availableWidth) const = 0;

        // Providers retain ownership of their controls and command-backed edit
        // callbacks. The framework only asks them to refresh and apply bounds;
        // it never writes scene state directly and therefore cannot bypass the
        // existing CommandService Undo/Redo boundary.
        virtual void Refresh(const InspectorSectionContext& context) = 0;
        virtual void ApplyLayout(
            const InspectorSectionContext& context,
            const InspectorSectionLayout& layout) = 0;
    };

    class InspectorSectionRegistry final
    {
    public:
        explicit InspectorSectionRegistry(
            IInspectorSectionPreferenceStore* preferences = nullptr) noexcept;

        void SetPreferenceStore(
            IInspectorSectionPreferenceStore* preferences) noexcept;

        [[nodiscard]] bool Register(
            std::shared_ptr<IInspectorSectionProvider> provider,
            std::string& error);
        [[nodiscard]] bool Unregister(std::string_view sectionId) noexcept;
        void Clear() noexcept;

        [[nodiscard]] bool Contains(std::string_view sectionId) const noexcept;
        [[nodiscard]] std::size_t Count() const noexcept;

        [[nodiscard]] bool IsExpanded(std::string_view sectionId) const;
        [[nodiscard]] bool SetExpanded(
            std::string_view sectionId,
            bool expanded);
        [[nodiscard]] bool ToggleExpanded(std::string_view sectionId);

        // Visible sections are returned in deterministic descriptor order.
        // Collapsed sections consume header height only. Expanded content is
        // measured by the provider, eliminating the fixed global Y-offset
        // model that S1B will retire from the existing Inspector.
        [[nodiscard]] std::vector<InspectorSectionLayout> LayoutVisibleSections(
            const InspectorSectionContext& context,
            float top,
            float availableWidth);

        void RefreshVisibleSections(const InspectorSectionContext& context);

        [[nodiscard]] static bool IsValidSectionId(
            std::string_view sectionId) noexcept;

    private:
        [[nodiscard]] const IInspectorSectionProvider* Find(
            std::string_view sectionId) const noexcept;
        [[nodiscard]] IInspectorSectionProvider* Find(
            std::string_view sectionId) noexcept;
        [[nodiscard]] bool ResolveExpanded(
            const InspectorSectionDescriptor& descriptor) const;
        void SortProviders();

        IInspectorSectionPreferenceStore* preferences_ = nullptr;
        std::vector<std::shared_ptr<IInspectorSectionProvider>> providers_;
        mutable std::unordered_map<std::string, bool> expansionCache_;
    };
}
