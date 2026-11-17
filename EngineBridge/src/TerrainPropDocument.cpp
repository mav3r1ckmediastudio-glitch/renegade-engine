#include "renegade/bridge/TerrainPropDocument.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace renegade::bridge
{
    namespace fs = std::filesystem;
    namespace
    {
        constexpr char RevisionKey[] = "renegade.terrain_props.revision";
        constexpr char ProjectKey[] = "renegade.terrain_props.project";
        constexpr size_t MaxPageBytes = 24 + 65536 * 16 + 1000000 * 32;
        constexpr size_t MaxLayerPlacements = 1000000;
        constexpr size_t MaxManifestBytes = 4 * 1024 * 1024;
        fs::path Store(const fs::path& root) { return root / "Content/TerrainProps/Revisions"; }

        bool IsHex(const std::string& text, size_t size)
        {
            return text.size() == size && text.find_first_not_of("0123456789abcdef") == std::string::npos;
        }

        std::string Hex(const TerrainPropProfileId& id)
        {
            std::ostringstream out;
            out << std::hex << std::setfill('0');
            for (auto byte : id) out << std::setw(2) << unsigned(byte);
            return out.str();
        }

        bool ReadProduct(const fs::path& root, const std::string& hash, const char* extension,
            size_t limit, std::vector<std::uint8_t>& bytes, std::string& error)
        {
            if (!IsHex(hash, 64)) { error = "Invalid terrain prop digest."; return false; }
            const auto resolved = ResolveDependencyPath(root.generic_u8string(),
                "Content/TerrainProps/Revisions/" + hash + extension);
            if (!resolved.accepted) { error = resolved.error; return false; }
            std::ifstream in(fs::u8path(resolved.absolutePath), std::ios::binary | std::ios::ate);
            auto size = in.tellg();
            if (!in || size <= 0 || size > static_cast<std::streamoff>(limit))
            { error = "Missing or oversized terrain prop product " + hash; return false; }
            bytes.resize(static_cast<size_t>(size));
            in.seekg(0);
            if (!in.read(reinterpret_cast<char*>(bytes.data()), size))
            { error = "Incomplete terrain prop product " + hash; return false; }
            std::string actual;
            if (!ComputePayloadSha256(bytes, actual, error)) return false;
            if (actual != hash) { error = "Terrain prop digest mismatch " + hash; return false; }
            return true;
        }

        bool Validate(const TerrainPropDocument& doc, std::string& error)
        {
            if (!doc.nextPlacementId || !IsValidStableId(doc.layerId) || doc.profiles.size() > 4096 || doc.pages.size() > 32768)
            { error = "Invalid terrain prop layer identity or counts."; return false; }
            std::set<TerrainPropProfileId> ids;
            for (const auto& profile : doc.profiles)
            {
                const auto& c = profile.collision;
                if (!IsValidStableId(profile.modelAssetId) || profile.id == TerrainPropProfileId{} ||
                    !ids.insert(profile.id).second || static_cast<unsigned>(c.shape) > 2 ||
                    !std::isfinite(c.center.x) || !std::isfinite(c.center.y) || !std::isfinite(c.center.z) ||
                    !std::isfinite(c.halfExtents.x) || !std::isfinite(c.halfExtents.y) || !std::isfinite(c.halfExtents.z) ||
                    c.halfExtents.x <= 0 || c.halfExtents.y <= 0 || c.halfExtents.z <= 0)
                { error = "Invalid terrain prop profile."; return false; }
            }
            std::set<std::pair<int,int>> cells;
            std::unordered_set<std::uint64_t> placements;
            for (const auto& page : doc.pages)
            {
                if (!cells.emplace(page.cellX, page.cellZ).second)
                { error = "Duplicate terrain prop cell."; return false; }
                for (const auto& profile : page.profiles)
                    if (!ids.count(profile)) { error = "Undefined terrain prop profile."; return false; }
                if (page.placements.size() > MaxLayerPlacements - placements.size())
                { error = "Terrain prop layer exceeds placement budget."; return false; }
                for (const auto& p : page.placements)
                    if (p.id >= doc.nextPlacementId || !placements.insert(p.id).second) { error = "Placement ID exceeds layer counter or repeats."; return false; }
            }
            return true;
        }
    }

    bool PublishTerrainPropRevision(const fs::path& root, const StableId& projectId,
        const TerrainPropDocument& doc, wi::scene::Scene& scene, wi::ecs::Entity owner,
        std::string& error, ProjectDocumentTransactionOptions options)
    {
        error.clear();
        auto* existing = scene.metadatas.GetComponent(owner);
        if (!root.is_absolute() || !owner || (existing && existing->runtimeProjectionOnly) ||
            !IsValidStableId(projectId))
        { error = "Invalid terrain prop project or layer owner."; return false; }
        if (!Validate(doc, error)) return false;
        if (existing && existing->string_values.has(RevisionKey))
        {
            TerrainPropDocument previous;
            if (!ReadTerrainPropRevision(root, projectId, existing->string_values.get(RevisionKey), previous, error))
                return false;
            if (doc.layerId != previous.layerId || doc.nextPlacementId < previous.nextPlacementId)
            { error = "Cannot change the layer identity or rewind its placement counter."; return false; }
        }
        AssetRegistry registry;
        if (!ReadAssetRegistry(root.generic_u8string(), projectId, registry, error)) return false;
        for (const auto& profile : doc.profiles)
        {
            auto asset = std::find_if(registry.records.begin(), registry.records.end(),
                [&](const AssetRecord& record) { return record.assetId == profile.modelAssetId; });
            if (asset == registry.records.end() || fs::u8path(asset->projectRelativePath).extension() != ".rasset")
            { error = "Terrain prop profile requires a registered reusable model product."; return false; }
            const auto product = ResolveDependencyPath(root.generic_u8string(), asset->projectRelativePath);
            if (!product.accepted || !product.exists) { error = "Missing terrain prop model product."; return false; }
        }
        std::vector<ProjectDocumentWrite> writes;
        std::ostringstream manifest;
        manifest.imbue(std::locale::classic());
        manifest << std::setprecision(std::numeric_limits<float>::max_digits10)
            << "RPROPS 1 " << projectId << ' ' << doc.layerId << ' ' << doc.nextPlacementId << ' ' << doc.profiles.size() << ' ' << doc.pages.size() << '\n';
        for (const auto& profile : doc.profiles)
        {
            const auto& c = profile.collision;
            manifest << Hex(profile.id) << ' ' << profile.modelAssetId << ' ' << static_cast<unsigned>(c.shape)
                << ' ' << c.center.x << ' ' << c.center.y << ' ' << c.center.z
                << ' ' << c.halfExtents.x << ' ' << c.halfExtents.y << ' ' << c.halfExtents.z << '\n';
        }
        size_t encodedBytes = 0;
        for (const auto& page : doc.pages)
        {
            std::vector<std::uint8_t> bytes;
            std::string digest;
            if (!EncodeTerrainPropPage(page, bytes, error) || !ComputePayloadSha256(bytes, digest, error)) return false;
            encodedBytes += bytes.size();
            if (encodedBytes > 128ull * 1024 * 1024)
            { error = "Terrain prop layer exceeds encoded page budget."; return false; }
            manifest << digest << '\n';
            writes.push_back({(Store(root) / (digest + ".rppg")).generic_u8string(), std::move(bytes), {}});
        }
        const auto text = manifest.str();
        std::vector<std::uint8_t> bytes(text.begin(), text.end());
        std::string digest;
        if (!ComputePayloadSha256(bytes, digest, error)) return false;
        writes.push_back({(Store(root) / (digest + ".rprops")).generic_u8string(), std::move(bytes), {}});
        options.allowedRoot = root.generic_u8string();
        options.journalDirectory = (root / "Intermediate/Transactions").generic_u8string();
        auto result = ProjectDocumentTransaction().Execute(std::move(writes), std::move(options));
        if (!result.success) { error = result.code + " " + result.message; return false; }
        auto& metadata = existing ? *existing : scene.metadatas.Create(owner);
        metadata.string_values.set(ProjectKey, projectId);
        metadata.string_values.set(RevisionKey, digest);
        return true;
    }

    bool ReadTerrainPropRevision(const fs::path& root, const StableId& projectId, const std::string& digest,
        TerrainPropDocument& output, std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadProduct(root, digest, ".rprops", MaxManifestBytes, bytes, error)) return false;
        std::istringstream manifest(std::string(bytes.begin(), bytes.end()));
        manifest.imbue(std::locale::classic());
        TerrainPropDocument doc;
        unsigned version, profiles, pages;
        std::string magic, project;
        if (!(manifest >> magic >> version >> project >> doc.layerId >> doc.nextPlacementId >> profiles >> pages) ||
            magic != "RPROPS" || version != 1 || project != projectId || profiles > 4096 || pages > 32768)
        { error = "Invalid terrain prop manifest header."; return false; }
        for (unsigned i = 0; i < profiles; ++i)
        {
            TerrainPropProfile profile;
            unsigned shape;
            std::string id;
            auto& c = profile.collision;
            if (!(manifest >> id >> profile.modelAssetId >> shape >> c.center.x >> c.center.y >> c.center.z >>
                c.halfExtents.x >> c.halfExtents.y >> c.halfExtents.z) || shape > 2 || !IsHex(id, 32))
            { error = "Invalid terrain prop profile record."; return false; }
            c.shape = static_cast<TerrainPropShape>(shape);
            for (size_t n = 0; n < profile.id.size(); ++n)
                profile.id[n] = static_cast<std::uint8_t>(std::stoul(id.substr(n * 2, 2), nullptr, 16));
            doc.profiles.push_back(profile);
        }
        size_t placements = 0;
        size_t decodedBytes = 0;
        for (unsigned i = 0; i < pages; ++i)
        {
            std::string hash;
            TerrainPropPage page;
            if (!(manifest >> hash)) { error = "Missing terrain prop page digest."; return false; }
            if (!ReadProduct(root, hash, ".rppg", MaxPageBytes, bytes, error) ||
                !DecodeTerrainPropPage(bytes, page, error)) return false;
            placements += page.placements.size();
            decodedBytes += bytes.size();
            if (placements > MaxLayerPlacements || decodedBytes > 128ull * 1024 * 1024)
            { error = "Terrain prop layer exceeds decoded page budget."; return false; }
            doc.pages.push_back(std::move(page));
        }
        std::string extra;
        if (manifest >> extra) { error = "Trailing terrain prop manifest data."; return false; }
        if (!Validate(doc, error)) return false;
        output = std::move(doc);
        return true;
    }

    namespace
    {
        fs::path ProjectRootForScene(const fs::path& path)
        {
            auto root = fs::absolute(path).parent_path();
            std::error_code ec;
            while (root.has_relative_path() && !fs::is_regular_file(root / AssetRegistryDocumentName, ec))
                root = root.parent_path();
            return root;
        }
    }

    bool ValidateTerrainPropRevisionLinks(const wi::scene::Scene& scene, const fs::path& path, std::string& error)
    {
        std::set<StableId> layers;
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto& metadata = scene.metadatas[i];
            if (metadata.runtimeProjectionOnly || !metadata.string_values.has(RevisionKey)) continue;
            TerrainPropDocument doc;
            if (!ReadTerrainPropRevision(ProjectRootForScene(path), metadata.string_values.get(ProjectKey),
                metadata.string_values.get(RevisionKey), doc, error)) return false;
            if (!layers.insert(doc.layerId).second) { error = "Duplicate terrain prop layer identity."; return false; }
        }
        return true;
    }

    bool RestoreTerrainPropRevisions(wi::scene::Scene& scene, const fs::path& scenePath, std::string& error)
    {
        std::unique_ptr<wi::scene::Scene> staged;
        std::set<StableId> layers;
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto& properties = scene.metadatas[i].string_values;
            if (!properties.has(RevisionKey)) continue;
            const auto project = properties.get(ProjectKey);
            const auto root = ProjectRootForScene(scenePath);
            TerrainPropDocument doc;
            if (!ReadTerrainPropRevision(root, project, properties.get(RevisionKey), doc, error)) return false;
            if (!layers.insert(doc.layerId).second) { error = "Duplicate terrain prop layer identity."; return false; }
            wi::scene::Scene layer;
            TerrainPropProjection projection(layer);
            for (const auto& p : doc.profiles)
            {
                auto model = ReusableAssetService().PrepareModelAssetPlacement({root.generic_u8string(), project, p.modelAssetId});
                if (!model.IsReady()) { error = model.Result().error; return false; }
                if (!projection.Register(p.id, *model.PeekScene(), p.collision, error)) return false;
            }
            for (const auto& page : doc.pages)
                if (!projection.Place(page, error)) return false;
            if (!staged) staged = std::make_unique<wi::scene::Scene>();
            staged->Merge(layer);
        }
        if (staged) scene.Merge(*staged);
        return true;
    }
}
