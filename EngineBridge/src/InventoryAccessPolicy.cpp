#include "renegade/bridge/InventoryAccessPolicy.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace renegade::bridge {
namespace {
constexpr std::size_t MaxPolicyBytes = 1024u * 1024u;
constexpr std::size_t MaxGrants = 4096;
bool SafeIdentity(const std::string& s, std::size_t maxLength) {
    if (s.empty() || s.size() > maxLength) return false;
    for (const unsigned char c : s)
        if (c < 33 || c > 126 || c == '"' || c == '\\') return false;
    return true;
}
bool Fail(std::vector<InventoryAccessGrant>& out,
          std::string& error, const char* why) {
    out.clear();
    error = why;
    return false;
}
} // namespace

bool ParseInventoryAccessPolicy(const std::string& bytes,
        const InventoryService& inventory,
        std::vector<InventoryAccessGrant>& result,
        std::string& error) {
    result.clear();
    error.clear();
    if (bytes.empty() || bytes.size() > MaxPolicyBytes)
        return Fail(result, error, "Inventory access policy is empty or oversized.");
    std::istringstream input(bytes);
    input.imbue(std::locale::classic());
    std::string magic;
    unsigned version = 0;
    std::size_t count = 0;
    if (!(input >> magic >> version >> count) ||
        magic != "RENEGADE_INVENTORY_ACCESS" || version != 1 ||
        count > MaxGrants)
        return Fail(result, error, "Inventory access policy header/version is invalid.");

    std::string line;
    std::getline(input, line);
    std::vector<InventoryAccessGrant> parsed;
    parsed.reserve(count);
    std::map<std::string, std::string> owners;
    std::set<std::tuple<std::string, int, std::string>> uniqueGrants;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::getline(input, line) || line.empty() || line.size() > 1024)
            return Fail(result, error, "Inventory access policy row is missing or oversized.");
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string kind, script, owner, argument;
        int read = -1, deposit = -1, withdraw = -1;
        if (!(row >> kind >> std::quoted(script) >> std::quoted(owner)))
            return Fail(result, error, "Inventory access policy row identity is invalid.");
        InventoryAccessGrant grant;
        if (kind == "CONTAINER") {
            grant.kind = InventoryGrantKind::Container;
            if (!(row >> std::quoted(argument) >> read >> deposit >> withdraw) ||
                !inventory.FindContainer(argument) ||
                (read != 0 && read != 1) ||
                (deposit != 0 && deposit != 1) ||
                (withdraw != 0 && withdraw != 1) ||
                ((deposit || withdraw) && !read))
                return Fail(result, error, "Invalid inventory container permission.");
            grant.permissions = {read == 1, deposit == 1, withdraw == 1};
        } else if (kind == "CREATE") {
            grant.kind = InventoryGrantKind::CreateContainer;
            if (!(row >> std::quoted(argument)) ||
                !SafeIdentity(argument, 128))
                return Fail(result, error, "Invalid inventory creation namespace.");
        } else if (kind == "DEFINE") {
            grant.kind = InventoryGrantKind::DefineItems;
        } else {
            return Fail(result, error, "Unknown inventory permission capability.");
        }
        row >> std::ws;
        if (!row.eof() ||
            !SafeIdentity(script, 256) ||
            (owner != "-" && !SafeIdentity(owner, 256)))
            return Fail(result, error, "Inventory access policy contains invalid fields.");

        const std::string effectiveOwner = owner == "-" ? "" : owner;
        const auto insertedOwner = owners.emplace(script, effectiveOwner);
        if (!insertedOwner.second && insertedOwner.first->second != effectiveOwner)
            return Fail(result, error, "Inventory script has inconsistent entity ownership.");

        grant.scriptInstanceId = std::move(script);
        grant.ownerEntityId = effectiveOwner;
        grant.argument = std::move(argument);
        // A second DEFINE or CREATE per script is ambiguous, and repeated
        // container grants could silently override permissions. Reject all.
        const std::string& grantKey =
            grant.kind == InventoryGrantKind::Container ? grant.argument : std::string{};
        if (!uniqueGrants.emplace(grant.scriptInstanceId,
                                  static_cast<int>(grant.kind), grantKey).second)
            return Fail(result, error, "Inventory access policy has duplicate grants.");
        parsed.push_back(std::move(grant));
    }
    input >> std::ws;
    if (!input.eof())
        return Fail(result, error, "Inventory access policy has trailing content.");
    result = std::move(parsed);
    error.clear();
    return true;
}

bool ReadInventoryAccessPolicy(const std::string& path,
        const InventoryService& inventory,
        std::vector<InventoryAccessGrant>& result,
        std::string& error) {
    result.clear();
    std::error_code ec;
    const auto source = std::filesystem::u8path(path);
    if (!std::filesystem::is_regular_file(source, ec) || ec ||
        std::filesystem::file_size(source, ec) > MaxPolicyBytes || ec)
        return Fail(result, error, "Inventory access policy is missing or oversized.");
    std::ifstream in(source, std::ios::binary);
    if (!in)
        return Fail(result, error, "Inventory access policy cannot be opened.");
    std::string contents;
    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        contents.append(buffer, static_cast<std::size_t>(in.gcount()));
        if (contents.size() > MaxPolicyBytes)
            return Fail(result, error, "Inventory access policy exceeds byte limit.");
    }
    if (!in.eof())
        return Fail(result, error, "Inventory access policy read failed.");
    return ParseInventoryAccessPolicy(contents, inventory, result, error);
}
} // namespace renegade::bridge
