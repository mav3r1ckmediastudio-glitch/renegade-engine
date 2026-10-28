#include "StudioApplication.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace renegade::studio {
namespace {
constexpr wi::Color PickupPanelColor = wi::Color(8, 12, 16, 255);
constexpr wi::Color PickupMutedColor = wi::Color(178, 178, 176, 255);
void PickupStatus(wi::gui::Label& label, std::string message) {
    const bool warning = message.rfind("ERROR", 0) == 0 ||
        message.find("FAILED") != std::string::npos;
    label.font.params.color = warning
        ? wi::Color(235, 89, 89, 255) : PickupMutedColor;
    label.SetText(std::move(message));
}
namespace fs = std::filesystem;
fs::path ItemCataloguePath(const std::string& root) {
    return fs::u8path(root) / "Content" / "Gameplay" / "Inventory.renegade-items";
}
bool PositiveField(const std::string& text, std::uint32_t& result) {
    if (text.empty() || text.size() > 9 ||
        !std::all_of(text.begin(), text.end(), [](unsigned char c) {
            return c >= '0' && c <= '9';
        })) return false;
    try {
        const auto n = std::stoull(text);
        if (n == 0 || n > 1000000) return false;
        result = static_cast<std::uint32_t>(n);
        return true;
    } catch (...) { return false; }
}
bool FloatField(const std::string& text, double& result) {
    if (text.empty() || text.size() > 30) return false;
    try {
        std::size_t consumed = 0;
        result = std::stod(text, &consumed);
        return consumed == text.size() && std::isfinite(result) &&
            result >= 0.0 && result <= 1000000.0;
    } catch (...) { return false; }
}
} // namespace

void StudioRenderPath::CreatePickupEditor() {
    pickupPanel_.Create("Pickup and Item", wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
    pickupPanel_.SetShadowRadius(0);
    pickupPanel_.SetColor(PickupPanelColor, wi::gui::WIDGET_ID_WINDOW_BASE);
    pickupPanel_.SetSize(XMFLOAT2(560, 740));
    const auto label = [this](wi::gui::Label& w, const char* id,
            const char* text, float y, float h = 21.0f) {
        w.Create(id);
        w.SetText(text);
        w.font.params.size = 12;
        w.font.params.color = PickupMutedColor;
        w.font.params.h_align = wi::font::WIFALIGN_LEFT;
        w.SetColor(wi::Color::Transparent());
        w.SetPos(XMFLOAT2(20, y));
        w.SetSize(XMFLOAT2(520, h));
        pickupPanel_.AddWidget(&w);
    };
    const auto textInput = [this](SceneInspectorTextInputField& w,
            const char* id, const char* hint, float y) {
        w.Create(id); w.SetDescription(std::string(id) + ": ");
        w.SetPlaceholder(hint);
        w.SetCancelInputEnabled(false);
        w.SetPos(XMFLOAT2(20, y)); w.SetSize(XMFLOAT2(520, 29));
        pickupPanel_.AddWidget(&w);
    };
    const auto combo = [this](SceneInspectorComboBox& w, const char* id, float y) {
        w.Create(id); w.SetPos(XMFLOAT2(20, y));
        w.SetSize(XMFLOAT2(520, 29)); pickupPanel_.AddWidget(&w);
    };
    const auto button = [this](SceneInspectorButton& w, const char* id,
            const char* text, float x, float y, float width = 250.0f) {
        w.Create(id); w.SetText(text);
        w.SetPos(XMFLOAT2(x, y)); w.SetSize(XMFLOAT2(width, 29));
        pickupPanel_.AddWidget(&w);
    };
    label(pickupTitle_, "Pickup Header", "GAMEPLAY // PICKUP & SHARED ITEM", 12, 23);
    label(pickupStatus_, "Pickup Status", "Select or create an item.", 40, 26);
    combo(pickupItemSelect_, "Inventory Item", 75);
    pickupItemSelect_.OnSelect([this](const wi::gui::EventArgs& e) {
        if (pickupRefreshing_) return;
        const std::size_t index = static_cast<std::size_t>(e.userdata);
        if (index == 0 || index > pickupCatalogue_.items.size()) return;
        pickupDraft_ = pickupCatalogue_.items[index - 1];
        pickupConfig_.itemId = pickupDraft_.id;
        RefreshPickupEditor();
    });
    button(pickupNewItem_, "New Inventory Item", "NEW ITEM", 20, 112);
    button(pickupSaveItem_, "Save Inventory Item", "SAVE ITEM", 288, 112);
    pickupNewItem_.OnClick([this](const wi::gui::EventArgs&) {
        pickupDraft_ = {};
        pickupDraft_.id = "item-" + bridge::GenerateStableId();
        pickupDraft_.name = "New Item";
        pickupDraft_.maximumStack = pickupCatalogue_.defaultMaximumStack;
        pickupConfig_.itemId = pickupDraft_.id;
        RefreshPickupEditor();
        PickupStatus(pickupStatus_, "NEW ITEM // Edit fields and click SAVE ITEM.");
    });
    pickupSaveItem_.OnClick([this](const wi::gui::EventArgs&) { SavePickupItem(); });
    textInput(pickupItemName_, "Shared Item Name", "ITEM NAME (e.g. Health Potion)", 150);
    combo(pickupKind_, "Item Kind", 188);
    pickupKind_.AddItem("CONSUMABLE", 0);
    pickupKind_.AddItem("EQUIPMENT", 1);
    pickupKind_.AddItem("MATERIAL", 2);
    pickupKind_.AddItem("KEY ITEM", 3);
    pickupKind_.OnSelect([this](const wi::gui::EventArgs& e) {
        pickupDraft_.kind = static_cast<bridge::InventoryItemKind>(e.userdata);
        if ((pickupDraft_.kind == bridge::InventoryItemKind::Equipment ||
             pickupDraft_.kind == bridge::InventoryItemKind::KeyItem) &&
            pickupStack_.GetValue() == std::to_string(pickupCatalogue_.defaultMaximumStack))
            pickupStack_.SetValue("1");
    });
    combo(pickupEffect_, "Item Use Effect", 226);
    pickupEffect_.AddItem("NO EFFECT", 0);
    pickupEffect_.AddItem("RESTORE HEALTH", 1);
    pickupEffect_.OnSelect([this](const wi::gui::EventArgs& e) {
        pickupDraft_.effect = static_cast<bridge::InventoryItemEffect>(e.userdata);
    });
    textInput(pickupHeal_, "Health Restored", "HEALTH RESTORED (e.g. 50; 0 = none)", 264);
    textInput(pickupStack_, "Maximum Stack Size", "MAXIMUM PER STACK", 302);
    textInput(pickupWeight_, "Weight Per Item", "WEIGHT PER ITEM (optional)", 340);
    button(pickupChooseIcon_, "Choose Item Icon", "ADD / CHANGE ICON", 20, 377, 520);
    pickupChooseIcon_.OnClick([this](const wi::gui::EventArgs&) { ChoosePickupItemIcon(); });
    label(pickupIconStatus_, "Item Icon Path", "ICON // NONE", 413, 28);
    label(pickupWorldLabel_, "World Pickup Header", "WORLD OBJECT // COLLECTION", 455, 20);
    pickupEnabled_.Create("Can be collected");
    pickupEnabled_.SetTooltip("Only opted-in objects with a saved item definition are collectable.");
    pickupEnabled_.SetPos(XMFLOAT2(20, 483)); pickupEnabled_.SetSize(XMFLOAT2(520, 27));
    pickupPanel_.AddWidget(&pickupEnabled_);
    textInput(pickupQty_, "Pickup Quantity", "PICKUP QUANTITY (e.g. 3)", 521);
    combo(pickupMethod_, "Collection Method", 559);
    pickupMethod_.AddItem("PRESS E TO COLLECT", 0);
    pickupMethod_.AddItem("COLLECT ON TOUCH", 1);
    pickupMethod_.OnSelect([this](const wi::gui::EventArgs& e) {
        pickupConfig_.method = static_cast<bridge::InventoryPickupMethod>(e.userdata);
    });
    pickupRemove_.Create("Remove world object after collection");
    pickupRemove_.SetPos(XMFLOAT2(20, 597));
    pickupRemove_.SetSize(XMFLOAT2(520, 28));
    pickupPanel_.AddWidget(&pickupRemove_);
    textInput(pickupDistance_, "Interaction Distance", "PRESS E DISTANCE (metres)", 633);
    button(pickupApply_, "Apply World Pickup", "APPLY PICKUP", 20, 684);
    button(pickupClose_, "Close Pickup Inspector", "CLOSE", 288, 684);
    pickupApply_.OnClick([this](const wi::gui::EventArgs&) { ApplyPickupToScene(); });
    pickupClose_.OnClick([this](const wi::gui::EventArgs&) {
        pickupPanel_.SetVisible(false);
    });
    pickupPanel_.SetVisible(false);
    GetGUI().AddWidget(&pickupPanel_);
}

void StudioRenderPath::RefreshPickupEditor() {
    pickupRefreshing_ = true;
    pickupItemSelect_.ClearItems();
    pickupItemSelect_.AddItem("CHOOSE SAVED ITEM", 0);
    std::uint64_t selected = 0;
    for (std::size_t i = 0; i < pickupCatalogue_.items.size(); ++i) {
        const auto& item = pickupCatalogue_.items[i];
        pickupItemSelect_.AddItem(item.name, static_cast<std::uint64_t>(i + 1));
        if (item.id == pickupDraft_.id) selected = i + 1;
    }
    pickupItemSelect_.SetSelectedByUserdataWithoutCallback(selected);
    pickupItemName_.SetValue(pickupDraft_.name);
    pickupKind_.SetSelectedByUserdataWithoutCallback(
        static_cast<std::uint64_t>(pickupDraft_.kind));
    pickupEffect_.SetSelectedByUserdataWithoutCallback(
        static_cast<std::uint64_t>(pickupDraft_.effect));
    pickupHeal_.SetValue(std::to_string(pickupDraft_.healthRestored));
    pickupStack_.SetValue(std::to_string(pickupDraft_.maximumStack));
    pickupWeight_.SetValue(std::to_string(pickupDraft_.weightPerUnit));
    const auto icon = fs::u8path(pickupDraft_.iconProjectRelativePath)
        .filename().generic_u8string();
    pickupIconStatus_.SetText(icon.empty() ? "ICON // NONE" : "ICON // " + icon);
    pickupEnabled_.SetCheck(pickupConfig_.canCollect);
    pickupQty_.SetValue(std::to_string(pickupConfig_.quantity));
    pickupMethod_.SetSelectedByUserdataWithoutCallback(
        static_cast<std::uint64_t>(pickupConfig_.method));
    pickupRemove_.SetCheck(pickupConfig_.removeWhenCollected);
    pickupDistance_.SetValue(std::to_string(pickupConfig_.interactDistance));
    pickupRefreshing_ = false;
}

void StudioRenderPath::OpenPickupEditor() {
    if (!session_ || !session_->Projects().HasProject() ||
        !session_->Selection().HasSelection()) return;
    const auto& project = session_->Projects().CurrentProject();
    pickupProjectId_ = project.projectId;
    pickupTarget_ = bridge::ResolveSceneComponentAuthoringRoot(
        session_->Scenes().GetScene(), session_->Selection().SelectedEntity());
    if (pickupTarget_ == wi::ecs::INVALID_ENTITY) return;
    const fs::path cataloguePath = ItemCataloguePath(project.rootPath);
    pickupCatalogue_ = {};
    std::string error;
    std::error_code ec;
    if (fs::exists(cataloguePath, ec)) {
        if (!bridge::ReadInventoryItemCatalogue(
                cataloguePath.generic_u8string(), project.projectId, pickupCatalogue_, error)) {
            ShowStudioMessageBox(error, "Inventory Item Catalogue");
            return;
        }
    } else {
        pickupCatalogue_.envelope = bridge::CreateDocumentEnvelope(
            project.projectId, bridge::InventoryItemCatalogueDocumentType,
            "Content/Gameplay/Inventory.renegade-items", "G5A");
    }
    if (!bridge::ReadInventoryPickupConfig(
            session_->Scenes().GetScene(), pickupTarget_, pickupConfig_, error)) {
        ShowStudioMessageBox(error, "World Pickup");
        return;
    }
    pickupDraft_ = {};
    if (!pickupConfig_.itemId.empty()) {
        if (const auto* item = bridge::FindInventoryAuthoringItem(
                pickupCatalogue_, pickupConfig_.itemId))
            pickupDraft_ = *item;
    }
    if (pickupDraft_.id.empty() && !pickupCatalogue_.items.empty()) {
        pickupDraft_ = pickupCatalogue_.items.front();
        // The UI may suggest the first item, but does not silently change the world instance.
    }
    if (pickupDraft_.id.empty()) {
        pickupDraft_.id = "item-" + bridge::GenerateStableId();
        pickupDraft_.name = "New Item";
        pickupDraft_.maximumStack = pickupCatalogue_.defaultMaximumStack;
    }
    RefreshPickupEditor();
    PickupStatus(pickupStatus_, pickupConfig_.canCollect
        ? "Pickup enabled. Choose an item and apply to scene."
        : "Select or save an item, then enable collection.");
    pickupPanel_.SetVisible(true);
}

void StudioRenderPath::SavePickupItem() {
    if (!session_ || !session_->Projects().HasProject() ||
        session_->Projects().CurrentProject().projectId != pickupProjectId_) return;
    auto candidate = pickupDraft_;
    candidate.name = pickupItemName_.GetValue();
    std::uint32_t stack = 0;
    if (!PositiveField(pickupStack_.GetValue(), stack)) {
        PickupStatus(pickupStatus_, "ERROR // Maximum stack must be 1 to 1,000,000.");
        return;
    }
    candidate.maximumStack = stack;
    double weight = 0;
    if (!FloatField(pickupWeight_.GetValue(), weight)) {
        PickupStatus(pickupStatus_, "ERROR // Weight must be a nonnegative number.");
        return;
    }
    candidate.weightPerUnit = weight;
    if (candidate.effect == bridge::InventoryItemEffect::RestoreHealth) {
        std::uint32_t health = 0;
        if (!PositiveField(pickupHeal_.GetValue(), health)) {
            PickupStatus(pickupStatus_, "ERROR // Health Restored must be positive.");
            return;
        }
        candidate.healthRestored = health;
    } else candidate.healthRestored = 0;

    bridge::InventoryItemCatalogue updated = pickupCatalogue_;
    std::string error;
    if (!bridge::UpsertInventoryAuthoringItem(updated, candidate, error)) {
        PickupStatus(pickupStatus_, "ERROR // " + error);
        return;
    }
    if (!bridge::WriteInventoryItemCatalogue(
            ItemCataloguePath(session_->Projects().CurrentProject().rootPath).generic_u8string(),
            updated, error)) {
        PickupStatus(pickupStatus_, "SAVE FAILED // " + error);
        return;
    }
    pickupCatalogue_ = std::move(updated);
    pickupDraft_ = std::move(candidate);
    pickupConfig_.itemId = pickupDraft_.id;
    RefreshPickupEditor();
    PickupStatus(pickupStatus_, "ITEM SAVED // Shared definition updated.");
}

void StudioRenderPath::ApplyPickupToScene() {
    if (!session_ || !session_->Projects().HasProject() ||
        session_->Projects().CurrentProject().projectId != pickupProjectId_ ||
        !session_->Selection().HasSelection() ||
        bridge::ResolveSceneComponentAuthoringRoot(session_->Scenes().GetScene(),
            session_->Selection().SelectedEntity()) != pickupTarget_) {
        PickupStatus(pickupStatus_, "ERROR // Reopen pickup settings for the selected object.");
        return;
    }
    auto config = pickupConfig_;
    config.canCollect = pickupEnabled_.GetCheck();
    config.removeWhenCollected = pickupRemove_.GetCheck();
    std::uint32_t count = 0;
    if (!PositiveField(pickupQty_.GetValue(), count)) {
        PickupStatus(pickupStatus_, "ERROR // Pickup quantity must be 1 to 1,000,000.");
        return;
    }
    config.quantity = count;
    double metres = 0;
    if (!FloatField(pickupDistance_.GetValue(), metres) ||
        metres < 0.5 || metres > 20.0) {
        PickupStatus(pickupStatus_, "ERROR // Pickup distance must be 0.5 to 20 metres.");
        return;
    }
    config.interactDistance = static_cast<float>(metres);
    if (config.canCollect &&
        !bridge::FindInventoryAuthoringItem(pickupCatalogue_, config.itemId)) {
        PickupStatus(pickupStatus_, "ERROR // SAVE ITEM before enabling collection.");
        return;
    }
    std::string error;
    if (!bridge::ValidateInventoryPickupConfig(config, error)) {
        PickupStatus(pickupStatus_, "ERROR // " + error);
        return;
    }
    const bool applied = session_->Commands().Execute(
        std::make_unique<bridge::SetInventoryPickupConfigCommand>(
            session_->Scenes().GetScene(), pickupTarget_, config));
    pickupConfig_ = std::move(config);
    PickupStatus(pickupStatus_, applied ? "PICKUP SAVED // Undo/Redo available." :
                                  "PICKUP UNCHANGED // No scene modification.");
    RefreshInspector();
    RefreshStatus();
}

void StudioRenderPath::ChoosePickupItemIcon() {
    if (!session_ || !session_->Projects().HasProject() ||
        pickupDraft_.id.empty()) return;
    const auto projectId = pickupProjectId_;
    const auto itemId = pickupDraft_.id;
    wi::helper::FileDialogParams params;
    params.type = wi::helper::FileDialogParams::OPEN;
    params.description = "Inventory item icon (PNG / JPG)";
    params.extensions = {"png", "jpg", "jpeg"};
    wi::helper::FileDialog(params, [this, projectId, itemId](const std::string& sourcePath) {
        if (sourcePath.empty()) return;
        wi::eventhandler::Subscribe_Once(
            wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this, projectId, itemId, sourcePath](std::uint64_t) {
                if (!session_ || !session_->Projects().HasProject() ||
                    session_->Projects().CurrentProject().projectId != projectId ||
                    pickupDraft_.id != itemId) return;
                const auto& root = session_->Projects().CurrentProject().rootPath;
                const auto source = fs::u8path(sourcePath);
                auto ext = source.extension().generic_u8string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::error_code ec;
                if ((ext != ".png" && ext != ".jpg" && ext != ".jpeg") ||
                    !fs::is_regular_file(source, ec) || ec ||
                    fs::file_size(source, ec) > 8u * 1024u * 1024u || ec) {
                    PickupStatus(pickupStatus_, "ERROR // Choose a PNG/JPG smaller than 8 MB.");
                    return;
                }
                std::ifstream input(source, std::ios::binary);
                std::vector<std::uint8_t> bytes(
                    (std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>());
                const bool png = ext == ".png" && bytes.size() >= 8 &&
                    bytes[0] == 0x89 && bytes[1] == 0x50 &&
                    bytes[2] == 0x4E && bytes[3] == 0x47;
                const bool jpg = (ext == ".jpg" || ext == ".jpeg") &&
                    bytes.size() >= 3 && bytes[0] == 0xFF &&
                    bytes[1] == 0xD8 && bytes[2] == 0xFF;
                if ((!input.eof() && input.fail()) || (!png && !jpg)) {
                    PickupStatus(pickupStatus_, "ERROR // Image file could not be validated.");
                    return;
                }
                const auto relative = "Content/UI/Icons/item-" +
                    bridge::GenerateStableId() + ext;
                const auto destination = fs::u8path(root) / fs::u8path(relative);
                fs::create_directories(destination.parent_path(), ec);
                if (ec) { PickupStatus(pickupStatus_, "ICON IMPORT FAILED // Cannot create icon folder."); return; }
                fs::create_directories(fs::u8path(root) / "Content" / "Gameplay", ec);
                if (ec) { PickupStatus(pickupStatus_, "ICON IMPORT FAILED // Cannot create journal folder."); return; }
                bridge::ProjectDocumentWrite write;
                write.destinationPath = destination.generic_u8string();
                write.content = std::move(bytes);
                bridge::ProjectDocumentTransactionOptions options;
                options.allowedRoot = root;
                options.journalDirectory = (fs::u8path(root) / "Content" / "Gameplay")
                    .generic_u8string();
                const auto saved = bridge::ProjectDocumentTransaction{}.Execute(
                    {std::move(write)}, std::move(options));
                if (!saved.success) {
                    PickupStatus(pickupStatus_, "ICON IMPORT FAILED // " + saved.message);
                    return;
                }
                pickupDraft_.iconProjectRelativePath = relative;
                pickupIconStatus_.SetText("ICON // " + destination.filename().generic_u8string());
                PickupStatus(pickupStatus_, "ICON IMPORTED // Click SAVE ITEM to apply.");
            });
    });
}
} // namespace renegade::studio
