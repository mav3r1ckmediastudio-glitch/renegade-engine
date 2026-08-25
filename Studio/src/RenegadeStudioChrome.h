#pragma once

#include <array>
#include <string>
#include <functional>
#include <cstdint>
#include <vector>
#include <unordered_set>

#include <WickedEngine.h>

#include "renegade/bridge/AssetCatalogueService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"

namespace renegade::studio
{
    class RenegadeTextInputField final : public wi::gui::TextInputField
    {
    public:
        void SetPlaceholder(std::string placeholder);
        void SetRenderTextSize(int size) noexcept;
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeTextInputField"; }

    private:
        std::string placeholder_;
        int renderTextSize_ = 10;
    };

    class RenegadeButton final : public wi::gui::Button
    {
    public:
        void SetRenderTextSize(int size) noexcept;
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeButton"; }

    private:
        int renderTextSize_ = 10;
    };

    class RenegadeCheckBox final : public wi::gui::CheckBox
    {
    public:
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeCheckBox"; }
    };

    class RenegadeComboBox final : public wi::gui::ComboBox
    {
    public:
        void SetRenderTextSize(int size) noexcept;
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeComboBox"; }

    private:
        int renderTextSize_ = 10;
    };

    class RenegadeSlider final : public wi::gui::Slider
    {
    public:
        void Create(
            float minimum,
            float maximum,
            float defaultValue,
            float steps,
            const std::string& name,
            std::string label);
        void OnDragStarted(std::function<void(float)> callback);
        void OnValuePreview(std::function<void(float)> callback);
        void OnValueCommitted(std::function<void(float)> callback);
        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeSlider";
        }

    private:
        std::string label_;
        float valueBeforeUpdate_ = 0.0f;
        bool dragging_ = false;
        std::function<void(float)> dragStarted_;
        std::function<void(float)> valuePreview_;
        std::function<void(float)> valueCommitted_;
    };

    class RenegadeTextureMapList final : public wi::gui::Widget
    {
    public:
        static constexpr std::size_t SlotCount = 7;

        void ClearSlots();
        void SetSlot(
            std::size_t index,
            const wi::Resource& resource,
            std::string path);
        void SetSelectedSlot(std::size_t index);
        void OnSlotSelected(std::function<void(std::size_t)> callback);
        void OnBrowseRequested(std::function<void(std::size_t)> callback);
        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeTextureMapList";
        }

    private:
        std::array<wi::Resource, SlotCount> resources_;
        std::array<std::string, SlotCount> paths_;
        std::size_t selectedSlot_ = 0;
        std::size_t hoveredSlot_ = SlotCount;
        std::function<void(std::size_t)> slotSelected_;
        std::function<void(std::size_t)> browseRequested_;
    };

    class RenegadeStudioChrome : public wi::gui::Widget
    {
    public:
        enum class HierarchyCategory : std::uint8_t
        {
            Lights,
            Models,
            Characters,
            Cameras,
            Terrain,
            Effects,
            Audio,
            Other,
            Count,
        };

        enum class TestLevelState : std::uint8_t
        {
            Idle,
            Starting,
            Running,
        };

        enum class Action
        {
            ProjectHub,
            OpenScene,
            Save,
            SaveAs,
            Reopen,
            Undo,
            Redo,
            Duplicate,
            Delete,
            CreatePointLight,
            CreateSpotLight,
            CreateDirectionalLight,
            CreateRectangleLight,
            ImportModel,
            Focus,
            ToggleGrid,
            EnvironmentWorkspace,
            TerrainWorkspace,
            SceneWorkspace,
            TestLevelPlay,
            TestLevelStop,
            ValidateModelImport,
        };

        struct HierarchyRow
        {
            std::string name;
            int depth = 0;
            bool selected = false;
            std::uint64_t entity = 0;
            HierarchyCategory category = HierarchyCategory::Other;
        };

        struct AssetFolderRow
        {
            std::string name;
            std::string relativePath;
            int depth = 0;
            bool selected = false;
        };

        struct AssetCard
        {
            std::string name;
            std::string relativePath;
            std::string typeLabel;
            wi::Resource thumbnail;
            bool directory = false;
        };

        void Create();
        void SetLayout(float width, float height);
        void SetHierarchyRows(std::vector<HierarchyRow> rows);
        void SetAssetBrowserData(
            std::vector<AssetFolderRow> folders,
            std::vector<AssetCard> assets,
            std::string currentPath);
        void SetAssetBrowserSelectedPath(std::string relativePath);
        void SetSceneName(std::string sceneName);
        void SetSceneDirty(bool dirty) noexcept;
        void SetStatusText(std::string statusText);
        void SetSelectionName(std::string selectionName);
        void SetActiveTool(int toolIndex) noexcept;
        void SetHierarchyFilter(std::string filter);
        void SetGridVisible(bool visible) noexcept;
        void SetEnvironmentWorkspaceActive(bool active) noexcept;
        void SetTerrainWorkspaceActive(bool active) noexcept;
        void SetTestLevelState(TestLevelState state) noexcept;
        void SetPanelSizes(
            float hierarchyWidth,
            float inspectorWidth,
            float drawerHeight) noexcept;
        void SetActiveBottomTab(int tab, bool notify = false);
        void OnHierarchySelected(std::function<void(std::uint64_t)> callback);
        void OnToolSelected(std::function<void(int)> callback);
        void OnAction(std::function<void(Action)> callback);
        void OnDrawerChanged(std::function<void(int)> callback);
        void OnAssetBrowserFolderSelected(
            std::function<void(const std::string&)> callback);
        void OnAssetBrowserItemSelected(
            std::function<void(const std::string&)> callback);
        void OnAssetBrowserItemDropped(
            std::function<void(
                const std::string&,
                float,
                float)> callback);
        void OnLayoutChanged(
            std::function<void(float, float, float, bool)> callback);

        [[nodiscard]] XMFLOAT4 ViewportBounds() const noexcept;
        [[nodiscard]] float HierarchyWidth() const noexcept;
        [[nodiscard]] float InspectorWidth() const noexcept;
        [[nodiscard]] float DrawerHeight() const noexcept;
        [[nodiscard]] int ActiveBottomTab() const noexcept { return activeBottomTab_; }
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;
        [[nodiscard]] bool AssetBrowserDragCandidate() const noexcept
        {
            return assetBrowserDragCandidate_;
        }
        [[nodiscard]] bool AssetBrowserDragging() const noexcept
        {
            return assetBrowserDragging_;
        }
        [[nodiscard]] const std::string& AssetBrowserDragPath() const noexcept
        {
            return assetBrowserDragPath_;
        }

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStudioChrome";
        }

    private:
        struct VisibleHierarchyItem
        {
            bool header = false;
            HierarchyCategory category = HierarchyCategory::Other;
            std::size_t rowIndex = 0;
        };

        void RebuildVisibleAssetFolders();
        void RenderAssetBrowser(
            float drawerTop,
            float inspectorX,
            wi::graphics::CommandList cmd) const;

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        float hierarchyWidth_ = 320.0f;
        float inspectorWidth_ = 360.0f;
        float drawerHeight_ = 238.0f;
        bool layoutInitialized_ = false;
        int resizingPanel_ = 0;
        int hoveredSplitter_ = 0;
        int activeTool_ = 1;
        int activeBottomTab_ = -1;
        int activeMenu_ = -1;
        int activeViewportMenu_ = -1;
        bool gridVisible_ = true;
        bool environmentWorkspaceActive_ = false;
        bool terrainWorkspaceActive_ = false;
        TestLevelState testLevelState_ = TestLevelState::Idle;
        bool pointerConsumed_ = false;
        wi::Resource brandLockup_;
        std::vector<HierarchyRow> hierarchyRows_;
        std::vector<VisibleHierarchyItem> visibleHierarchyRows_;
        std::vector<AssetFolderRow> assetBrowserFolders_;
        std::vector<std::size_t> visibleAssetFolderRows_;
        std::vector<AssetCard> assetBrowserAssets_;
        std::unordered_set<std::string> collapsedAssetFolders_;
        std::array<bool,
            static_cast<std::size_t>(HierarchyCategory::Count)>
            collapsedHierarchyCategories_ = {};
        std::size_t hierarchyScrollRow_ = 0;
        std::size_t assetBrowserFolderScrollRow_ = 0;
        std::size_t assetBrowserAssetScrollRow_ = 0;
        std::uint64_t lastHierarchySelection_ = 0;
        std::string sceneName_ = "PROVING GROUND";
        bool sceneDirty_ = false;
        std::string statusText_ = "STUDIO READY";
        std::string selectionName_;
        std::string hierarchyFilter_;
        std::string assetBrowserCurrentPath_ = "Content";
        std::string assetBrowserSelectedPath_;
        bool assetBrowserFoldersVisible_ = true;
        float fpsSampleTime_ = 0.0f;
        float displayedFps_ = 0.0f;
        std::uint32_t fpsSampleFrames_ = 0;
        std::function<void(std::uint64_t)> hierarchySelected_;
        std::function<void(int)> toolSelected_;
        std::function<void(Action)> action_;
        std::function<void(int)> drawerChanged_;
        std::function<void(const std::string&)>
            assetBrowserFolderSelected_;
        std::function<void(const std::string&)>
            assetBrowserItemSelected_;
        std::function<void(const std::string&, float, float)>
            assetBrowserItemDropped_;
        std::string assetBrowserDragPath_;
        XMFLOAT2 assetBrowserDragStart_ = {};
        bool assetBrowserDragCandidate_ = false;
        bool assetBrowserDragging_ = false;
        std::function<void(float, float, float, bool)> layoutChanged_;
    };

    namespace detail
    {
        void RequestCreatorAssetDragPreparation(
            const bridge::StableId& assetId,
            const std::string& assetPath);
        void WarmCreatorAssetDragPreparation(
            const bridge::StableId& assetId,
            const std::string& assetPath);
        void PrimeCreatorAssetDragPreparation(
            const bridge::StableId& assetId,
            const std::string& assetPath,
            bridge::PreparedReusableModelPlacement prepared);
        void QueueCreatorAssetDrop(
            const bridge::StableId& assetId,
            const std::string& assetPath,
            float screenX,
            float screenY);
        [[nodiscard]] bool CreatorAssetDragPreviewOwnsDrop(
            const bridge::StableId& assetId) noexcept;
        [[nodiscard]] wi::ecs::Entity UpdateCreatorAssetDragPreview(
            const wi::Canvas& canvas,
            const wi::scene::CameraComponent& camera);
        void ClearCreatorAssetDragPreview();
        [[nodiscard]] bool CreatorAssetDragPreviewBlocksSave() noexcept;
    }

    // LP07 Gate 5 overlays the creator Asset Browser lifecycle on Renegade's
    // existing custom chrome. The base chrome remains the rendering/interaction
    // authority for the rest of Studio; this subtype only intercepts the
    // existing Import Model and asset-card callbacks and adds creator controls
    // inside the already-owned Assets drawer.
    class CreatorAssetStudioChrome final : public RenegadeStudioChrome
    {
    public:
        CreatorAssetStudioChrome()
        {
            current_ = this;
        }
        ~CreatorAssetStudioChrome() override
        {
            detail::ClearCreatorAssetDragPreview();
            if (current_ == this)
                current_ = nullptr;
        }

        [[nodiscard]] static CreatorAssetStudioChrome* Current() noexcept
        {
            return current_;
        }
        [[nodiscard]] const bridge::StableId& SelectedCreatorAssetId() const noexcept
        {
            return creatorSelectedAssetId_;
        }
        [[nodiscard]] const std::string& SelectedCreatorAssetPath() const noexcept
        {
            return creatorSelectedAssetPath_;
        }

        void Create();
        void SetLayout(float width, float height);
        void SetAssetBrowserData(
            std::vector<AssetFolderRow> folders,
            std::vector<AssetCard> assets,
            std::string currentPath);
        void SetActiveBottomTab(int tab, bool notify = false);
        void OnAction(std::function<void(Action)> callback);
        void OnAssetBrowserItemSelected(
            std::function<void(const std::string&)> callback);
        void OnCreatorAssetPlaceRequested(
            std::function<void(
                const bridge::StableId&,
                const std::string&)> callback);
        void OnCreatorAssetDropped(
            std::function<void(
                const bridge::StableId&,
                const std::string&,
                float,
                float)> callback);
        [[nodiscard]] bool RevealCreatorAsset(
            const bridge::StableId& assetId,
            const std::string& relativePath,
            std::string& error);
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "CreatorAssetStudioChrome";
        }

    private:
        void CreateCreatorAssetControls();
        void LayoutCreatorAssetControls();
        void UpdateCreatorAssetControls(
            const wi::Canvas& canvas,
            float dt);
        void RenderCreatorAssetControls(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const;
        void RefreshCreatorAssetBrowser();
        bool SelectCreatorAsset(const std::string& relativePath);
        void ImportCreatorModel();
        void PlaceSelectedCreatorAsset();
        void ReimportSelectedCreatorAsset();
        void SaveSelectedCreatorTags();
        void RefreshCreatorHierarchyRows();
        [[nodiscard]] bridge::AssetCatalogueQuery CreatorAssetQuery() const;
        [[nodiscard]] std::vector<std::string> CreatorTagInput() const;

        inline static CreatorAssetStudioChrome* current_ = nullptr;
        bridge::CreatorAssetWorkflowService creatorAssetWorkflow_;
        bridge::AssetCatalogue creatorAssetCatalogue_;
        bridge::StableId creatorCatalogueProjectId_;
        bridge::StableId creatorSelectedAssetId_;
        std::string creatorSelectedAssetPath_;
        bool creatorAssetRefreshPending_ = true;
        bool creatorAssetCatalogueDirty_ = true;
        bool creatorAssetControlConsumed_ = false;
        std::string creatorAssetLastSearch_;
        int creatorAssetStateFilter_ = 0;
        int creatorAssetFormatFilter_ = 0;
        int creatorAssetRigFilter_ = 0;
        std::function<void(Action)> creatorAction_;
        std::function<void(const bridge::StableId&, const std::string&)>
            creatorAssetPlaceRequested_;
        std::function<void(
            const bridge::StableId&,
            const std::string&,
            float,
            float)> creatorAssetDropped_;
        wi::jobsystem::context creatorAssetWorkload_;
        RenegadeTextInputField creatorAssetSearch_;
        RenegadeTextInputField creatorAssetTags_;
        RenegadeComboBox creatorAssetStateCombo_;
        RenegadeComboBox creatorAssetFormatCombo_;
        RenegadeComboBox creatorAssetRigCombo_;
        RenegadeButton creatorAssetImportButton_;
        RenegadeButton creatorAssetPlaceButton_;
        RenegadeButton creatorAssetReimportButton_;
        RenegadeButton creatorAssetSaveTagsButton_;
        float creatorLayoutWidth_ = 1920.0f;
        float creatorLayoutHeight_ = 1080.0f;
        std::vector<AssetFolderRow> creatorFilesystemFolders_;
        std::vector<AssetCard> creatorFilesystemAssets_;
        std::vector<std::string> creatorVisibleAssetPaths_;
        std::unordered_set<std::string> creatorVisibleThumbnailPaths_;
        std::string creatorCurrentPath_ = "Content";
    };
}
