from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# RenegadeStudioChrome.h
path = "Studio/src/RenegadeStudioChrome.h"
replace_once(
    path,
    "        void SetActiveBottomTab(int tab, bool notify = false);\n        void OnHierarchySelected(std::function<void(std::uint64_t)> callback);",
    "        void SetActiveBottomTab(int tab, bool notify = false);\n        void ResetDisclosureState();\n        [[nodiscard]] bool HasOpenMenuPopup() const noexcept\n        {\n            return activeMenu_ >= 0 || activeViewportMenu_ >= 0;\n        }\n        void OnHierarchySelected(std::function<void(std::uint64_t)> callback);",
)
replace_once(
    path,
    "        [[nodiscard]] bool HierarchyRowHasChildren(\n            std::size_t rowIndex) const noexcept;\n        void RebuildVisibleAssetFolders();",
    "        [[nodiscard]] bool HierarchyRowHasChildren(\n            std::size_t rowIndex) const noexcept;\n        void ResetHierarchyDisclosureToSelection();\n        void ResetAssetFolderDisclosureToSelection();\n        void RebuildVisibleAssetFolders();",
)

# RenegadeStudioChrome.cpp
path = "Studio/src/RenegadeStudioChrome.cpp"
replace_once(
    path,
    "        SetLayout(width_, height_);\n        SetShadowRadius(0.0f);",
    "        SetLayout(width_, height_);\n        ResetDisclosureState();\n        SetShadowRadius(0.0f);",
)

p = Path(path)
text = p.read_text(encoding="utf-8")
start = "    void RenegadeStudioChrome::SetHierarchyRows(\n        std::vector<HierarchyRow> rows)\n    {\n"
end = "    bool RenegadeStudioChrome::HierarchyRowHasChildren(\n"
a = text.find(start)
b = text.find(end, a)
if a < 0 or b < 0:
    raise RuntimeError("RenegadeStudioChrome.cpp: SetHierarchyRows boundary not found")
new_block = '''    void RenegadeStudioChrome::SetHierarchyRows(
        std::vector<HierarchyRow> rows)
    {
        hierarchyRows_ = std::move(rows);
        ResetHierarchyDisclosureToSelection();
        const auto selected = std::find_if(
            hierarchyRows_.begin(),
            hierarchyRows_.end(),
            [](const HierarchyRow& row)
            {
                return row.selected;
            });
        lastHierarchySelection_ = selected != hierarchyRows_.end()
            ? selected->entity
            : 0;
        SetHierarchyFilter(hierarchyFilter_);
    }

    void RenegadeStudioChrome::ResetHierarchyDisclosureToSelection()
    {
        collapsedHierarchyCategories_.fill(true);
        collapsedHierarchyEntities_.clear();
        initializedHierarchyDisclosureEntities_.clear();

        for (std::size_t index = 0; index + 1 < hierarchyRows_.size(); ++index)
        {
            const auto& row = hierarchyRows_[index];
            const auto& next = hierarchyRows_[index + 1];
            if (row.entity != 0 && row.category == next.category &&
                next.depth > row.depth)
            {
                collapsedHierarchyEntities_.insert(row.entity);
            }
        }

        const auto selected = std::find_if(
            hierarchyRows_.begin(),
            hierarchyRows_.end(),
            [](const HierarchyRow& row)
            {
                return row.selected;
            });
        if (selected == hierarchyRows_.end())
            return;

        collapsedHierarchyCategories_[
            static_cast<std::size_t>(selected->category)] = false;
        collapsedHierarchyEntities_.erase(selected->entity);

        const std::size_t selectedIndex = static_cast<std::size_t>(
            std::distance(hierarchyRows_.begin(), selected));
        int ancestorDepth = selected->depth;
        for (std::size_t index = selectedIndex; index-- > 0 && ancestorDepth > 0;)
        {
            const auto& ancestor = hierarchyRows_[index];
            if (ancestor.category == selected->category &&
                ancestor.depth < ancestorDepth)
            {
                collapsedHierarchyEntities_.erase(ancestor.entity);
                ancestorDepth = ancestor.depth;
            }
        }
    }

    void RenegadeStudioChrome::ResetAssetFolderDisclosureToSelection()
    {
        collapsedAssetFolders_.clear();
        for (std::size_t index = 0; index + 1 < assetBrowserFolders_.size(); ++index)
        {
            const auto& folder = assetBrowserFolders_[index];
            const auto& next = assetBrowserFolders_[index + 1];
            if (next.depth > folder.depth)
                collapsedAssetFolders_.insert(folder.relativePath);
        }

        const auto selected = std::find_if(
            assetBrowserFolders_.begin(),
            assetBrowserFolders_.end(),
            [](const AssetFolderRow& folder)
            {
                return folder.selected;
            });
        if (selected != assetBrowserFolders_.end())
        {
            for (auto item = collapsedAssetFolders_.begin();
                item != collapsedAssetFolders_.end();)
            {
                if (*item == selected->relativePath ||
                    IsAssetDescendant(selected->relativePath, *item))
                {
                    item = collapsedAssetFolders_.erase(item);
                }
                else
                {
                    ++item;
                }
            }
        }
        RebuildVisibleAssetFolders();
    }

    void RenegadeStudioChrome::ResetDisclosureState()
    {
        activeMenu_ = -1;
        activeViewportMenu_ = -1;
        ResetHierarchyDisclosureToSelection();
        SetHierarchyFilter(hierarchyFilter_);
        ResetAssetFolderDisclosureToSelection();
    }

'''
p.write_text(text[:a] + new_block + text[b:], encoding="utf-8")

replace_once(
    path,
    '''        for (const auto& folder : assetBrowserFolders_)
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
        RebuildVisibleAssetFolders();''',
    "        ResetAssetFolderDisclosureToSelection();",
)
replace_once(
    path,
    '''    void RenegadeStudioChrome::SetActiveBottomTab(
        const int tab,
        const bool notify)
    {
        activeBottomTab_ = std::clamp(tab, -1, 3);
        if (notify && drawerChanged_)
        {
            drawerChanged_(activeBottomTab_);
        }
    }''',
    '''    void RenegadeStudioChrome::SetActiveBottomTab(
        const int tab,
        const bool notify)
    {
        const int next = std::clamp(tab, -1, 3);
        if (next != activeBottomTab_)
        {
            activeBottomTab_ = next;
            ResetDisclosureState();
        }
        if (notify && drawerChanged_)
        {
            drawerChanged_(activeBottomTab_);
        }
    }''',
)
replace_once(
    path,
    '''                    if (item.header)
                    {
                        const auto categoryIndex =
                            static_cast<std::size_t>(item.category);
                        collapsedHierarchyCategories_[categoryIndex] =
                            !collapsedHierarchyCategories_[categoryIndex];
                        SetHierarchyFilter(hierarchyFilter_);
                    }''',
    '''                    if (item.header)
                    {
                        const auto categoryIndex =
                            static_cast<std::size_t>(item.category);
                        const bool opening =
                            collapsedHierarchyCategories_[categoryIndex];
                        collapsedHierarchyCategories_.fill(true);
                        if (opening)
                            collapsedHierarchyCategories_[categoryIndex] = false;
                        SetHierarchyFilter(hierarchyFilter_);
                    }''',
)
replace_once(
    path,
    '''                        if (HierarchyRowHasChildren(item.rowIndex))
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
                        }''',
    '''                        if (HierarchyRowHasChildren(item.rowIndex))
                        {
                            const bool opening =
                                collapsedHierarchyEntities_.count(row.entity) != 0;
                            if (opening)
                            {
                                ResetHierarchyDisclosureToSelection();
                                collapsedHierarchyCategories_.fill(true);
                                collapsedHierarchyCategories_[
                                    static_cast<std::size_t>(row.category)] = false;
                                collapsedHierarchyEntities_.erase(row.entity);
                                int ancestorDepth = row.depth;
                                for (std::size_t index = item.rowIndex;
                                    index-- > 0 && ancestorDepth > 0;)
                                {
                                    const auto& ancestor = hierarchyRows_[index];
                                    if (ancestor.category == row.category &&
                                        ancestor.depth < ancestorDepth)
                                    {
                                        collapsedHierarchyEntities_.erase(ancestor.entity);
                                        ancestorDepth = ancestor.depth;
                                    }
                                }
                            }
                            else if (!row.selected)
                            {
                                collapsedHierarchyEntities_.insert(row.entity);
                            }
                            SetHierarchyFilter(hierarchyFilter_);
                        }''',
)
replace_once(
    path,
    '''                        if (x < arrowEdge + 15.0f)
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
                        }''',
    '''                        if (x < arrowEdge + 15.0f)
                        {
                            const bool opening = collapsedAssetFolders_.count(
                                folder.relativePath) != 0;
                            if (opening)
                            {
                                collapsedAssetFolders_.clear();
                                for (std::size_t index = 0;
                                    index + 1 < assetBrowserFolders_.size(); ++index)
                                {
                                    const auto& candidate = assetBrowserFolders_[index];
                                    const auto& next = assetBrowserFolders_[index + 1];
                                    if (next.depth > candidate.depth)
                                        collapsedAssetFolders_.insert(candidate.relativePath);
                                }
                                for (auto item = collapsedAssetFolders_.begin();
                                    item != collapsedAssetFolders_.end();)
                                {
                                    if (*item == folder.relativePath ||
                                        IsAssetDescendant(folder.relativePath, *item))
                                    {
                                        item = collapsedAssetFolders_.erase(item);
                                    }
                                    else
                                    {
                                        ++item;
                                    }
                                }
                            }
                            else
                            {
                                collapsedAssetFolders_.insert(folder.relativePath);
                            }
                            RebuildVisibleAssetFolders();
                        }''',
)

# S1B Inspector accordion/reset policy
path = "Studio/src/S1BInspectorSectionMigration.cpp"
replace_once(
    path,
    '''            button.OnClick([this, sectionId](const wi::gui::EventArgs&)
            {
                if (inspectorSectionRegistry_.ToggleExpanded(sectionId))
                {
                    RefreshInspector();
                }
            });''',
    '''            button.OnClick([this, sectionId](const wi::gui::EventArgs&)
            {
                const bool opening =
                    !inspectorSectionRegistry_.IsExpanded(sectionId);
                ResetS1BInspectorDisclosure();
                if (opening)
                    (void)inspectorSectionRegistry_.SetExpanded(sectionId, true);
                RefreshInspector();
            });''',
)
replace_once(path, 'MakeSection("transform", "TRANSFORM", 10, true)', 'MakeSection("transform", "TRANSFORM", 10, false)')
replace_once(path, 'MakeSection("rendering", "RENDERING", 20, true)', 'MakeSection("rendering", "RENDERING", 20, false)')
replace_once(path, 'MakeSection("materials", "MATERIALS", 30, true)', 'MakeSection("materials", "MATERIALS", 30, false)')
replace_once(
    path,
    "    void StudioRenderPath::LayoutS1BInspectorSections()\n    {",
    '''    void StudioRenderPath::ResetS1BInspectorDisclosure()
    {
        for (const char* sectionId : {"transform", "rendering", "materials"})
            (void)inspectorSectionRegistry_.SetExpanded(sectionId, false);
    }

    void StudioRenderPath::LayoutS1BInspectorSections()
    {''',
)
replace_once(
    path,
    '''        const float panelWidth = inspectorPanel_.GetSize().x;
        const float innerWidth = std::max(1.0f, panelWidth - 24.0f);
        const bool hasSession = session_ != nullptr;
        const auto selected = hasSession && session_->Selection().HasSelection()
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;''',
    '''        const float panelWidth = inspectorPanel_.GetSize().x;
        const float innerWidth = std::max(1.0f, panelWidth - 24.0f);
        const bool hasSession = session_ != nullptr;
        const auto selected = hasSession && session_->Selection().HasSelection()
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        const std::uint64_t selectionRevision = selected != wi::ecs::INVALID_ENTITY
            ? static_cast<std::uint64_t>(selected)
            : 0u;
        const int viewToken = projectHubVisible_
            ? 1
            : environmentWorkspaceActive_ ? 2
            : terrainWorkspaceActive_ ? 3
            : renderWorkspaceActive_ ? 4
            : studioChrome_.IsAudioWorkspaceActive() ? 5
            : studioChrome_.IsPhysicsLabActive() ? 6
            : 16 + studioChrome_.ActiveBottomTab();
        if (selectionRevision != inspectorDisclosureSelectionRevision_ ||
            viewToken != inspectorDisclosureViewToken_)
        {
            ResetS1BInspectorDisclosure();
            inspectorDisclosureSelectionRevision_ = selectionRevision;
            inspectorDisclosureViewToken_ = viewToken;
        }''',
)

# StudioApplication.h
path = "Studio/src/StudioApplication.h"
replace_once(
    path,
    "        void CreateS1BInspectorSections();\n        void LayoutS1BInspectorSections();",
    "        void CreateS1BInspectorSections();\n        void ResetS1BInspectorDisclosure();\n        void LayoutS1BInspectorSections();",
)
replace_once(
    path,
    "        InspectorSectionRegistry inspectorSectionRegistry_;\n        SceneInspectorButton transformSectionHeader_;",
    "        InspectorSectionRegistry inspectorSectionRegistry_;\n        std::uint64_t inspectorDisclosureSelectionRevision_ =\n            static_cast<std::uint64_t>(-1);\n        int inspectorDisclosureViewToken_ = -1;\n        SceneInspectorButton transformSectionHeader_;",
)

# StudioApplication.cpp transitions
path = "Studio/src/StudioApplication.cpp"
replace_once(
    path,
    '''        studioChrome_.OnAction(
            [this](const RenegadeStudioChrome::Action action)
        {
            switch (action)
            {''',
    '''        studioChrome_.OnAction(
            [this](const RenegadeStudioChrome::Action action)
        {
            if (action == RenegadeStudioChrome::Action::ProjectHub ||
                action == RenegadeStudioChrome::Action::SceneWorkspace ||
                action == RenegadeStudioChrome::Action::EnvironmentWorkspace ||
                action == RenegadeStudioChrome::Action::TerrainWorkspace ||
                action == RenegadeStudioChrome::Action::RenderWorkspace)
            {
                ResetS1BInspectorDisclosure();
            }
            switch (action)
            {''',
)

# Audio popup ownership / workspace transitions
path = "Studio/src/RenegadePhysicsLabStudioChrome.cpp"
replace_once(
    path,
    '''                {
                    workspaceTransitionRequested_ = true;
                    SetPhysicsLabActive(false);
                    SetAudioWorkspaceActive(false);
                }''',
    '''                {
                    workspaceTransitionRequested_ = true;
                    SetActiveBottomTab(-1, true);
                    ResetDisclosureState();
                    SetPhysicsLabActive(false);
                    SetAudioWorkspaceActive(false);
                }''',
)
replace_once(
    path,
    '''        else if (AudioViewportToolHit(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))''',
    '''        else if (!HasOpenMenuPopup() &&
            AudioViewportToolHit(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))''',
)
replace_once(
    path,
    '''    void RenegadePhysicsLabStudioChrome::RenderAudioViewportTool(
        const wi::graphics::CommandList cmd) const
    {
        const XMFLOAT4 bounds = AudioViewportToolBounds();''',
    '''    void RenegadePhysicsLabStudioChrome::RenderAudioViewportTool(
        const wi::graphics::CommandList cmd) const
    {
        if (HasOpenMenuPopup())
            return;
        const XMFLOAT4 bounds = AudioViewportToolBounds();''',
)

# Source contract
Path("Tests/StudioDisclosurePolicySourceContract.cmake").write_text(
    r'''set(chrome_header "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.h")
set(chrome_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")
set(inspector_source "${RENEGADE_SOURCE_DIR}/Studio/src/S1BInspectorSectionMigration.cpp")
set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(audio_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")

foreach(required_file IN ITEMS
    "${chrome_header}"
    "${chrome_source}"
    "${inspector_source}"
    "${studio_header}"
    "${studio_source}"
    "${audio_source}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Studio disclosure policy file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${chrome_header}" chrome_header_text)
file(READ "${chrome_source}" chrome_text)
file(READ "${inspector_source}" inspector_text)
file(READ "${studio_header}" studio_header_text)
file(READ "${studio_source}" studio_text)
file(READ "${audio_source}" audio_text)

foreach(token IN ITEMS
    "ResetDisclosureState"
    "ResetHierarchyDisclosureToSelection"
    "ResetAssetFolderDisclosureToSelection"
    "HasOpenMenuPopup")
    string(FIND "${chrome_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio disclosure chrome contract is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "collapsedHierarchyCategories_.fill(true)"
    "ResetHierarchyDisclosureToSelection();"
    "ResetAssetFolderDisclosureToSelection();")
    string(FIND "${chrome_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio disclosure implementation is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "MakeSection(\"transform\", \"TRANSFORM\", 10, false)"
    "MakeSection(\"rendering\", \"RENDERING\", 20, false)"
    "MakeSection(\"materials\", \"MATERIALS\", 30, false)"
    "ResetS1BInspectorDisclosure"
    "inspectorDisclosureSelectionRevision_"
    "inspectorDisclosureViewToken_")
    string(FIND "${inspector_text}${studio_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio Inspector disclosure policy is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "ResetS1BInspectorDisclosure();"
    "Action::ProjectHub"
    "Action::SceneWorkspace")
    string(FIND "${studio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio view-reset seam is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "!HasOpenMenuPopup()"
    "if (HasOpenMenuPopup())"
    "ResetDisclosureState();")
    string(FIND "${audio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Audio/menu ownership fix is missing ${token}")
    endif()
endforeach()

foreach(forbidden IN ITEMS "lua_State" "renegade.events" "GLOBAL SCRIPT")
    string(FIND "${chrome_text}${inspector_text}${audio_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Studio UI consistency gate must not introduce scripting/runtime work: ${forbidden}")
    endif()
endforeach()
''',
    encoding="utf-8",
)

# Root CMake registration
path = "CMakeLists.txt"
replace_once(
    path,
    "    include(Tests/S1BInspectorMigration.cmake)\n    include(Tests/SceneUiGate2.cmake)",
    '''    include(Tests/S1BInspectorMigration.cmake)
    add_test(
        NAME RenegadeStudioDisclosurePolicySourceContract
        COMMAND ${CMAKE_COMMAND}
            -DRENEGADE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/StudioDisclosurePolicySourceContract.cmake
    )
    include(Tests/SceneUiGate2.cmake)''',
)
