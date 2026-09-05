#include "InspectorSectionFramework.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace
{
    using namespace renegade::studio;

    void Require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    class MemoryPreferenceStore final : public IInspectorSectionPreferenceStore
    {
    public:
        bool ReadExpanded(
            const std::string_view sectionId,
            const bool fallback) const override
        {
            const auto it = values.find(std::string(sectionId));
            return it != values.end() ? it->second : fallback;
        }

        void WriteExpanded(
            const std::string_view sectionId,
            const bool expanded) override
        {
            values[std::string(sectionId)] = expanded;
            ++writeCount;
        }

        std::unordered_map<std::string, bool> values;
        int writeCount = 0;
    };

    class TestProvider final : public IInspectorSectionProvider
    {
    public:
        TestProvider(
            std::string id,
            std::string title,
            const int order,
            const bool defaultExpanded,
            const float contentHeight,
            const bool selectionOnly = false)
            : contentHeight_(contentHeight),
              selectionOnly_(selectionOnly)
        {
            descriptor_.id = std::move(id);
            descriptor_.title = std::move(title);
            descriptor_.order = order;
            descriptor_.defaultExpanded = defaultExpanded;
            descriptor_.headerHeight = 20.0f;
            descriptor_.spacingAfter = 3.0f;
        }

        const InspectorSectionDescriptor& Descriptor() const noexcept override
        {
            return descriptor_;
        }

        bool IsVisible(const InspectorSectionContext& context) const override
        {
            return !selectionOnly_ || context.hasSelection;
        }

        float MeasureContentHeight(
            const InspectorSectionContext&,
            const float) const override
        {
            ++measureCount;
            return contentHeight_;
        }

        void Refresh(const InspectorSectionContext&) override
        {
            ++refreshCount;
        }

        void ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout) override
        {
            ++layoutCount;
            lastLayout = layout;
        }

        InspectorSectionDescriptor descriptor_;
        float contentHeight_ = 0.0f;
        bool selectionOnly_ = false;
        mutable int measureCount = 0;
        int refreshCount = 0;
        int layoutCount = 0;
        InspectorSectionLayout lastLayout;
    };

    void TestRegistrationAndStableIdentity()
    {
        InspectorSectionRegistry registry;
        std::string error;

        auto transform = std::make_shared<TestProvider>(
            "transform", "TRANSFORM", 10, true, 50.0f);
        Require(registry.Register(transform, error), "transform registration failed");
        Require(error.empty(), "successful registration returned an error");
        Require(registry.Count() == 1, "registry count mismatch");
        Require(registry.Contains("transform"), "registered section missing");

        auto duplicate = std::make_shared<TestProvider>(
            "transform", "OTHER", 20, true, 1.0f);
        Require(!registry.Register(duplicate, error), "duplicate ID was accepted");

        auto invalid = std::make_shared<TestProvider>(
            "Bad Section", "BAD", 30, true, 1.0f);
        Require(!registry.Register(invalid, error), "unstable section ID was accepted");

        Require(InspectorSectionRegistry::IsValidSectionId("collision.physics"),
            "valid stable section ID was rejected");
        Require(!InspectorSectionRegistry::IsValidSectionId("Collision Physics"),
            "uppercase/space section ID was accepted");
    }

    void TestDeterministicMeasuredLayout()
    {
        InspectorSectionRegistry registry;
        std::string error;
        auto materials = std::make_shared<TestProvider>(
            "materials", "MATERIALS", 30, true, 30.0f);
        auto transform = std::make_shared<TestProvider>(
            "transform", "TRANSFORM", 10, true, 50.0f);
        auto rendering = std::make_shared<TestProvider>(
            "rendering", "RENDERING", 20, false, 40.0f);

        Require(registry.Register(materials, error), "materials registration failed");
        Require(registry.Register(transform, error), "transform registration failed");
        Require(registry.Register(rendering, error), "rendering registration failed");

        InspectorSectionContext context;
        context.hasSelection = true;
        const auto layouts = registry.LayoutVisibleSections(context, 100.0f, 240.0f);
        Require(layouts.size() == 3, "unexpected visible layout count");
        Require(layouts[0].id == "transform", "transform ordering mismatch");
        Require(layouts[1].id == "rendering", "rendering ordering mismatch");
        Require(layouts[2].id == "materials", "materials ordering mismatch");

        Require(layouts[0].expanded, "transform should default expanded");
        Require(Near(layouts[0].contentHeight, 50.0f), "transform measurement mismatch");
        Require(Near(layouts[0].totalHeight, 73.0f), "transform total height mismatch");
        Require(Near(layouts[1].top, 173.0f), "second section top mismatch");

        Require(!layouts[1].expanded, "rendering should default collapsed");
        Require(Near(layouts[1].contentHeight, 0.0f), "collapsed content consumed height");
        Require(rendering->measureCount == 0, "collapsed provider was measured");
        Require(Near(layouts[1].totalHeight, 23.0f), "collapsed total height mismatch");
        Require(Near(layouts[2].top, 196.0f), "measured stacking mismatch");
    }

    void TestContextVisibilityAndRefresh()
    {
        InspectorSectionRegistry registry;
        std::string error;
        auto always = std::make_shared<TestProvider>(
            "level", "LEVEL", 10, true, 10.0f);
        auto selected = std::make_shared<TestProvider>(
            "selection", "SELECTION", 20, true, 10.0f, true);
        Require(registry.Register(always, error), "always provider registration failed");
        Require(registry.Register(selected, error), "selection provider registration failed");

        InspectorSectionContext context;
        context.hasSelection = false;
        auto layouts = registry.LayoutVisibleSections(context, 0.0f, 200.0f);
        Require(layouts.size() == 1 && layouts[0].id == "level",
            "context predicate did not hide selection-only provider");
        registry.RefreshVisibleSections(context);
        Require(always->refreshCount == 1, "visible provider was not refreshed");
        Require(selected->refreshCount == 0, "hidden provider was refreshed");

        context.hasSelection = true;
        layouts = registry.LayoutVisibleSections(context, 0.0f, 200.0f);
        Require(layouts.size() == 2, "selection context did not reveal provider");
    }

    void TestExpansionPersistence()
    {
        MemoryPreferenceStore preferences;
        preferences.values["rendering"] = false;

        std::string error;
        InspectorSectionRegistry first(&preferences);
        auto rendering = std::make_shared<TestProvider>(
            "rendering", "RENDERING", 10, true, 25.0f);
        Require(first.Register(rendering, error), "rendering registration failed");
        Require(!first.IsExpanded("rendering"), "persisted collapse state was ignored");
        Require(first.SetExpanded("rendering", true), "set expanded failed");
        Require(preferences.values["rendering"], "expanded state was not persisted");
        Require(preferences.writeCount == 1, "preference write count mismatch");

        InspectorSectionRegistry reopened(&preferences);
        auto reopenedRendering = std::make_shared<TestProvider>(
            "rendering", "RENDERING", 10, false, 25.0f);
        Require(reopened.Register(reopenedRendering, error), "reopened registration failed");
        Require(reopened.IsExpanded("rendering"),
            "persisted expansion did not survive registry recreation");
        Require(reopened.ToggleExpanded("rendering"), "toggle failed");
        Require(!reopened.IsExpanded("rendering"), "toggle did not collapse section");
    }

    void TestProjectPreferenceKeyContract()
    {
        Require(
            ProjectInspectorSectionPreferenceStore::PreferenceKey("materials") ==
                "inspector.section.materials.expanded",
            "ProjectService preference key contract changed");
    }
}

int main()
{
    try
    {
        TestRegistrationAndStableIdentity();
        TestDeterministicMeasuredLayout();
        TestContextVisibilityAndRefresh();
        TestExpansionPersistence();
        TestProjectPreferenceKeyContract();
    }
    catch (const std::exception& error)
    {
        std::cerr << "S1A Inspector section framework test failure: "
                  << error.what() << '\n';
        return 1;
    }

    std::cout << "S1A Inspector section framework tests passed.\n";
    return 0;
}
