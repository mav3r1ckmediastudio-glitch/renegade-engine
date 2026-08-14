#include "RenegadeStudioChrome.h"

#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceAssetService.h"
#include "renegade/bridge/ResourceImportService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <sstream>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr float BottomTabsHeight = 32.0f;
    constexpr float StatusBarHeight = 28.0f;

    void DrawSolidRect(
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

    std::string UpperAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            });
        return value;
    }

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char c)
        {
            return std::isspace(c) != 0;
        };
        while (!value.empty() && whitespace(value.front()))
            value.erase(value.begin());
        while (!value.empty() && whitespace(value.back()))
            value.pop_back();
        return value;
    }

    std::string InputValue(const renegade::studio::RenegadeTextInputField& input)
    {
        return const_cast<renegade::studio::RenegadeTextInputField&>(input).GetValue();
    }

    std::string ParentPath(const std::string& relativePath)
    {
        return fs::u8path(relativePath).parent_path().lexically_normal().generic_u8string();
    }

    std::string JoinTags(const std::vector<std::string>& tags)
    {
        std::ostringstream stream;
        for (std::size_t i = 0; i < tags.size(); ++i)
        {
            if (i != 0)
                stream << ", ";
            stream << tags[i];
        }
        return stream.str();
    }

    renegade::studio::RenegadeStudioChrome::HierarchyCategory MapCategory(
        const renegade::bridge::SceneEntityCategory category)
    {
        using Source = renegade::bridge::SceneEntityCategory;
        using Target = renegade::studio::RenegadeStudioChrome::HierarchyCategory;
        switch (category)
        {
        case Source::Lights: return Target::Lights;
        case Source::Models: return Target::Models;
        case Source::Characters: return Target::Characters;
        case Source::Cameras: return Target::Cameras;
        case Source::Terrain: return Target::Terrain;
        case Source::Effects: return Target::Effects;
        case Source::Audio: return Target::Audio;
        default: return Target::Other;
        }
    }
}

namespace renegade::studio
{
    void CreatorAssetStudioChrome::Create()
    {
        RenegadeStudioChrome::Create();
        CreateCreatorAssetControls();
        creatorAssetRefreshPending_ = true;
        creatorAssetCatalogueDirty_ = true;
    }

    void CreatorAssetStudioChrome::SetLayout(const float width, const float height)
    {
        creatorLayoutWidth_ = width;
        creatorLayoutHeight_ = height;
        RenegadeStudioChrome::SetLayout(width, height);
        LayoutCreatorAssetControls();
    }

    void CreatorAssetStudioChrome::SetAssetBrowserData(
        std::vector<AssetFolderRow> folders,
        std::vector<AssetCard> assets,
        std::string currentPath)
    {
        const bool folderChanged = currentPath != creatorCurrentPath_;
        creatorFilesystemFolders_ = folders;
        creatorFilesystemAssets_ = assets;
        creatorCurrentPath_ = currentPath;
        RenegadeStudioChrome::SetAssetBrowserData(
            std::move(folders), std::move(assets), std::move(currentPath));
        if (folderChanged || creatorAssetCatalogue_.entries.empty())
            creatorAssetRefreshPending_ = true;
    }

    void CreatorAssetStudioChrome::SetActiveBottomTab(const int tab, const bool notify)
    {
        RenegadeStudioChrome::SetActiveBottomTab(tab, notify);
        if (tab == 0)
        {
            creatorAssetRefreshPending_ = true;
            creatorAssetCatalogueDirty_ = true;
        }
    }

    void CreatorAssetStudioChrome::OnAction(std::function<void(Action)> callback)
    {
        // ADD > IMPORT MODEL belongs to StudioApplication's guided
        // creator workflow. LP07 previously intercepted this action
        // here and auto-imported/placed the model, bypassing the
        // dedicated import workspace entirely.
        creatorAction_ = std::move(callback);
        RenegadeStudioChrome::OnAction(
            [this](const Action action)
            {
                if (creatorAction_)
                    creatorAction_(action);
            });
    }

    void CreatorAssetStudioChrome::OnAssetBrowserItemSelected(
        std::function<void(const std::string&)> callback)
    {
        RenegadeStudioChrome::OnAssetBrowserItemSelected(
            [this, callback = std::move(callback)](const std::string& path) mutable
            {
                if (!SelectCreatorAsset(path) && callback)
                    callback(path);
            });
    }

    void CreatorAssetStudioChrome::OnCreatorAssetPlaceRequested(
        std::function<void(
            const bridge::StableId&,
            const std::string&)> callback)
    {
        creatorAssetPlaceRequested_ = std::move(callback);
    }

    void CreatorAssetStudioChrome::OnCreatorAssetDropped(
        std::function<void(
            const bridge::StableId&,
            const std::string&,
            float,
            float)> callback)
    {
        creatorAssetDropped_ = std::move(callback);
        RenegadeStudioChrome::OnAssetBrowserItemDropped(
            [this](const std::string& path, const float x, const float y)
            {
                if (!SelectCreatorAsset(path) ||
                    !bridge::IsValidStableId(creatorSelectedAssetId_) ||
                    !creatorAssetDropped_)
                    return;
                const auto selected = std::find_if(
                    creatorAssetCatalogue_.entries.begin(),
                    creatorAssetCatalogue_.entries.end(),
                    [this](const bridge::AssetCatalogueEntry& entry)
                    {
                        return entry.assetId == creatorSelectedAssetId_;
                    });
                if (selected == creatorAssetCatalogue_.entries.end() ||
                    !bridge::CanPlaceCreatorModelAsset(*selected))
                {
                    SetStatusText(
                        "PLACE ASSET // DRAG A CURRENT MODEL ASSET");
                    return;
                }
                creatorAssetDropped_(
                    creatorSelectedAssetId_,
                    fs::u8path(path).filename().generic_u8string(),
                    x,
                    y);
            });
    }

    bool CreatorAssetStudioChrome::RevealCreatorAsset(
        const bridge::StableId& assetId,
        const std::string& relativePath,
        std::string& error)
    {
        if (!bridge::IsValidStableId(assetId) || relativePath.empty())
        {
            error = "The imported asset did not provide a valid browser identity.";
            return false;
        }

        const std::string normalizedPath =
            fs::u8path(relativePath).lexically_normal().generic_u8string();
        creatorCurrentPath_ = ParentPath(normalizedPath);
        creatorSelectedAssetId_ = assetId;
        creatorSelectedAssetPath_ = normalizedPath;
        creatorAssetSearch_.SetValue("");
        creatorAssetLastSearch_.clear();
        creatorAssetStateFilter_ = 0;
        creatorAssetFormatFilter_ = 0;
        creatorAssetRigFilter_ = 0;
        creatorAssetStateCombo_.SetSelectedWithoutCallback(0);
        creatorAssetFormatCombo_.SetSelectedWithoutCallback(0);
        creatorAssetRigCombo_.SetSelectedWithoutCallback(0);
        creatorAssetCatalogueDirty_ = true;
        creatorAssetRefreshPending_ = false;
        RefreshCreatorAssetBrowser();

        const auto found = std::find_if(
            creatorAssetCatalogue_.entries.begin(),
            creatorAssetCatalogue_.entries.end(),
            [&assetId, &normalizedPath](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.assetId == assetId &&
                    fs::u8path(entry.projectRelativePath)
                        .lexically_normal().generic_u8string() == normalizedPath;
            });
        if (found == creatorAssetCatalogue_.entries.end())
        {
            error = "The governed asset committed, but the Asset Browser catalogue did not expose its stable ID and path.";
            creatorAssetRefreshPending_ = true;
            return false;
        }
        if (!found->registered || !found->importedProduct ||
            !found->productAvailable ||
            !bridge::CanPlaceCreatorModelAsset(*found))
        {
            error = "The governed asset committed, but its Asset Browser entry is not a current placeable model product.";
            creatorAssetRefreshPending_ = true;
            return false;
        }

        const auto visibleCard = std::find_if(
            creatorVisibleAssetPaths_.begin(),
            creatorVisibleAssetPaths_.end(),
            [&normalizedPath](const std::string& card)
            {
                return fs::u8path(card)
                    .lexically_normal().generic_u8string() == normalizedPath;
            });
        if (visibleCard == creatorVisibleAssetPaths_.end())
        {
            error = "The governed asset is registered but its destination folder is not visible in the Asset Browser.";
            creatorAssetRefreshPending_ = true;
            return false;
        }
        if (creatorVisibleThumbnailPaths_.count(normalizedPath) == 0)
        {
            error = "The governed asset card is visible, but its captured thumbnail could not be loaded.";
            creatorAssetRefreshPending_ = true;
            return false;
        }

        SetAssetBrowserSelectedPath(normalizedPath);
        error.clear();
        return true;
    }

    bool CreatorAssetStudioChrome::ConsumedPointerThisFrame() const noexcept
    {
        return creatorAssetControlConsumed_ ||
            RenegadeStudioChrome::ConsumedPointerThisFrame();
    }

    void CreatorAssetStudioChrome::Update(const wi::Canvas& canvas, const float dt)
    {
        RenegadeStudioChrome::Update(canvas, dt);
        UpdateCreatorAssetControls(canvas, dt);
    }

    void CreatorAssetStudioChrome::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        RenegadeStudioChrome::Render(canvas, cmd);
        RenderCreatorAssetControls(canvas, cmd);
    }

    void CreatorAssetStudioChrome::CreateCreatorAssetControls()
    {
        creatorAssetSearch_.Create("Creator Asset Search");
        creatorAssetSearch_.SetDescription("SEARCH  ");
        creatorAssetSearch_.SetValue("");
        creatorAssetSearch_.SetPlaceholder("NAME / TAG / STATE / ID...");
        creatorAssetSearch_.SetTooltip(
            "Search registered asset name, path, stable ID, source format, importer, state and creator tags.");
        creatorAssetSearch_.SetCancelInputEnabled(false);
        creatorAssetSearch_.OnInput([this](const wi::gui::EventArgs&)
        {
            creatorAssetRefreshPending_ = true;
        });
        creatorAssetSearch_.OnInputAccepted([this](const wi::gui::EventArgs&)
        {
            creatorAssetRefreshPending_ = true;
        });

        creatorAssetTags_.Create("Creator Asset Tags");
        creatorAssetTags_.SetDescription("TAGS  ");
        creatorAssetTags_.SetValue("");
        creatorAssetTags_.SetPlaceholder("hero, gameplay, environment...");
        creatorAssetTags_.SetTooltip(
            "Comma-separated creator-owned tags for the selected registered asset.");
        creatorAssetTags_.SetCancelInputEnabled(false);

        creatorAssetStateCombo_.Create("Creator Asset State Filter");
        creatorAssetStateCombo_.AddItem("ALL", 0);
        creatorAssetStateCombo_.AddItem("CURRENT", 1);
        creatorAssetStateCombo_.AddItem("STALE", 2);
        creatorAssetStateCombo_.AddItem("MISSING", 3);
        creatorAssetStateCombo_.AddItem("MOVED", 4);
        creatorAssetStateCombo_.AddItem("UNREG", 5);
        creatorAssetStateCombo_.SetSelectedWithoutCallback(0);
        creatorAssetStateCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            creatorAssetStateFilter_ = static_cast<int>(args.userdata);
            creatorAssetRefreshPending_ = true;
        });

        creatorAssetFormatCombo_.Create("Creator Asset Format Filter");
        creatorAssetFormatCombo_.AddItem("FORMAT", 0);
        creatorAssetFormatCombo_.AddItem("FBX", 1);
        creatorAssetFormatCombo_.AddItem("GLTF", 2);
        creatorAssetFormatCombo_.AddItem("GLB", 3);
        creatorAssetFormatCombo_.AddItem("PNG", 4);
        creatorAssetFormatCombo_.AddItem("JPG", 5);
        creatorAssetFormatCombo_.AddItem("JPEG", 6);
        creatorAssetFormatCombo_.AddItem("DDS", 7);
        creatorAssetFormatCombo_.AddItem("TGA", 8);
        creatorAssetFormatCombo_.AddItem("BMP", 9);
        creatorAssetFormatCombo_.AddItem("HDR", 10);
        creatorAssetFormatCombo_.SetSelectedWithoutCallback(0);
        creatorAssetFormatCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            creatorAssetFormatFilter_ = static_cast<int>(args.userdata);
            creatorAssetRefreshPending_ = true;
        });

        creatorAssetRigCombo_.Create("Creator Asset Rig Filter");
        creatorAssetRigCombo_.AddItem("RIG", 0);
        creatorAssetRigCombo_.AddItem("SKINNED", 1);
        creatorAssetRigCombo_.AddItem("ANIMATED", 2);
        creatorAssetRigCombo_.AddItem("STATIC", 3);
        creatorAssetRigCombo_.SetSelectedWithoutCallback(0);
        creatorAssetRigCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            creatorAssetRigFilter_ = static_cast<int>(args.userdata);
            creatorAssetRefreshPending_ = true;
        });

        creatorAssetImportButton_.Create("Creator Import Asset");
        creatorAssetImportButton_.SetText("IMPORT MODEL...");
        creatorAssetImportButton_.SetTooltip(
            "Open the guided preview, material, lighting, animation, thumbnail and final import workflow.");
        creatorAssetImportButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ImportCreatorModel();
        });

        creatorAssetPlaceButton_.Create("Creator Asset Action");
        creatorAssetPlaceButton_.SetText("PLACE");
        creatorAssetPlaceButton_.SetTooltip(
            "Place the selected registered model .rasset without reconverting its source.");
        creatorAssetPlaceButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            PlaceSelectedCreatorAsset();
        });

        creatorAssetReimportButton_.Create("Creator Reimport Asset");
        creatorAssetReimportButton_.SetText("REIMPORT");
        creatorAssetReimportButton_.SetTooltip(
            "Refresh LC01 state and replay the selected model or governed resource's accepted import recipe.");
        creatorAssetReimportButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReimportSelectedCreatorAsset();
        });

        creatorAssetSaveTagsButton_.Create("Creator Save Asset Tags");
        creatorAssetSaveTagsButton_.SetText("SAVE TAGS");
        creatorAssetSaveTagsButton_.SetTooltip(
            "Persist creator-owned tags without changing system-derived metadata.");
        creatorAssetSaveTagsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SaveSelectedCreatorTags();
        });

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorAssetSearch_),
            static_cast<wi::gui::Widget*>(&creatorAssetTags_),
            static_cast<wi::gui::Widget*>(&creatorAssetStateCombo_),
            static_cast<wi::gui::Widget*>(&creatorAssetFormatCombo_),
            static_cast<wi::gui::Widget*>(&creatorAssetRigCombo_),
            static_cast<wi::gui::Widget*>(&creatorAssetImportButton_),
            static_cast<wi::gui::Widget*>(&creatorAssetPlaceButton_),
            static_cast<wi::gui::Widget*>(&creatorAssetReimportButton_),
            static_cast<wi::gui::Widget*>(&creatorAssetSaveTagsButton_)})
        {
            widget->SetVisible(false);
            widget->SetShadowRadius(0.0f);
        }
        LayoutCreatorAssetControls();
    }

    void CreatorAssetStudioChrome::LayoutCreatorAssetControls()
    {
        // Preserve the base browser's 22px folder-tree toggle at the start of
        // the toolbar; creator filters begin after it and never share its hit
        // target.
        const float left = HierarchyWidth() + 45.0f;
        const float right = creatorLayoutWidth_ - InspectorWidth() - 13.0f;
        const float available = std::max(0.0f, right - left);
        const float drawerTop = creatorLayoutHeight_ - BottomTabsHeight -
            StatusBarHeight - DrawerHeight();

        constexpr float gap = 4.0f;
        constexpr float stateWidth = 70.0f;
        constexpr float formatWidth = 58.0f;
        constexpr float rigWidth = 70.0f;
        constexpr float importWidth = 110.0f;
        constexpr float placeWidth = 80.0f;
        constexpr float reimportWidth = 76.0f;
        constexpr float saveWidth = 70.0f;

        // Keep the creator actions in the drawer header, away from the legacy
        // breadcrumb / LOCAL CONTENT toolbar. The previous single-row layout
        // put these controls underneath that toolbar at common 16:9 sizes,
        // leaving the backend workflow effectively unreachable to creators.
        const float actionY = drawerTop + 8.0f;
        float actionX = right -
            (importWidth + placeWidth + reimportWidth + saveWidth + gap * 3.0f);
        const auto placeAction = [&actionX, actionY, gap](
            wi::gui::Widget& widget, const float width)
        {
            widget.SetPos(XMFLOAT2(actionX, actionY));
            widget.SetSize(XMFLOAT2(width, 25.0f));
            actionX += width + gap;
        };
        placeAction(creatorAssetImportButton_, importWidth);
        placeAction(creatorAssetPlaceButton_, placeWidth);
        placeAction(creatorAssetReimportButton_, reimportWidth);
        placeAction(creatorAssetSaveTagsButton_, saveWidth);

        const float filterY = drawerTop + 45.0f;
        const float fixed = stateWidth + formatWidth + rigWidth + gap * 4.0f;
        const float flexible = std::max(80.0f, available - fixed);
        const float searchWidth = flexible * 0.5f;
        const float tagsWidth = flexible - searchWidth;

        float x = left;
        const auto placeFilter = [&x, filterY, gap](
            wi::gui::Widget& widget, const float width)
        {
            widget.SetPos(XMFLOAT2(x, filterY));
            widget.SetSize(XMFLOAT2(width, 25.0f));
            x += width + gap;
        };
        placeFilter(creatorAssetStateCombo_, stateWidth);
        placeFilter(creatorAssetFormatCombo_, formatWidth);
        placeFilter(creatorAssetRigCombo_, rigWidth);
        placeFilter(creatorAssetSearch_, searchWidth);
        placeFilter(creatorAssetTags_, tagsWidth);
    }

    bridge::AssetCatalogueQuery CreatorAssetStudioChrome::CreatorAssetQuery() const
    {
        bridge::AssetCatalogueQuery query;
        query.text = InputValue(creatorAssetSearch_);
        switch (creatorAssetStateFilter_)
        {
        case 1: query.state = bridge::AssetCatalogueState::Current; break;
        case 2: query.state = bridge::AssetCatalogueState::Stale; break;
        case 3: query.state = bridge::AssetCatalogueState::Missing; break;
        case 4: query.state = bridge::AssetCatalogueState::Moved; break;
        case 5: query.state = bridge::AssetCatalogueState::Unregistered; break;
        default: break;
        }
        switch (creatorAssetFormatFilter_)
        {
        case 1: query.sourceFormat = "fbx"; break;
        case 2: query.sourceFormat = "gltf"; break;
        case 3: query.sourceFormat = "glb"; break;
        case 4: query.sourceFormat = "png"; break;
        case 5: query.sourceFormat = "jpg"; break;
        case 6: query.sourceFormat = "jpeg"; break;
        case 7: query.sourceFormat = "dds"; break;
        case 8: query.sourceFormat = "tga"; break;
        case 9: query.sourceFormat = "bmp"; break;
        case 10: query.sourceFormat = "hdr"; break;
        default: break;
        }
        switch (creatorAssetRigFilter_)
        {
        case 1: query.skinned = true; break;
        case 2: query.animated = true; break;
        case 3: query.staticModelsOnly = true; break;
        default: break;
        }
        return query;
    }

    std::vector<std::string> CreatorAssetStudioChrome::CreatorTagInput() const
    {
        std::vector<std::string> tags;
        std::stringstream stream(InputValue(creatorAssetTags_));
        std::string tag;
        while (std::getline(stream, tag, ','))
        {
            tag = Trim(std::move(tag));
            if (!tag.empty())
                tags.push_back(std::move(tag));
        }
        return tags;
    }

    void CreatorAssetStudioChrome::UpdateCreatorAssetControls(
        const wi::Canvas& canvas,
        const float dt)
    {
        creatorAssetControlConsumed_ = false;
        const bool visible = ActiveBottomTab() == 0;
        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorAssetSearch_),
            static_cast<wi::gui::Widget*>(&creatorAssetTags_),
            static_cast<wi::gui::Widget*>(&creatorAssetStateCombo_),
            static_cast<wi::gui::Widget*>(&creatorAssetFormatCombo_),
            static_cast<wi::gui::Widget*>(&creatorAssetRigCombo_),
            static_cast<wi::gui::Widget*>(&creatorAssetImportButton_),
            static_cast<wi::gui::Widget*>(&creatorAssetPlaceButton_),
            static_cast<wi::gui::Widget*>(&creatorAssetReimportButton_),
            static_cast<wi::gui::Widget*>(&creatorAssetSaveTagsButton_)})
        {
            widget->SetVisible(visible);
        }
        if (!visible)
            return;

        LayoutCreatorAssetControls();
        creatorAssetSearch_.Update(canvas, dt);
        creatorAssetTags_.Update(canvas, dt);
        creatorAssetStateCombo_.Update(canvas, dt);
        creatorAssetFormatCombo_.Update(canvas, dt);
        creatorAssetRigCombo_.Update(canvas, dt);

        const auto selected = std::find_if(
            creatorAssetCatalogue_.entries.begin(), creatorAssetCatalogue_.entries.end(),
            [this](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.projectRelativePath == creatorSelectedAssetPath_;
            });
        const bool registered = selected != creatorAssetCatalogue_.entries.end() &&
            selected->registered && bridge::IsValidStableId(selected->assetId);
        const bool modelProduct = registered &&
            bridge::CanPlaceCreatorModelAsset(*selected);
        const bool modelReimportable = registered &&
            bridge::CanReimportCreatorModelAsset(*selected);
        const bool resourceReimportable = registered &&
            bridge::CanReimportCreatorResourceAsset(*selected);
        const bool textureProduct = registered && selected->importedProduct &&
            selected->productAvailable &&
            selected->dependencyClass == bridge::DependencyClass::Texture;

        bool textureAssignable = false;
        if (textureProduct && selected->state == bridge::AssetCatalogueState::Current)
        {
            auto* session = bridge::StudioSession::Current();
            if (session != nullptr && session->Selection().HasSelection())
            {
                textureAssignable = bridge::ResolveEditableMaterialEntity(
                    session->Scenes().GetScene(),
                    session->Selection().SelectedEntity()) != wi::ecs::INVALID_ENTITY;
            }
        }
        if (textureProduct)
        {
            creatorAssetPlaceButton_.SetText("ASSIGN BASE");
            creatorAssetPlaceButton_.SetTooltip(
                "Assign the selected governed texture to the base-colour slot of the selected object's one editable material.");
        }
        else
        {
            creatorAssetPlaceButton_.SetText("PLACE");
            creatorAssetPlaceButton_.SetTooltip(
                "Place the selected registered model .rasset without reconverting its source.");
        }
        creatorAssetPlaceButton_.SetEnabled(modelProduct || textureAssignable);
        creatorAssetReimportButton_.SetEnabled(
            modelReimportable || resourceReimportable);
        creatorAssetSaveTagsButton_.SetEnabled(registered);

        creatorAssetImportButton_.Update(canvas, dt);
        creatorAssetPlaceButton_.Update(canvas, dt);
        creatorAssetReimportButton_.Update(canvas, dt);
        creatorAssetSaveTagsButton_.Update(canvas, dt);

        const std::string search = InputValue(creatorAssetSearch_);
        if (search != creatorAssetLastSearch_)
        {
            creatorAssetLastSearch_ = search;
            creatorAssetRefreshPending_ = true;
        }

        const auto engaged = [](const wi::gui::Widget& widget)
        {
            return widget.GetState() != wi::gui::IDLE;
        };
        creatorAssetControlConsumed_ =
            engaged(creatorAssetSearch_) || engaged(creatorAssetTags_) ||
            engaged(creatorAssetStateCombo_) || engaged(creatorAssetFormatCombo_) ||
            engaged(creatorAssetRigCombo_) || engaged(creatorAssetImportButton_) ||
            engaged(creatorAssetPlaceButton_) || engaged(creatorAssetReimportButton_) ||
            engaged(creatorAssetSaveTagsButton_);

        if (creatorAssetRefreshPending_ && !wi::jobsystem::IsBusy(creatorAssetWorkload_))
            RefreshCreatorAssetBrowser();
    }

    void CreatorAssetStudioChrome::RenderCreatorAssetControls(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (ActiveBottomTab() != 0)
            return;
        // The creator filters own the browser toolbar row. Cover the base
        // chrome's placeholder breadcrumb/search treatment first so two
        // independent toolbars can never render on top of each other.
        const float left = HierarchyWidth() + 38.0f;
        const float right = creatorLayoutWidth_ - InspectorWidth();
        const float drawerTop = creatorLayoutHeight_ - BottomTabsHeight -
            StatusBarHeight - DrawerHeight();
        DrawSolidRect(
            left,
            drawerTop + 42.0f,
            std::max(0.0f, right - left),
            34.0f,
            wi::Color(5, 10, 13, 255),
            cmd);
        creatorAssetSearch_.Render(canvas, cmd);
        creatorAssetTags_.Render(canvas, cmd);
        creatorAssetStateCombo_.Render(canvas, cmd);
        creatorAssetFormatCombo_.Render(canvas, cmd);
        creatorAssetRigCombo_.Render(canvas, cmd);
        creatorAssetImportButton_.Render(canvas, cmd);
        creatorAssetPlaceButton_.Render(canvas, cmd);
        creatorAssetReimportButton_.Render(canvas, cmd);
        creatorAssetSaveTagsButton_.Render(canvas, cmd);
    }

    void CreatorAssetStudioChrome::RefreshCreatorAssetBrowser()
    {
        creatorAssetRefreshPending_ = false;
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
        {
            creatorAssetCatalogue_ = {};
            creatorSelectedAssetId_.clear();
            SetStatusText("ASSET BROWSER // OPEN A PROJECT");
            return;
        }

        const auto& project = session->Projects().CurrentProject();
        std::string catalogueWarning;
        if (creatorCatalogueProjectId_ != project.projectId ||
            creatorAssetCatalogueDirty_)
        {
            bridge::AssetCatalogue catalogue;
            std::string error;
            if (!creatorAssetWorkflow_.BuildCatalogueSnapshot(
                    project.rootPath, project.projectId, catalogue, error))
            {
                creatorAssetCatalogueDirty_ = true;
                SetStatusText("ASSET BROWSER // REFRESH FAILED // " + error);
                return;
            }
            bridge::CreatorTextureWorkflowService textureWorkflow;
            std::string textureWarning;
            (void)textureWorkflow.EnrichTextureCatalogue(
                project.rootPath, project.projectId, catalogue, textureWarning);
            creatorAssetCatalogue_ = std::move(catalogue);
            creatorCatalogueProjectId_ = project.projectId;
            creatorAssetCatalogueDirty_ = false;
            catalogueWarning = std::move(textureWarning);
        }

        const auto query = CreatorAssetQuery();
        const auto matches = bridge::QueryAssetCatalogue(creatorAssetCatalogue_, query);
        const bool globalSearch = !query.text.empty();
        std::vector<AssetCard> cards;
        creatorVisibleAssetPaths_.clear();
        creatorVisibleThumbnailPaths_.clear();
        if (!globalSearch)
        {
            for (const auto& existing : creatorFilesystemAssets_)
            {
                if (existing.directory)
                {
                    cards.push_back(existing);
                    creatorVisibleAssetPaths_.push_back(existing.relativePath);
                }
            }
        }
        for (const auto& entry : matches)
        {
            if (!globalSearch && ParentPath(entry.projectRelativePath) !=
                    fs::u8path(creatorCurrentPath_).lexically_normal().generic_u8string())
                continue;
            AssetCard card;
            card.name = entry.name;
            card.relativePath = entry.projectRelativePath;
            card.typeLabel = UpperAscii(bridge::AssetCatalogueStateLabel(entry.state));
            fs::path thumbnailPath =
                fs::u8path(project.rootPath) /
                fs::u8path(entry.projectRelativePath);
            thumbnailPath.replace_extension(".thumbnail.png");
            if (fs::exists(thumbnailPath))
            {
                card.thumbnail = wi::resourcemanager::Load(
                    thumbnailPath.generic_u8string());
                if (card.thumbnail.IsValid())
                    creatorVisibleThumbnailPaths_.insert(
                        fs::u8path(card.relativePath)
                            .lexically_normal().generic_u8string());
            }
            if (!entry.sourceFormat.empty())
                card.typeLabel += " / " + UpperAscii(entry.sourceFormat);
            if (entry.dependencyClass == bridge::DependencyClass::Texture)
                card.typeLabel += " / TEXTURE";
            if (entry.model.known && entry.model.skinned)
                card.typeLabel += " / SKIN";
            if (entry.model.known && entry.model.animated)
                card.typeLabel += " / ANIM";
            creatorVisibleAssetPaths_.push_back(card.relativePath);
            cards.push_back(std::move(card));
        }
        RenegadeStudioChrome::SetAssetBrowserData(
            creatorFilesystemFolders_, std::move(cards), creatorCurrentPath_);

        const auto selected = std::find_if(
            creatorAssetCatalogue_.entries.begin(), creatorAssetCatalogue_.entries.end(),
            [this](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.projectRelativePath == creatorSelectedAssetPath_;
            });
        const bool selectedVisible = selected != creatorAssetCatalogue_.entries.end() &&
            std::find(
                creatorVisibleAssetPaths_.begin(),
                creatorVisibleAssetPaths_.end(),
                selected->projectRelativePath) != creatorVisibleAssetPaths_.end();
        if (!selectedVisible)
        {
            creatorSelectedAssetId_.clear();
            creatorSelectedAssetPath_.clear();
            creatorAssetTags_.SetValue("");
        }
        else
        {
            creatorSelectedAssetId_ = selected->assetId;
            SetAssetBrowserSelectedPath(selected->projectRelativePath);
        }

        if (!catalogueWarning.empty())
        {
            SetStatusText("ASSET BROWSER // TEXTURE METADATA WARNING // " +
                catalogueWarning);
        }
        else
        {
            SetStatusText("ASSET BROWSER // " + std::to_string(matches.size()) +
                " CATALOGUE MATCHES // " +
                (globalSearch ? std::string("ALL CONTENT") : creatorCurrentPath_));
        }
    }

    bool CreatorAssetStudioChrome::SelectCreatorAsset(const std::string& relativePath)
    {
        const auto found = std::find_if(
            creatorAssetCatalogue_.entries.begin(), creatorAssetCatalogue_.entries.end(),
            [&relativePath](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.projectRelativePath == relativePath;
            });
        if (found == creatorAssetCatalogue_.entries.end())
            return false;

        creatorSelectedAssetPath_ = found->projectRelativePath;
        creatorSelectedAssetId_ = found->assetId;
        creatorAssetTags_.SetValue(JoinTags(found->creatorTags));

        std::ostringstream status;
        status << "ASSET // " << found->name << " // "
               << UpperAscii(bridge::AssetCatalogueStateLabel(found->state));
        if (!found->sourceFormat.empty())
            status << " // " << UpperAscii(found->sourceFormat);
        if (found->dependencyClass == bridge::DependencyClass::Texture)
            status << " // GOVERNED TEXTURE";
        if (found->model.known)
        {
            status << " // MESH " << found->model.meshCount
                   << " MAT " << found->model.materialCount
                   << " BONE " << found->model.boneCount
                   << " ANIM " << found->model.animationClipCount;
            if (found->model.skinned) status << " // SKINNED";
            if (found->model.animated) status << " // ANIMATED";
        }
        if (!found->creatorTags.empty())
            status << " // TAGS " << JoinTags(found->creatorTags);
        SetStatusText(status.str());
        return true;
    }

    void CreatorAssetStudioChrome::ImportCreatorModel()
    {
        // Model imports must never bypass the dedicated preview-first
        // workspace or its required thumbnail/verification stage. Route the
        // Asset Browser shortcut through the same Studio action as
        // ADD > IMPORT MODEL... .
        if (creatorAction_)
        {
            creatorAction_(Action::ImportModel);
            return;
        }

        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
        {
            wi::helper::messageBox(
                "Open or create a Renegade project before importing an asset.",
                "Import Project Asset");
            return;
        }
        if (wi::jobsystem::IsBusy(creatorAssetWorkload_))
            return;

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description =
            "Reusable model or governed texture to import into this project";
        params.extensions = {
            "fbx", "gltf", "glb",
            "jpg", "jpeg", "png", "bmp", "dds", "tga", "hdr"};
        wi::helper::FileDialog(params, [this](const std::string& sourcePath)
        {
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, sourcePath](std::uint64_t)
                {
                    auto* current = bridge::StudioSession::Current();
                    if (sourcePath.empty() || current == nullptr ||
                        !current->Projects().HasProject() ||
                        wi::jobsystem::IsBusy(creatorAssetWorkload_))
                        return;

                    const bridge::ResourceSourceFormat resourceFormat =
                        bridge::DetectResourceSourceFormat(sourcePath);
                    const bool isTexture =
                        resourceFormat != bridge::ResourceSourceFormat::Unknown &&
                        bridge::ClassifyResourceSourceFormat(resourceFormat) ==
                            bridge::ResourceClass::Texture;
                    if (isTexture)
                    {
                        struct TextureImportWorkState
                        {
                            std::string projectRoot;
                            bridge::StableId projectId;
                            std::string sourcePath;
                            bridge::CreatorTextureImportResult imported;
                        };

                        auto state = std::make_shared<TextureImportWorkState>();
                        state->projectRoot = current->Projects().CurrentProject().rootPath;
                        state->projectId = current->Projects().CurrentProject().projectId;
                        state->sourcePath = sourcePath;
                        SetStatusText("IMPORT TEXTURE // RETAIN + REGISTER // " +
                            fs::u8path(sourcePath).filename().generic_u8string());

                        wi::jobsystem::Execute(creatorAssetWorkload_,
                            [this, state](wi::jobsystem::JobArgs)
                            {
                                bridge::CreatorTextureWorkflowService workflow;
                                state->imported = workflow.ImportTexture(
                                    state->projectRoot, state->projectId,
                                    state->sourcePath);
                                wi::eventhandler::Subscribe_Once(
                                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                    [this, state](std::uint64_t)
                                    {
                                        creatorAssetCatalogueDirty_ = true;
                                        creatorAssetRefreshPending_ = true;
                                        if (!state->imported.succeeded)
                                        {
                                            if (state->imported.committed)
                                            {
                                                SetStatusText(
                                                    "IMPORT TEXTURE // COMMITTED // VERIFY FAILED");
                                                wi::helper::messageBox(
                                                    "The governed texture transaction committed, but post-commit verification failed. Refresh the project before retrying.\n\nReason: " +
                                                        state->imported.error,
                                                    "Import Project Texture");
                                            }
                                            else
                                            {
                                                SetStatusText("IMPORT TEXTURE // FAILED");
                                                wi::helper::messageBox(
                                                    "Could not create the governed project texture.\n\nReason: " +
                                                        state->imported.error,
                                                    "Import Project Texture");
                                            }
                                            return;
                                        }
                                        creatorSelectedAssetId_ =
                                            state->imported.assetId;
                                        creatorSelectedAssetPath_ =
                                            state->imported.assetProjectRelativePath;
                                        SetStatusText(
                                            "IMPORT TEXTURE // CURRENT // SELECT OBJECT + ASSIGN BASE");
                                    });
                            });
                        return;
                    }

                    struct ImportWorkState
                    {
                        std::string projectRoot;
                        bridge::StableId projectId;
                        std::string sourcePath;
                        bridge::CreatorModelImportResult imported;
                    };

                    auto state = std::make_shared<ImportWorkState>();
                    state->projectRoot = current->Projects().CurrentProject().rootPath;
                    state->projectId = current->Projects().CurrentProject().projectId;
                    state->sourcePath = sourcePath;
                    SetStatusText("IMPORT ASSET // RETAIN + CONVERT // " +
                        fs::u8path(sourcePath).filename().generic_u8string());

                    wi::jobsystem::Execute(creatorAssetWorkload_,
                        [this, state](wi::jobsystem::JobArgs)
                        {
                            bridge::CreatorAssetWorkflowService workflow;
                            state->imported = workflow.ImportModel(
                                state->projectRoot, state->projectId, state->sourcePath);
                            wi::eventhandler::Subscribe_Once(
                                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                [this, state](std::uint64_t)
                                {
                                    creatorAssetCatalogueDirty_ = true;
                                    creatorAssetRefreshPending_ = true;
                                    if (!state->imported.succeeded)
                                    {
                                        SetStatusText("IMPORT ASSET // FAILED");
                                        wi::helper::messageBox(
                                            "Could not create the reusable project asset.\n\nReason: " +
                                                state->imported.error,
                                            "Import Project Asset");
                                        return;
                                    }
                                    creatorSelectedAssetId_ = state->imported.asset.assetId;
                                    creatorSelectedAssetPath_ = state->imported.assetProjectRelativePath;
                                    SetStatusText(
                                        "IMPORT ASSET // CURRENT // READY IN ASSET BROWSER");
                                });
                        });
                });
        });
    }

    void CreatorAssetStudioChrome::PlaceSelectedCreatorAsset()
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(creatorSelectedAssetId_))
            return;

        const auto selected = std::find_if(
            creatorAssetCatalogue_.entries.begin(), creatorAssetCatalogue_.entries.end(),
            [this](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.assetId == creatorSelectedAssetId_;
            });
        if (selected != creatorAssetCatalogue_.entries.end() &&
            selected->dependencyClass == bridge::DependencyClass::Texture)
        {
            if (selected->state != bridge::AssetCatalogueState::Current ||
                !selected->productAvailable)
            {
                SetStatusText("ASSIGN BASE // TEXTURE IS NOT CURRENT");
                return;
            }
            if (!session->Selection().HasSelection())
            {
                SetStatusText("ASSIGN BASE // SELECT AN OBJECT WITH ONE EDITABLE MATERIAL");
                return;
            }
            const wi::ecs::Entity materialEntity =
                bridge::ResolveEditableMaterialEntity(
                    session->Scenes().GetScene(),
                    session->Selection().SelectedEntity());
            if (materialEntity == wi::ecs::INVALID_ENTITY)
            {
                SetStatusText("ASSIGN BASE // MATERIAL TARGET IS MISSING OR AMBIGUOUS");
                return;
            }

            const auto* material = session->Scenes().GetScene().materials.GetComponent(
                materialEntity);
            const auto* metadata = session->Scenes().GetScene().metadatas.GetComponent(
                materialEntity);
            if (material != nullptr && metadata != nullptr &&
                metadata->string_values.has(
                    bridge::MaterialBaseColorTextureAssetIdMetadataKey) &&
                metadata->string_values.get(
                    bridge::MaterialBaseColorTextureAssetIdMetadataKey) ==
                    creatorSelectedAssetId_ &&
                material->textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() &&
                material->textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].name.empty())
            {
                SetStatusText("ASSIGN BASE // ALREADY ASSIGNED // NO CHANGE");
                return;
            }

            const auto& project = session->Projects().CurrentProject();
            bridge::PreparedMaterialTextureAsset prepared;
            std::string error;
            if (!bridge::PrepareMaterialTextureAsset(
                    project.rootPath, project.projectId,
                    creatorSelectedAssetId_, prepared, error))
            {
                SetStatusText("ASSIGN BASE // PREPARE FAILED // " + error);
                return;
            }
            auto command =
                std::make_unique<bridge::SetMaterialBaseColorTextureAssetCommand>(
                    session->Scenes().GetScene(), materialEntity,
                    std::move(prepared));
            if (!session->Commands().Execute(std::move(command)))
            {
                SetStatusText("ASSIGN BASE // FAILED");
                return;
            }
            SetSceneDirty(session->Commands().IsDirty());
            SetStatusText("ASSIGN BASE // GOVERNED TEXTURE // STABLE ID " +
                creatorSelectedAssetId_);
            return;
        }

        if (creatorAssetPlaceRequested_)
        {
            creatorAssetPlaceRequested_(
                creatorSelectedAssetId_,
                fs::u8path(creatorSelectedAssetPath_)
                    .filename().generic_u8string());
            SetStatusText("PLACE ASSET // CLICK A SURFACE // ESC TO CANCEL");
        }
    }

    void CreatorAssetStudioChrome::ReimportSelectedCreatorAsset()
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(creatorSelectedAssetId_) ||
            wi::jobsystem::IsBusy(creatorAssetWorkload_))
            return;

        const auto selected = std::find_if(
            creatorAssetCatalogue_.entries.begin(), creatorAssetCatalogue_.entries.end(),
            [this](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.assetId == creatorSelectedAssetId_;
            });
        if (selected == creatorAssetCatalogue_.entries.end())
            return;

        if (bridge::IsCreatorGovernedResourceClass(selected->dependencyClass))
        {
            if (!bridge::CanReimportCreatorResourceAsset(*selected))
            {
                SetStatusText(
                    "REIMPORT RESOURCE // RECOVER MISSING SOURCE/PRODUCT OR REPAIR INVALID STATE");
                return;
            }

            struct ResourceReimportWorkState
            {
                std::string projectRoot;
                bridge::StableId projectId;
                bridge::StableId assetId;
                bridge::DependencyClass dependencyClass = bridge::DependencyClass::Data;
                bridge::ResourceAssetReimportResult result;
            };

            auto state = std::make_shared<ResourceReimportWorkState>();
            state->projectRoot = session->Projects().CurrentProject().rootPath;
            state->projectId = session->Projects().CurrentProject().projectId;
            state->assetId = creatorSelectedAssetId_;
            state->dependencyClass = selected->dependencyClass;
            SetStatusText("REIMPORT RESOURCE // REFRESH + REPLAY STORED RECIPE");

            wi::jobsystem::Execute(creatorAssetWorkload_,
                [this, state](wi::jobsystem::JobArgs)
                {
                    bridge::CreatorAssetWorkflowService refreshWorkflow;
                    bridge::AssetCatalogue refreshedCatalogue;
                    std::string refreshError;
                    if (!refreshWorkflow.BuildCatalogue(
                            state->projectRoot, state->projectId,
                            refreshedCatalogue, refreshError))
                    {
                        state->result.error =
                            "LC01 refresh before resource reimport failed: " +
                            refreshError;
                    }
                    else
                    {
                        bridge::ResourceAssetReimportRequest request;
                        request.projectRoot = state->projectRoot;
                        request.projectId = state->projectId;
                        request.assetId = state->assetId;
                        state->result = bridge::ResourceAssetService()
                            .ReimportResourceAsset(request);
                    }

                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, state](std::uint64_t)
                        {
                            creatorAssetCatalogueDirty_ = true;
                            creatorAssetRefreshPending_ = true;
                            if (!state->result.succeeded)
                            {
                                if (state->result.transaction.committed)
                                {
                                    SetStatusText(
                                        "REIMPORT RESOURCE // COMMITTED // VERIFY FAILED // " +
                                        state->result.error);
                                }
                                else
                                {
                                    SetStatusText(
                                        "REIMPORT RESOURCE // FAILED // " +
                                        state->result.error);
                                }
                                return;
                            }

                            if (state->dependencyClass ==
                                bridge::DependencyClass::Texture)
                            {
                                auto* current = bridge::StudioSession::Current();
                                if (current == nullptr ||
                                    !current->Projects().HasProject() ||
                                    current->Projects().CurrentProject().projectId !=
                                        state->projectId)
                                {
                                    SetStatusText(
                                        "REIMPORT TEXTURE // CURRENT // LIVE SCENE NO LONGER MATCHES PROJECT");
                                    return;
                                }
                                const auto refreshed =
                                    bridge::RefreshMaterialTextureBindingsForAsset(
                                        current->Scenes().GetScene(),
                                        state->projectRoot, state->projectId,
                                        state->assetId);
                                if (!refreshed.succeeded)
                                {
                                    SetStatusText(
                                        "REIMPORT TEXTURE // CURRENT // LIVE REFRESH WARNING // " +
                                        refreshed.error);
                                    return;
                                }
                                SetStatusText(
                                    "REIMPORT TEXTURE // CURRENT // SAME STABLE ID // LIVE BINDINGS " +
                                    std::to_string(refreshed.restored));
                                return;
                            }

                            SetStatusText(
                                "REIMPORT RESOURCE // CURRENT // SAME STABLE ID");
                        });
                });
            return;
        }

        struct ReimportWorkState
        {
            std::string projectRoot;
            bridge::StableId projectId;
            bridge::StableId assetId;
            bridge::ReusableModelReimportResult result;
        };

        auto state = std::make_shared<ReimportWorkState>();
        state->projectRoot = session->Projects().CurrentProject().rootPath;
        state->projectId = session->Projects().CurrentProject().projectId;
        state->assetId = creatorSelectedAssetId_;
        SetStatusText("REIMPORT ASSET // REFRESH + REPLAY RECIPE");

        wi::jobsystem::Execute(creatorAssetWorkload_,
            [this, state](wi::jobsystem::JobArgs)
            {
                bridge::CreatorAssetWorkflowService workflow;
                state->result = workflow.ReimportModel(
                    state->projectRoot, state->projectId, state->assetId);
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state](std::uint64_t)
                    {
                        creatorAssetCatalogueDirty_ = true;
                        creatorAssetRefreshPending_ = true;
                        if (!state->result.succeeded)
                        {
                            SetStatusText("REIMPORT ASSET // FAILED // " + state->result.error);
                            return;
                        }
                        SetStatusText("REIMPORT ASSET // CURRENT // SAME STABLE ID");
                    });
            });
    }

    void CreatorAssetStudioChrome::SaveSelectedCreatorTags()
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(creatorSelectedAssetId_))
            return;
        const auto& project = session->Projects().CurrentProject();
        std::string error;
        if (!creatorAssetWorkflow_.SetCreatorTags(
                project.rootPath, project.projectId, creatorSelectedAssetId_,
                CreatorTagInput(), error))
        {
            SetStatusText("SAVE TAGS // FAILED // " + error);
            return;
        }
        creatorAssetCatalogueDirty_ = true;
        creatorAssetRefreshPending_ = true;
        SetStatusText("SAVE TAGS // COMMITTED");
    }

    void CreatorAssetStudioChrome::RefreshCreatorHierarchyRows()
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr)
            return;
        const wi::ecs::Entity selected = session->Selection().SelectedEntity();
        std::vector<HierarchyRow> rows;
        for (const auto& entity : session->Scenes().ListEntities())
        {
            HierarchyRow row;
            row.name = entity.name;
            row.depth = entity.depth;
            row.selected = entity.entity == selected;
            row.entity = static_cast<std::uint64_t>(entity.entity);
            row.category = MapCategory(entity.category);
            rows.push_back(std::move(row));
        }
        SetHierarchyRows(std::move(rows));
    }
}
