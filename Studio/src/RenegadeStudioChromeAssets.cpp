#include "RenegadeStudioChrome.h"

#include "renegade/bridge/ImportService.h"
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

    std::string UpperAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::toupper(character));
            });
        return value;
    }

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char value)
        {
            return std::isspace(value) != 0;
        };
        while (!value.empty() && whitespace(value.front()))
            value.erase(value.begin());
        while (!value.empty() && whitespace(value.back()))
            value.pop_back();
        return value;
    }

    std::string ParentPath(const std::string& projectRelativePath)
    {
        return fs::u8path(projectRelativePath)
            .parent_path().lexically_normal().generic_u8string();
    }

    std::string JoinTags(const std::vector<std::string>& tags)
    {
        std::ostringstream stream;
        for (std::size_t index = 0; index < tags.size(); ++index)
        {
            if (index != 0)
                stream << ", ";
            stream << tags[index];
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
        case Source::Other:
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
    }

    void CreatorAssetStudioChrome::SetLayout(
        const float width,
        const float height)
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
        creatorFilesystemFolders_ = folders;
        creatorFilesystemAssets_ = assets;
        creatorCurrentPath_ = currentPath;
        RenegadeStudioChrome::SetAssetBrowserData(
            std::move(folders), std::move(assets), std::move(currentPath));
        creatorAssetRefreshPending_ = true;
    }

    void CreatorAssetStudioChrome::SetActiveBottomTab(
        const int tab,
        const bool notify)
    {
        RenegadeStudioChrome::SetActiveBottomTab(tab, notify);
        if (tab == 0)
            creatorAssetRefreshPending_ = true;
    }

    void CreatorAssetStudioChrome::OnAction(
        std::function<void(Action)> callback)
    {
        RenegadeStudioChrome::OnAction(
            [this, callback = std::move(callback)](const Action action) mutable
            {
                if (action == Action::ImportModel)
                {
                    ImportCreatorModel();
                    return;
                }
                if (callback)
                    callback(action);
            });
    }

    void CreatorAssetStudioChrome::OnAssetBrowserItemSelected(
        std::function<void(const std::string&)> callback)
    {
        RenegadeStudioChrome::OnAssetBrowserItemSelected(
            [this, callback = std::move(callback)](const std::string& path) mutable
            {
                if (SelectCreatorAsset(path))
                    return;
                if (callback)
                    callback(path);
            });
    }

    bool CreatorAssetStudioChrome::ConsumedPointerThisFrame() const noexcept
    {
        return creatorAssetControlConsumed_ ||
            RenegadeStudioChrome::ConsumedPointerThisFrame();
    }

    void CreatorAssetStudioChrome::Update(
        const wi::Canvas& canvas,
        const float dt)
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
        creatorAssetSearch_.SetDescription("⌕  ");
        creatorAssetSearch_.SetValue("");
        creatorAssetSearch_.SetPlaceholder("SEARCH ASSETS / TAGS / STATE...");
        creatorAssetSearch_.SetTooltip(
            "Search name, path, stable ID, source format, importer, state and creator tags.");
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
        creatorAssetFormatCombo_.SetSelectedWithoutCallback(0);
        creatorAssetFormatCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            creatorAssetFormatFilter_ = static_cast<int>(args.userdata);
            creatorAssetRefreshPending_ = true;
        });

        creatorAssetRigCombo_.Create("Creator Asset Rig Filter");
        creatorAssetRigCombo_.AddItem("MODEL", 0);
        creatorAssetRigCombo_.AddItem("SKINNED", 1);
        creatorAssetRigCombo_.AddItem("ANIMATED", 2);
        creatorAssetRigCombo_.AddItem("STATIC", 3);
        creatorAssetRigCombo_.SetSelectedWithoutCallback(0);
        creatorAssetRigCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            creatorAssetRigFilter_ = static_cast<int>(args.userdata);
            creatorAssetRefreshPending_ = true;
        });

        creatorAssetImportButton_.Create("Creator Import Model");
        creatorAssetImportButton_.SetText("IMPORT");
        creatorAssetImportButton_.SetTooltip(
            "Retain FBX/GLTF/GLB source, create a governed .rasset and place it.");
        creatorAssetImportButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ImportCreatorModel();
        });

        creatorAssetPlaceButton_.Create("Creator Place Asset");
        creatorAssetPlaceButton_.SetText("PLACE");
        creatorAssetPlaceButton_.SetTooltip(
            "Place the selected registered .rasset without reconverting its source.");
        creatorAssetPlaceButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            PlaceSelectedCreatorAsset();
        });

        creatorAssetReimportButton_.Create("Creator Reimport Asset");
        creatorAssetReimportButton_.SetText("REIMPORT");
        creatorAssetReimportButton_.SetTooltip(
            "Refresh LC01 state and replay the selected asset's accepted import recipe.");
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
        const float left = HierarchyWidth() + 13.0f;
        const float right = creatorLayoutWidth_ - InspectorWidth() - 13.0f;
        const float available = std::max(0.0f, right - left);
        const float bottomTabsTop =
            creatorLayoutHeight_ - BottomTabsHeight - StatusBarHeight;
        const float drawerTop = bottomTabsTop - DrawerHeight();
        const float y = drawerTop + 45.0f;

        constexpr float gap = 4.0f;
        constexpr float stateWidth = 70.0f;
        constexpr float formatWidth = 58.0f;
        constexpr float rigWidth = 70.0f;
        constexpr float importWidth = 52.0f;
        constexpr float placeWidth = 48.0f;
        constexpr float reimportWidth = 64.0f;
        constexpr float saveWidth = 68.0f;
        constexpr float fixed = stateWidth + formatWidth + rigWidth +
            importWidth + placeWidth + reimportWidth + saveWidth + gap * 8.0f;
        const float flexible = std::max(160.0f, available - fixed);
        const float searchWidth = flexible * 0.5f;
        const float tagsWidth = flexible - searchWidth;

        float x = left;
        creatorAssetStateCombo_.SetPos(XMFLOAT2(x, y));
        creatorAssetStateCombo_.SetSize(XMFLOAT2(stateWidth, 25.0f));
        x += stateWidth + gap;
        creatorAssetFormatCombo_.SetPos(XMFLOAT2(x, y));
        creatorAssetFormatCombo_.SetSize(XMFLOAT2(formatWidth, 25.0f));
        x += formatWidth + gap;
        creatorAssetRigCombo_.SetPos(XMFLOAT2(x, y));
        creatorAssetRigCombo_.SetSize(XMFLOAT2(rigWidth, 25.0f));
        x += rigWidth + gap;
        creatorAssetSearch_.SetPos(XMFLOAT2(x, y));
        creatorAssetSearch_.SetSize(XMFLOAT2(searchWidth, 25.0f));
        x += searchWidth + gap;
        creatorAssetTags_.SetPos(XMFLOAT2(x, y));
        creatorAssetTags_.SetSize(XMFLOAT2(tagsWidth, 25.0f));
        x += tagsWidth + gap;
        creatorAssetImportButton_.SetPos(XMFLOAT2(x, y));
        creatorAssetImportButton_.SetSize(XMFLOAT2(importWidth, 25.0f));
        x += importWidth + gap;
        creatorAssetPlaceButton_.SetPos(XMFLOAT2(x, y));
        creatorAssetPlaceButton_.SetSize(XMFLOAT2(placeWidth, 25.0f));
        x += placeWidth + gap;
        creatorAssetReimportButton_.SetPos(XMFLOAT2(x, y));
        creatorAssetReimportButton_.SetSize(XMFLOAT2(reimportWidth, 25.0f));
        x += reimportWidth + gap;
        creatorAssetSaveTagsButton_.SetPos(XMFLOAT2(x, y));
        creatorAssetSaveTagsButton_.SetSize(XMFLOAT2(saveWidth, 25.0f));
    }

    bridge::AssetCatalogueQuery CreatorAssetStudioChrome::CreatorAssetQuery() const
    {
        bridge::AssetCatalogueQuery query;
        query.text = creatorAssetSearch_.GetText();
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
        std::stringstream stream(creatorAssetTags_.GetText());
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
            creatorAssetCatalogue_.entries.begin(),
            creatorAssetCatalogue_.entries.end(),
            [this](const bridge::AssetCatalogueEntry& entry)
            {
                return !creatorSelectedAssetPath_.empty() &&
                    entry.projectRelativePath == creatorSelectedAssetPath_;
            });
        const bool selectedRegistered =
            selected != creatorAssetCatalogue_.entries.end() &&
            selected->registered && bridge::IsValidStableId(selected->assetId);
        const bool selectedReusable = selectedRegistered &&
            selected->importedProduct && selected->productAvailable &&
            fs::u8path(selected->projectRelativePath).extension() == ".rasset";
        creatorAssetPlaceButton_.SetEnabled(selectedReusable);
        creatorAssetReimportButton_.SetEnabled(
            selectedRegistered && selected->importedProduct);
        creatorAssetSaveTagsButton_.SetEnabled(selectedRegistered);

        creatorAssetImportButton_.Update(canvas, dt);
        creatorAssetPlaceButton_.Update(canvas, dt);
        creatorAssetReimportButton_.Update(canvas, dt);
        creatorAssetSaveTagsButton_.Update(canvas, dt);

        const std::string search = creatorAssetSearch_.GetText();
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

        if (creatorAssetRefreshPending_ &&
            !wi::jobsystem::IsBusy(creatorAssetWorkload_))
        {
            RefreshCreatorAssetBrowser();
        }
    }

    void CreatorAssetStudioChrome::RenderCreatorAssetControls(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (ActiveBottomTab() != 0)
            return;
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
        std::string error;
        bridge::AssetCatalogue catalogue;
        if (!creatorAssetWorkflow_.BuildCatalogue(
                project.rootPath, project.projectId, catalogue, error))
        {
            SetStatusText("ASSET BROWSER // REFRESH FAILED // " + error);
            return;
        }
        creatorAssetCatalogue_ = std::move(catalogue);

        const bridge::AssetCatalogueQuery query = CreatorAssetQuery();
        const auto matches = bridge::QueryAssetCatalogue(
            creatorAssetCatalogue_, query);
        const bool globalSearch = !query.text.empty();

        std::vector<AssetCard> cards;
        if (!globalSearch)
        {
            for (const auto& existing : creatorFilesystemAssets_)
            {
                if (existing.directory)
                    cards.push_back(existing);
            }
        }
        for (const auto& entry : matches)
        {
            if (!globalSearch && ParentPath(entry.projectRelativePath) !=
                    fs::u8path(creatorCurrentPath_)
                        .lexically_normal().generic_u8string())
                continue;

            AssetCard card;
            card.name = entry.name;
            card.relativePath = entry.projectRelativePath;
            card.directory = false;
            card.typeLabel = UpperAscii(bridge::AssetCatalogueStateLabel(entry.state));
            if (!entry.sourceFormat.empty())
                card.typeLabel += " / " + UpperAscii(entry.sourceFormat);
            if (entry.model.known && entry.model.skinned)
                card.typeLabel += " / SKIN";
            if (entry.model.known && entry.model.animated)
                card.typeLabel += " / ANIM";
            cards.push_back(std::move(card));
        }

        RenegadeStudioChrome::SetAssetBrowserData(
            creatorFilesystemFolders_, std::move(cards), creatorCurrentPath_);

        const auto selected = std::find_if(
            creatorAssetCatalogue_.entries.begin(),
            creatorAssetCatalogue_.entries.end(),
            [this](const bridge::AssetCatalogueEntry& entry)
            {
                return entry.projectRelativePath == creatorSelectedAssetPath_;
            });
        if (selected == creatorAssetCatalogue_.entries.end())
        {
            creatorSelectedAssetId_.clear();
            creatorSelectedAssetPath_.clear();
            creatorAssetTags_.SetValue("");
        }
        else
        {
            creatorSelectedAssetId_ = selected->assetId;
        }

        SetStatusText("ASSET BROWSER // " +
            std::to_string(matches.size()) + " CATALOGUE MATCHES // " +
            (globalSearch ? std::string("ALL CONTENT") : creatorCurrentPath_));
    }

    bool CreatorAssetStudioChrome::SelectCreatorAsset(
        const std::string& relativePath)
    {
        const auto found = std::find_if(
            creatorAssetCatalogue_.entries.begin(),
            creatorAssetCatalogue_.entries.end(),
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
        status << "ASSET // " << found->name
               << " // " << UpperAscii(bridge::AssetCatalogueStateLabel(found->state));
        if (!found->sourceFormat.empty())
            status << " // " << UpperAscii(found->sourceFormat);
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
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
        {
            wi::helper::messageBox(
                "Open or create a Renegade project before importing a model.",
                "Import Project Asset");
            return;
        }
        if (wi::jobsystem::IsBusy(creatorAssetWorkload_))
        {
            wi::helper::messageBox(
                "A creator asset import or reimport is already running.",
                "Import Project Asset");
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "FBX/GLTF/GLB model to import as a reusable project asset";
        params.extensions.push_back("fbx");
        params.extensions.push_back("gltf");
        params.extensions.push_back("glb");
        wi::helper::FileDialog(params, [this](const std::string& sourcePath)
        {
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, sourcePath](std::uint64_t)
                {
                    if (sourcePath.empty())
                        return;
                    auto* current = bridge::StudioSession::Current();
                    if (current == nullptr || !current->Projects().HasProject() ||
                        wi::jobsystem::IsBusy(creatorAssetWorkload_))
                        return;

                    const auto project = current->Projects().CurrentProject();
                    SetStatusText("IMPORT ASSET // RETAIN + CONVERT // " +
                        fs::u8path(sourcePath).filename().generic_u8string());
                    auto imported = std::make_shared<bridge::CreatorModelImportResult>();
                    auto prepared = std::make_shared<bridge::PreparedReusableModelPlacement>();
                    wi::jobsystem::Execute(
                        creatorAssetWorkload_,
                        [this, project, sourcePath, imported, prepared](wi::jobsystem::JobArgs)
                        {
                            bridge::CreatorAssetWorkflowService workflow;
                            *imported = workflow.ImportModel(
                                project.rootPath, project.projectId, sourcePath);
                            if (imported->succeeded)
                            {
                                *prepared = workflow.PrepareModelPlacement(
                                    project.rootPath,
                                    project.projectId,
                                    imported->asset.assetId);
                            }
                            wi::eventhandler::Subscribe_Once(
                                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                [this, imported, prepared](std::uint64_t)
                                {
                                    creatorAssetRefreshPending_ = true;
                                    if (!imported->succeeded)
                                    {
                                        SetStatusText("IMPORT ASSET // FAILED");
                                        wi::helper::messageBox(
                                            "Could not create the reusable project asset.\n\nReason: " +
                                                imported->error,
                                            "Import Project Asset");
                                        return;
                                    }
                                    creatorSelectedAssetId_ = imported->asset.assetId;
                                    creatorSelectedAssetPath_ =
                                        imported->assetProjectRelativePath;
                                    if (!prepared->IsReady())
                                    {
                                        SetStatusText(
                                            "IMPORT ASSET // CREATED // PLACEMENT FAILED");
                                        wi::helper::messageBox(
                                            "The .rasset was created, but its accepted payload could not be prepared for placement.\n\nReason: " +
                                                prepared->Result().error,
                                            "Import Project Asset");
                                        return;
                                    }
                                    PlacePreparedCreatorAsset(
                                        std::move(*prepared),
                                        fs::u8path(imported->assetProjectRelativePath)
                                            .filename().generic_u8string());
                                });
                        });
                });
        });
    }

    void CreatorAssetStudioChrome::PlacePreparedCreatorAsset(
        bridge::PreparedReusableModelPlacement prepared,
        const std::string& label)
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !prepared.IsReady())
            return;

        float scaleFactor = 1.0f;
        if (const auto* preparedScene = prepared.PeekScene())
        {
            scaleFactor = bridge::ImportService::ResolveScaleFactor(
                bridge::ModelScaleMode::Automatic, *preparedScene);
        }
        const XMFLOAT3 position(
            static_cast<float>(creatorPlacementSerial_) * 2.0f,
            0.0f,
            0.0f);

        auto command = std::make_unique<bridge::PlaceImportedModelCommand>(
            session->Scenes().GetScene(),
            prepared.ReleaseScene(),
            position,
            scaleFactor);
        auto* placed = command.get();
        if (!session->Commands().Execute(std::move(command)))
        {
            SetStatusText("PLACE ASSET // FAILED");
            wi::helper::messageBox(
                "The registered .rasset produced no placeable entity.",
                "Place Project Asset");
            return;
        }
        ++creatorPlacementSerial_;
        session->Selection().Select(placed->PlacedEntity());
        SetSceneDirty(session->Commands().IsDirty());
        SetSelectionName(label);
        RefreshCreatorHierarchyRows();
        creatorAssetRefreshPending_ = true;
        SetStatusText("PLACE ASSET // " + label +
            " // RASSET PAYLOAD // NO SOURCE RECONVERSION");
    }

    void CreatorAssetStudioChrome::PlaceSelectedCreatorAsset()
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(creatorSelectedAssetId_))
            return;
        const auto& project = session->Projects().CurrentProject();
        auto prepared = creatorAssetWorkflow_.PrepareModelPlacement(
            project.rootPath, project.projectId, creatorSelectedAssetId_);
        if (!prepared.IsReady())
        {
            SetStatusText("PLACE ASSET // FAILED");
            wi::helper::messageBox(
                "Could not prepare the selected reusable asset.\n\nReason: " +
                    prepared.Result().error,
                "Place Project Asset");
            return;
        }
        PlacePreparedCreatorAsset(
            std::move(prepared),
            fs::u8path(creatorSelectedAssetPath_).filename().generic_u8string());
    }

    void CreatorAssetStudioChrome::ReimportSelectedCreatorAsset()
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject() ||
            !bridge::IsValidStableId(creatorSelectedAssetId_))
            return;
        if (wi::jobsystem::IsBusy(creatorAssetWorkload_))
        {
            wi::helper::messageBox(
                "A creator asset import or reimport is already running.",
                "Reimport Project Asset");
            return;
        }

        const auto project = session->Projects().CurrentProject();
        const bridge::StableId assetId = creatorSelectedAssetId_;
        auto result = std::make_shared<bridge::ReusableModelReimportResult>();
        SetStatusText("REIMPORT ASSET // REFRESH + REPLAY RECIPE");
        wi::jobsystem::Execute(
            creatorAssetWorkload_,
            [this, project, assetId, result](wi::jobsystem::JobArgs)
            {
                bridge::CreatorAssetWorkflowService workflow;
                *result = workflow.ReimportModel(
                    project.rootPath, project.projectId, assetId);
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, result](std::uint64_t)
                    {
                        creatorAssetRefreshPending_ = true;
                        if (!result->succeeded)
                        {
                            SetStatusText("REIMPORT ASSET // FAILED");
                            wi::helper::messageBox(
                                "The selected asset was not replaced. The previous last-good .rasset remains authoritative.\n\nReason: " +
                                    result->error,
                                "Reimport Project Asset");
                            return;
                        }
                        SetStatusText(
                            "REIMPORT ASSET // CURRENT // SAME STABLE ID");
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
                project.rootPath,
                project.projectId,
                creatorSelectedAssetId_,
                CreatorTagInput(),
                error))
        {
            SetStatusText("SAVE TAGS // FAILED");
            wi::helper::messageBox(
                "Could not save creator asset tags.\n\nReason: " + error,
                "Asset Tags");
            return;
        }
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
