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
        // Wicked's TextInputField::GetValue() is non-const even though reading
        // the accepted input does not mutate creator workflow state.
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
            creatorAssetRefreshPending_ = true;
    }

    void CreatorAssetStudioChrome::OnAction(std::function<void(Action)> callback)
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
                if (!SelectCreatorAsset(path) && callback)
                    callback(path);
            });
    }

    bool CreatorAssetStudioChrome::ConsumedPointerThisFrame() const noexcept
    {
        return creatorAssetControlConsumed_ ||
            RenegadeStudioChrome::ConsumedPointerThisFrame();
    }

    void CreatorAssetStudioChrome::Update(const wi::Canvas& canvas, const float dt)
    {
        RenegadeStudioChrome::Update(canvas, dt);

        // Wicked serializes material texture filenames, while Renegade owns
        // governed resource identity. Gate 3 stores the stable texture ID in
        // WISCENE metadata and rehydrates its .rasset payload directly through
        // wi::resourcemanager after scene load. This call is idempotent once
        // the material has a live in-memory resource.
        auto* session = bridge::StudioSession::Current();
        if (session != nullptr && session->Projects().HasProject())
        {
            const auto& project = session->Projects().CurrentProject();
            const auto restored = bridge::RestoreMaterialTextureBindings(
                session->Scenes().GetScene(), project.rootPath, project.projectId);
            if (restored.succeeded && restored.restored > 0)
            {
                SetStatusText("TEXTURE BINDING // RESTORED " +
                    std::to_string(restored.restored) + " GOVERNED MATERIAL TEXTURE");
            }
        }

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
        creatorAssetImportButton_.SetText("IMPORT");
        creatorAssetImportButton_.SetTooltip(
            "Import a reusable FBX/GLTF/GLB model or governed image texture into this project.");
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
        const float left = HierarchyWidth() + 13.0f;
        const float right = creatorLayoutWidth_ - InspectorWidth() - 13.0f;
        const float available = std::max(0.0f, right - left);
        const float drawerTop = creatorLayoutHeight_ - BottomTabsHeight -
            StatusBarHeight - DrawerHeight();
        const float y = drawerTop + 45.0f;

        constexpr float gap = 4.0f;
        constexpr float stateWidth = 70.0f;
        constexpr float formatWidth = 58.0f;
        constexpr float rigWidth = 70.0f;
        constexpr float importWidth = 52.0f;
        constexpr float placeWidth = 78.0f;
        constexpr float reimportWidth = 64.0f;
        constexpr float saveWidth = 68.0f;
        constexpr float fixed = stateWidth + formatWidth + rigWidth +
            importWidth + placeWidth + reimportWidth + saveWidth + gap * 8.0f;
        const float flexible = std::max(160.0f, available - fixed);
        const float searchWidth = flexible * 0.5f;
        const float tagsWidth = flexible - searchWidth;

        float x = left;
        const auto place = [&x, y, gap](wi::gui::Widget& widget, const float width)
        {
            widget.SetPos(XMFLOAT2(x, y));
            widget.SetSize(XMFLOAT2(width, 25.0f));
            x += width + gap;
        };
        place(creatorAssetStateCombo_, stateWidth);
        place(creatorAssetFormatCombo_, formatWidth);
        place(creatorAssetRigCombo_, rigWidth);
        place(creatorAssetSearch_, searchWidth);
        place(creatorAssetTags_, tagsWidth);
        place(creatorAssetImportButton_, importWidth);
        place(creatorAssetPlaceButton_, placeWidth);
        place(creatorAssetReimportButton_, reimportWidth);
        place(creatorAssetSaveTagsButton_, saveWidth);
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
        bridge::AssetCatalogue catalogue;
        std::string error;
        if (!creatorAssetWorkflow_.BuildCatalogue(
                project.rootPath, project.projectId, catalogue, error))
        {
            SetStatusText("ASSET BROWSER // REFRESH FAILED // " + error);
            return;
        }
        bridge::CreatorTextureWorkflowService textureWorkflow;
        std::string textureWarning;
        (void)textureWorkflow.EnrichTextureCatalogue(
            project.rootPath, project.projectId, catalogue, textureWarning);
        creatorAssetCatalogue_ = std::move(catalogue);

        const auto query = CreatorAssetQuery();
        const auto matches = bridge::QueryAssetCatalogue(creatorAssetCatalogue_, query);
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
                    fs::u8path(creatorCurrentPath_).lexically_normal().generic_u8string())
                continue;
            AssetCard card;
            card.name = entry.name;
            card.relativePath = entry.projectRelativePath;
            card.typeLabel = UpperAscii(bridge::AssetCatalogueStateLabel(entry.state));
            if (!entry.sourceFormat.empty())
                card.typeLabel += " / " + UpperAscii(entry.sourceFormat);
            if (entry.dependencyClass == bridge::DependencyClass::Texture)
                card.typeLabel += " / TEXTURE";
            if (entry.model.known && entry.model.skinned)
                card.typeLabel += " / SKIN";
            if (entry.model.known && entry.model.animated)
                card.typeLabel += " / ANIM";
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

        if (!textureWarning.empty())
        {
            SetStatusText("ASSET BROWSER // TEXTURE METADATA WARNING // " +
                textureWarning);
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
                        bridge::PreparedReusableModelPlacement prepared;
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
                            if (state->imported.succeeded)
                            {
                                state->prepared = workflow.PrepareModelPlacement(
                                    state->projectRoot, state->projectId,
                                    state->imported.asset.assetId);
                            }
                            wi::eventhandler::Subscribe_Once(
                                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                [this, state](std::uint64_t)
                                {
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
                                    if (!state->prepared.IsReady())
                                    {
                                        SetStatusText("IMPORT ASSET // CREATED // PLACEMENT FAILED");
                                        wi::helper::messageBox(
                                            "The .rasset was created, but its payload could not be prepared for placement.\n\nReason: " +
                                                state->prepared.Result().error,
                                            "Import Project Asset");
                                        return;
                                    }
                                    PlacePreparedCreatorAsset(
                                        std::move(state->prepared),
                                        fs::u8path(state->imported.assetProjectRelativePath)
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

        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        const float scale = bridge::ImportService::ResolveScaleFactor(
            bridge::ModelScaleMode::Automatic, *preparedScene);
        const bridge::StableId assetId = prepared.Result().assetId;
        const XMFLOAT3 position(
            static_cast<float>(creatorPlacementSerial_) * 2.0f, 0.0f, 0.0f);
        auto command = std::make_unique<bridge::PlaceReusableModelCommand>(
            session->Scenes().GetScene(), prepared.ReleaseScene(), assetId,
            position, scale);
        auto* raw = command.get();
        if (!session->Commands().Execute(std::move(command)))
        {
            SetStatusText("PLACE ASSET // FAILED");
            return;
        }

        ++creatorPlacementSerial_;
        session->Selection().Select(raw->PlacedEntity());
        SetSceneDirty(session->Commands().IsDirty());
        SetSelectionName(label);
        RefreshCreatorHierarchyRows();
        creatorAssetRefreshPending_ = true;
        SetStatusText("PLACE ASSET // " + label +
            " // STABLE RASSET INSTANCE // NO SOURCE RECONVERSION");
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

        const auto& project = session->Projects().CurrentProject();
        auto prepared = creatorAssetWorkflow_.PrepareModelPlacement(
            project.rootPath, project.projectId, creatorSelectedAssetId_);
        if (!prepared.IsReady())
        {
            SetStatusText("PLACE ASSET // FAILED // " + prepared.Result().error);
            return;
        }
        PlacePreparedCreatorAsset(std::move(prepared),
            fs::u8path(creatorSelectedAssetPath_).filename().generic_u8string());
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
