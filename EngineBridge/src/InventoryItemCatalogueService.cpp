#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "ScreenBindingPersistence.h"
#include <wiConfig.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace renegade::bridge {
namespace {
    template<typename T> bool Scalar(
        const wi::config::Section& section, const char* key, T& out) {
        if (!section.Has(key)) return false;
        std::istringstream in(section.GetText(key));
        in.imbue(std::locale::classic());
        in >> std::noskipws >> out;
        return !in.fail() && in.peek() == std::char_traits<char>::eof();
    }
    std::string WeightText(double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10)
            << value;
        return out.str();
    }
    bool SafeIcon(const std::string& path) {
        if (path.empty()) return true;
        if (path.size() > 512 || path.rfind("Content/", 0) != 0 ||
            path.find('\\') != std::string::npos ||
            path.find(':') != std::string::npos ||
            path.find("//") != std::string::npos ||
            path.find_first_of("\r\n\0", 0, 2) != std::string::npos)
            return false;
        const std::filesystem::path candidate = std::filesystem::u8path(path);
        if (candidate.is_absolute() || candidate.lexically_normal().generic_u8string() != path)
            return false;
        for (const auto& segment : candidate) {
            if (segment == ".." || segment == "." || segment == "")
                return false;
        }
        const std::string ext = candidate.extension().generic_u8string();
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".tga" || ext == ".bmp" || ext == ".dds";
    }
    bool ValidateItem(const InventoryAuthoringItem& item, std::string& error) {
        if (static_cast<unsigned>(item.kind) > static_cast<unsigned>(InventoryItemKind::KeyItem) ||
            static_cast<unsigned>(item.effect) > static_cast<unsigned>(InventoryItemEffect::RestoreHealth) ||
            item.maximumStack == 0 || item.maximumStack > 1000000 ||
            item.weightPerUnit < 0 || !std::isfinite(item.weightPerUnit)) {
            error = "Item type, stack maximum or weight is invalid.";
            return false;
        }
        if (item.effect == InventoryItemEffect::RestoreHealth) {
            if (item.kind != InventoryItemKind::Consumable ||
                item.healthRestored == 0 || item.healthRestored > 1000000) {
                error = "Restore Health requires a consumable and positive healing amount.";
                return false;
            }
        } else if (item.healthRestored != 0) {
            error = "Health Restored must be zero when the item has no healing effect.";
            return false;
        }
        if (!SafeIcon(item.iconProjectRelativePath)) {
            error = "Inventory icon must be a safe project-relative Content image.";
            return false;
        }
        // Reuse the authoritative InventoryService ID/text/weight validation.
        InventoryService validation;
        if (validation.RegisterItem(MakeRuntimeInventoryDefinition(item)) != InventoryResult::Success) {
            error = "Item ID, name, equipment reference or inventory attributes are invalid.";
            return false;
        }
        error.clear();
        return true;
    }
}

InventoryItemDefinition MakeRuntimeInventoryDefinition(const InventoryAuthoringItem& item) {
    return {item.id, item.name, item.maximumStack, item.weightPerUnit,
            item.equipmentAssetId};
}

const InventoryAuthoringItem* FindInventoryAuthoringItem(
        const InventoryItemCatalogue& catalogue, const std::string& id) noexcept {
    for (const auto& item : catalogue.items)
        if (item.id == id) return &item;
    return nullptr;
}

bool ValidateInventoryItemCatalogue(const InventoryItemCatalogue& catalogue,
        const StableId& expectedProjectId, std::string& error) {
    if (!ValidateDocumentEnvelope(catalogue.envelope, error)) return false;
    if (catalogue.envelope.documentType != InventoryItemCatalogueDocumentType ||
        catalogue.envelope.projectId != expectedProjectId ||
        catalogue.schemaVersion != 1 || catalogue.items.size() > 1024 ||
        catalogue.defaultMaximumStack == 0 ||
        catalogue.defaultMaximumStack > 1000000) {
        error = "Item catalogue ownership, schema, count or defaults are invalid.";
        return false;
    }
    std::set<std::string> ids;
    for (const auto& item : catalogue.items) {
        if (!ValidateItem(item, error)) return false;
        if (!ids.insert(item.id).second) {
            error = "Duplicate inventory item identifier: " + item.id;
            return false;
        }
    }
    error.clear();
    return true;
}

bool UpsertInventoryAuthoringItem(InventoryItemCatalogue& catalogue,
        InventoryAuthoringItem item, std::string& error) {
    if (!ValidateItem(item, error)) return false;
    InventoryItemCatalogue candidate = catalogue;
    bool replaced = false;
    for (auto& existing : candidate.items) {
        if (existing.id == item.id) {
            existing = item;
            replaced = true;
            break;
        }
    }
    if (!replaced) candidate.items.push_back(std::move(item));
    if (!ValidateInventoryItemCatalogue(candidate, candidate.envelope.projectId, error))
        return false;
    catalogue = std::move(candidate);
    error.clear();
    return true;
}

bool ReadInventoryItemCatalogue(const std::string& filePath,
        const StableId& expectedProjectId,
        InventoryItemCatalogue& result, std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_regular_file(fs::u8path(filePath), ec) || ec ||
        fs::file_size(fs::u8path(filePath), ec) > (1u << 20) || ec) {
        error = "Inventory item catalogue is absent or exceeds its file-size limit.";
        return false;
    }
    InventoryItemCatalogue candidate;
    if (!ReadDocumentEnvelope(filePath, candidate.envelope, error)) return false;
    try {
        wi::config::File file;
        if (!file.Open(filePath) || !file.HasSection("inventory_items")) {
            error = "Inventory item catalogue section is missing.";
            return false;
        }
        const auto& root = file.GetSection("inventory_items");
        int schema = 0, count = -1;
        if (!Scalar(root, "schema_version", schema) || schema != 1 ||
            !Scalar(root, "item_count", count) || count < 0 || count > 1024 ||
            !Scalar(root, "default_maximum_stack", candidate.defaultMaximumStack)) {
            error = "Malformed inventory item catalogue header.";
            return false;
        }
        candidate.schemaVersion = 1;
        for (int i = 0; i < count; ++i) {
            const auto section = "item_" + std::to_string(i);
            if (!file.HasSection(section.c_str())) {
                error = "An inventory item section is missing.";
                return false;
            }
            const auto& record = file.GetSection(section.c_str());
            InventoryAuthoringItem item;
            int kind = -1, effect = -1;
            if (!record.Has("id") || !record.Has("name_hex") ||
                !record.Has("icon_hex") || !record.Has("equipment_hex") ||
                !detail::DecodeBindingText(record.GetText("name_hex"), 256, item.name) ||
                !detail::DecodeBindingText(record.GetText("icon_hex"), 512, item.iconProjectRelativePath) ||
                !detail::DecodeBindingText(record.GetText("equipment_hex"), 256, item.equipmentAssetId) ||
                !Scalar(record, "kind", kind) || !Scalar(record, "effect", effect) ||
                !Scalar(record, "maximum_stack", item.maximumStack) ||
                !Scalar(record, "weight", item.weightPerUnit) ||
                !Scalar(record, "health_restored", item.healthRestored) ||
                kind < 0 || kind > 3 || effect < 0 || effect > 1) {
                error = "An inventory item has invalid or incomplete fields.";
                return false;
            }
            item.id = record.GetText("id");
            item.kind = static_cast<InventoryItemKind>(kind);
            item.effect = static_cast<InventoryItemEffect>(effect);
            candidate.items.push_back(std::move(item));
        }
        if (!ValidateInventoryItemCatalogue(candidate, expectedProjectId, error))
            return false;
        result = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Inventory item catalogue read failed: ") + exception.what();
        return false;
    }
}

bool WriteInventoryItemCatalogue(const std::string& filePath,
        const InventoryItemCatalogue& catalogue, std::string& error) {
    if (!ValidateInventoryItemCatalogue(catalogue, catalogue.envelope.projectId, error))
        return false;
    return WriteTransactionalDocument(
        filePath, catalogue.envelope, false,
        [&](wi::config::File& file) {
            auto& root = file.GetSection("inventory_items");
            root.Set("schema_version", 1);
            root.Set("default_maximum_stack", catalogue.defaultMaximumStack);
            root.Set("item_count", static_cast<int>(catalogue.items.size()));
            for (std::size_t i = 0; i < catalogue.items.size(); ++i) {
                const auto& item = catalogue.items[i];
                auto& section = file.GetSection(("item_" + std::to_string(i)).c_str());
                section.Set("id", item.id);
                section.Set("name_hex", detail::EncodeBindingText(item.name));
                section.Set("icon_hex", detail::EncodeBindingText(item.iconProjectRelativePath));
                section.Set("equipment_hex", detail::EncodeBindingText(item.equipmentAssetId));
                section.Set("kind", static_cast<int>(item.kind));
                section.Set("effect", static_cast<int>(item.effect));
                section.Set("health_restored", item.healthRestored);
                section.Set("maximum_stack", item.maximumStack);
                section.Set("weight", WeightText(item.weightPerUnit));
            }
        },
        [id = catalogue.envelope.documentId,
         projectId = catalogue.envelope.projectId](const std::string& staged, std::string& checkError) {
            InventoryItemCatalogue check;
            return ReadInventoryItemCatalogue(staged, projectId, check, checkError) &&
                check.envelope.documentId == id;
        }, error);
}
}
