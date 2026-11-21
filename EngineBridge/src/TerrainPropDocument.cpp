#include "renegade/bridge/TerrainPropDocument.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include <algorithm>
#include <cmath>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace renegade::bridge
{
    namespace fs = std::filesystem;
    namespace
    {
        constexpr char RevisionKey[] = "renegade.terrain_props.revision";
        constexpr char ProjectKey[] = "renegade.terrain_props.project";
        constexpr size_t MaxPageBytes = 24 + 65536 * 16 + 1000000 * 40;
        constexpr size_t MaxLayerPlacements = 1000000;
        constexpr size_t MaxManifestBytes = 128 * 1024 * 1024;
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


        bool ReadUnsigned(std::istream& in, std::uint64_t& value)
        {
            std::string token;
            if (!(in >> token) || token.empty() || token.find_first_not_of("0123456789") != std::string::npos) return false;
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
            return parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size();
        }
        bool ReadProfileId(std::istream& in, TerrainPropProfileId& id)
        {
            std::string hex;
            if (!(in >> hex) || !IsHex(hex, 32)) return false;
            for (size_t n = 0; n < id.size(); ++n)
                id[n] = static_cast<std::uint8_t>(std::stoul(hex.substr(n * 2, 2), nullptr, 16));
            return true;
        }
        void WriteMix(std::ostream& out, const TerrainPropMix& mix)
        {
            out << std::quoted(mix.name) << ' ' << mix.entries.size() << '\n';
            for (const auto& e : mix.entries)
                out << Hex(e.profileId) << ' ' << e.densityPerSquareMetre << ' ' << e.minSpacing
                    << ' ' << e.minScale << ' ' << e.maxScale << '\n';
        }
        bool ReadMix(std::istream& in, TerrainPropMix& mix)
        {
            size_t count;
            if (!(in >> std::quoted(mix.name) >> count) || mix.name.size() > 256 || count > 64) return false;
            for (size_t i = 0; i < count; ++i)
            {
                TerrainPropMixEntry entry;
                if (!ReadProfileId(in, entry.profileId) ||
                    !(in >> entry.densityPerSquareMetre >> entry.minSpacing >> entry.minScale >> entry.maxScale)) return false;
                mix.entries.push_back(entry);
            }
            return true;
        }
        void WriteAuthoring(std::ostream& out, const TerrainPropAuthoringState& a)
        {
            out << "AUTHORING 1 " << a.recipe.seed << ' ' << a.recipe.cellSize << ' ' << a.recipe.candidateStep
                << ' ' << a.savedMixes.size() << ' ' << a.bindings.size() << ' ' << a.tombstones.size()
                << ' ' << a.clearings.size() << ' ' << a.overrides.size() << '\n';
            WriteMix(out, a.recipe.mix);
            for (const auto& mix : a.savedMixes) WriteMix(out, mix);
            for (const auto& b : a.bindings) out << b.candidateKey << ' ' << b.placementId << '\n';
            for (auto key : a.tombstones) out << key << '\n';
            for (const auto& c : a.clearings) out << c.x << ' ' << c.z << ' ' << c.radius << '\n';
            for (const auto& v : a.overrides)
                out << v.candidateKey << ' ' << v.placementId << ' ' << Hex(v.profileId)
                    << ' ' << v.position[0] << ' ' << v.position[1] << ' ' << v.position[2]
                    << ' ' << v.yawRadians << ' ' << v.scale << '\n';
        }
        bool ReadAuthoring(std::istream& in, TerrainPropAuthoringState& a)
        {
            std::string magic;
            unsigned version;
            size_t mixes, bindings, tombstones, clearings, overrides;
            if (!(in >> magic >> version) || !ReadUnsigned(in, a.recipe.seed) ||
                !(in >> a.recipe.cellSize >> a.recipe.candidateStep >>
                mixes >> bindings >> tombstones >> clearings >> overrides) || magic != "AUTHORING" || version != 1 ||
                mixes > 1024 || bindings > MaxLayerPlacements || tombstones > MaxLayerPlacements ||
                clearings > MaxLayerPlacements || overrides > MaxLayerPlacements || !ReadMix(in, a.recipe.mix)) return false;
            for (size_t i = 0; i < mixes; ++i)
            {
                TerrainPropMix mix;
                if (!ReadMix(in, mix)) return false;
                a.savedMixes.push_back(std::move(mix));
            }
            a.bindings.resize(bindings);
            for (auto& b : a.bindings)
                if (!ReadUnsigned(in, b.candidateKey) || !ReadUnsigned(in, b.placementId)) return false;
            a.tombstones.resize(tombstones);
            for (auto& key : a.tombstones) if (!ReadUnsigned(in, key)) return false;
            a.clearings.resize(clearings);
            for (auto& c : a.clearings) if (!(in >> c.x >> c.z >> c.radius)) return false;
            a.overrides.resize(overrides);
            for (auto& v : a.overrides)
                if (!ReadUnsigned(in, v.candidateKey) || !ReadUnsigned(in, v.placementId) || !ReadProfileId(in, v.profileId) ||
                    !(in >> v.position[0] >> v.position[1] >> v.position[2] >> v.yawRadians >> v.scale)) return false;
            return true;
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
                if (!std::isfinite(profile.drawDistance) || profile.drawDistance < 0 || !IsValidStableId(profile.modelAssetId) ||
                    (!profile.coarseModelAssetId.empty() && !IsValidStableId(profile.coarseModelAssetId)) || profile.id == TerrainPropProfileId{} ||
                    !ids.insert(profile.id).second || static_cast<unsigned>(c.shape) > 2 ||
                    !std::isfinite(c.center.x) || !std::isfinite(c.center.y) || !std::isfinite(c.center.z) ||
                    !std::isfinite(c.halfExtents.x) || !std::isfinite(c.halfExtents.y) || !std::isfinite(c.halfExtents.z) ||
                    c.halfExtents.x <= 0 || c.halfExtents.y <= 0 || c.halfExtents.z <= 0 ||
                    !std::isfinite(c.friction) || c.friction<0 || c.friction>1 ||
                    static_cast<unsigned>(c.surface)>static_cast<unsigned>(ImpactSurfaceType::Character))
                { error = "Invalid terrain prop profile."; return false; }
            }

            if (!ValidateTerrainPropAuthoring(doc.authoring, error)) return false;
            auto validMix = [&](const TerrainPropMix& mix)
            {
                return std::all_of(mix.entries.begin(), mix.entries.end(),
                    [&](const TerrainPropMixEntry& e) { return ids.count(e.profileId) != 0; });
            };
            if (!validMix(doc.authoring.recipe.mix) ||
                !std::all_of(doc.authoring.savedMixes.begin(), doc.authoring.savedMixes.end(), validMix))
            { error = "Mix references an undefined terrain prop profile."; return false; }
            std::unordered_map<std::uint64_t, std::uint64_t> bindings;
            for (const auto& b : doc.authoring.bindings)
            {
                if (b.placementId >= doc.nextPlacementId)
                { error = "Candidate binding exceeds placement high-water mark."; return false; }
                bindings.emplace(b.candidateKey, b.placementId);
            }
            for (const auto& v : doc.authoring.overrides)
                if (!ids.count(v.profileId) || v.placementId >= doc.nextPlacementId ||
                    (v.candidateKey && (!bindings.count(v.candidateKey) || bindings.at(v.candidateKey) != v.placementId)))
                { error = "Override references an undefined profile or candidate identity."; return false; }
            std::unordered_set<std::uint64_t> candidateKeys;
            std::set<std::pair<int,int>> cells;
            std::unordered_set<std::uint64_t> placements;
            for (const auto& page : doc.pages)
            {
                if (!ValidateTerrainPropPage(page, error)) return false;
                if (!cells.emplace(page.cellX, page.cellZ).second)
                { error = "Duplicate terrain prop cell."; return false; }
                for (const auto& profile : page.profiles)
                    if (!ids.count(profile)) { error = "Undefined terrain prop profile."; return false; }
                if (page.placements.size() > MaxLayerPlacements - placements.size())
                { error = "Terrain prop layer exceeds placement budget."; return false; }

                for (const auto& p : page.placements)
                {
                    if (p.id >= doc.nextPlacementId || !placements.insert(p.id).second)
                    { error = "Placement ID exceeds layer counter or repeats."; return false; }
                    if (p.candidateKey && (!candidateKeys.insert(p.candidateKey).second ||
                        !bindings.count(p.candidateKey) || bindings.at(p.candidateKey) != p.id))
                    { error = "Placement candidate key repeats or differs from its binding."; return false; }
                }
            }
            return true;
        }
    }

    bool ComputeTerrainPropDocumentContentDigest(const TerrainPropDocument& doc,
        std::string& digest, std::string& error)
    {
        // Explicit length/count delimiters keep the hash unambiguous, including
        // arbitrary valid mix names and binary pages. Allocation high-water is
        // excluded so chained Undo can retain it without changing authored content.
        std::ostringstream content;
        content.imbue(std::locale::classic());
        content << std::setprecision(std::numeric_limits<float>::max_digits10)
            << doc.layerId.size() << ':' << doc.layerId << ' ' << doc.profiles.size() << ' ' << doc.pages.size() << '\n';
        for (const auto& profile : doc.profiles)
        {
            const auto& c = profile.collision;
            content << Hex(profile.id) << ' ' << profile.modelAssetId << ' ' << static_cast<unsigned>(c.shape)
                << ' ' << c.center.x << ' ' << c.center.y << ' ' << c.center.z
                << ' ' << c.halfExtents.x << ' ' << c.halfExtents.y << ' ' << c.halfExtents.z
                << ' ' << c.friction << ' ' << static_cast<unsigned>(c.surface);
            if (profile.drawDistance > 0) content << " distance " << profile.drawDistance;
            if (!profile.coarseModelAssetId.empty()) content << ' ' << profile.coarseModelAssetId;
            content << '\n';
        }
        WriteAuthoring(content, doc.authoring);
        for (const auto& page : doc.pages)
        {
            std::vector<std::uint8_t> bytes;
            if (!EncodeTerrainPropPage(page, bytes, error)) return false;
            content << bytes.size() << ':';
            content.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        const auto text = content.str();
        const std::vector<std::uint8_t> bytes(text.begin(), text.end());
        return ComputePayloadSha256(bytes, digest, error);
    }

    bool TerrainPropDocumentContentEqual(const TerrainPropDocument& a, const TerrainPropDocument& b)
    {
        if (a.layerId != b.layerId || a.profiles.size() != b.profiles.size() || a.pages.size() != b.pages.size()) return false;
        for (size_t i = 0; i < a.profiles.size(); ++i)
        {
            const auto& x = a.profiles[i]; const auto& y = b.profiles[i];
            if (x.id != y.id || x.modelAssetId != y.modelAssetId || x.drawDistance != y.drawDistance || x.coarseModelAssetId != y.coarseModelAssetId || x.collision.shape != y.collision.shape ||
                x.collision.center.x != y.collision.center.x || x.collision.center.y != y.collision.center.y ||
                x.collision.center.z != y.collision.center.z || x.collision.halfExtents.x != y.collision.halfExtents.x ||
                x.collision.halfExtents.y != y.collision.halfExtents.y || x.collision.halfExtents.z != y.collision.halfExtents.z ||
                x.collision.friction != y.collision.friction || x.collision.surface != y.collision.surface) return false;
        }
        std::ostringstream x, y;
        x.imbue(std::locale::classic()); y.imbue(std::locale::classic());
        x << std::setprecision(std::numeric_limits<float>::max_digits10);
        y << std::setprecision(std::numeric_limits<float>::max_digits10);
        WriteAuthoring(x, a.authoring); WriteAuthoring(y, b.authoring);
        if (x.str() != y.str()) return false;
        for (size_t i = 0; i < a.pages.size(); ++i)
        {
            std::vector<std::uint8_t> first, second; std::string error;
            if (!EncodeTerrainPropPage(a.pages[i], first, error) || !EncodeTerrainPropPage(b.pages[i], second, error) || first != second) return false;
        }
        return true;
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
        const auto checkModel = [&](const StableId& model) {
            auto asset = std::find_if(registry.records.begin(), registry.records.end(),
                [&](const AssetRecord& record) { return record.assetId == model; });
            if (asset == registry.records.end() || fs::u8path(asset->projectRelativePath).extension() != ".rasset")
            { error = "Terrain prop profile requires a registered reusable model product."; return false; }
            const auto product = ResolveDependencyPath(root.generic_u8string(), asset->projectRelativePath);
            if (!product.accepted || !product.exists) { error = "Missing terrain prop model product."; return false; }
            return true;
        };
        bool hasCoarse = false;
        bool hasDistance = false;
        for (const auto& profile : doc.profiles)
        {
            hasDistance |= profile.drawDistance > 0;
            if (!checkModel(profile.modelAssetId)) return false;
            if (profile.coarseModelAssetId.empty()) continue;
            if (!checkModel(profile.coarseModelAssetId)) return false;
            hasCoarse = true;
        }
        std::vector<ProjectDocumentWrite> writes;
        std::ostringstream manifest;
        manifest.imbue(std::locale::classic());
        manifest << std::setprecision(std::numeric_limits<float>::max_digits10)
            << "RPROPS " << (hasDistance ? 5 : hasCoarse ? 4 : 3) << ' ' << projectId << ' ' << doc.layerId << ' ' << doc.nextPlacementId << ' ' << doc.profiles.size() << ' ' << doc.pages.size() << '\n';
        for (const auto& profile : doc.profiles)
        {
            const auto& c = profile.collision;
            manifest << Hex(profile.id) << ' ' << profile.modelAssetId << ' ' << static_cast<unsigned>(c.shape)
                << ' ' << c.center.x << ' ' << c.center.y << ' ' << c.center.z
                << ' ' << c.halfExtents.x << ' ' << c.halfExtents.y << ' ' << c.halfExtents.z
                << ' ' << c.friction << ' ' << static_cast<unsigned>(c.surface);
            if (hasCoarse || hasDistance) manifest << ' ' << (profile.coarseModelAssetId.empty() ? "-" : profile.coarseModelAssetId);
            if (hasDistance) manifest << ' ' << profile.drawDistance;
            manifest << '\n';
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
        WriteAuthoring(manifest, doc.authoring);
        const auto text = manifest.str();
        if (text.size() > MaxManifestBytes) { error = "Terrain prop manifest exceeds size budget."; return false; }
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

    static bool ReadRevision(const fs::path& root, const StableId& projectId, const std::string& digest,
        TerrainPropDocument& output, std::string& error, std::set<std::string>* files)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadProduct(root, digest, ".rprops", MaxManifestBytes, bytes, error)) return false;
        std::istringstream manifest(std::string(bytes.begin(), bytes.end()));
        manifest.imbue(std::locale::classic());
        TerrainPropDocument doc;
        unsigned version, profiles, pages;
        std::string magic, project;
        if (!(manifest >> magic >> version >> project >> doc.layerId) || !ReadUnsigned(manifest, doc.nextPlacementId) ||
            !(manifest >> profiles >> pages) ||
            magic != "RPROPS" || (version < 1 || version > 5) || project != projectId || profiles > 4096 || pages > 32768)
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
            if(version>=3)
            {
                unsigned surface;
                if(!(manifest>>c.friction>>surface) ||
                    surface>static_cast<unsigned>(ImpactSurfaceType::Character))
                {error="Invalid terrain collision surface record.";return false;}
                c.surface=static_cast<ImpactSurfaceType>(surface);
            }
            if (version >= 4)
            {
                std::string coarse;
                if (!(manifest >> coarse) || (coarse != "-" && !IsValidStableId(coarse)))
                { error = "Invalid coarse terrain model identity."; return false; }
                if (coarse != "-") profile.coarseModelAssetId = std::move(coarse);
            }
            if (version >= 5 && (!(manifest >> profile.drawDistance) || !std::isfinite(profile.drawDistance) || profile.drawDistance < 0))
            { error = "Invalid terrain prop draw distance."; return false; }
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
            if (files) files->insert((Store({}) / (hash + ".rppg")).generic_u8string());
            placements += page.placements.size();
            decodedBytes += bytes.size();
            if (placements > MaxLayerPlacements || decodedBytes > 128ull * 1024 * 1024)
            { error = "Terrain prop layer exceeds decoded page budget."; return false; }
            doc.pages.push_back(std::move(page));
        }
        if (version >= 2 && !ReadAuthoring(manifest, doc.authoring))
        { error = "Invalid terrain prop authoring records."; return false; }
        std::string extra;
        if (manifest >> extra) { error = "Trailing terrain prop manifest data."; return false; }
        if (!Validate(doc, error)) return false;
        output = std::move(doc);
        if (files) files->insert((Store({}) / (digest + ".rprops")).generic_u8string());
        return true;
    }

    bool ReadTerrainPropRevision(const fs::path& root, const StableId& projectId, const std::string& digest,
        TerrainPropDocument& output, std::string& error)
    {
        return ReadRevision(root, projectId, digest, output, error, nullptr);
    }

    bool CaptureTerrainPropRevisionRefs(const wi::scene::Scene& scene, const StableId& projectId,
        std::vector<std::string>& revisions, std::string& error)
    {
        std::vector<std::string> captured;
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto& metadata = scene.metadatas[i];
            const auto& properties = metadata.string_values;
            if (metadata.runtimeProjectionOnly || !properties.has(RevisionKey)) continue;
            if (!properties.has(ProjectKey) || properties.get(ProjectKey) != projectId)
            {
                error = "Terrain prop dependency belongs to a different project.";
                return false;
            }
            captured.push_back(properties.get(RevisionKey));
        }
        revisions.swap(captured);
        return true;
    }

    bool CollectTerrainPropDependencies(const wi::scene::Scene& scene, const fs::path& root,
        const StableId& projectId, std::vector<std::string>& files, std::vector<StableId>& models,
        std::string& error)
    {
        std::vector<std::string> revisions;
        return CaptureTerrainPropRevisionRefs(scene, projectId, revisions, error) &&
            CollectTerrainPropDependencies(revisions, root, projectId, files, models, error);
    }

    bool CollectTerrainPropDependencies(const std::vector<std::string>& revisions, const fs::path& root,
        const StableId& projectId, std::vector<std::string>& files, std::vector<StableId>& models,
        std::string& error)
    {
        std::set<std::string> paths;
        std::set<StableId> modelIds, layers;
        for (const auto& revision : revisions)
        {
            TerrainPropDocument document;
            if (!ReadRevision(root, projectId, revision, document, error, &paths)) return false;
            if (!layers.insert(document.layerId).second)
            {
                error = "Duplicate terrain prop layer identity.";
                return false;
            }
            for (const auto& profile : document.profiles)
            {
                modelIds.insert(profile.modelAssetId);
                if (!profile.coarseModelAssetId.empty()) modelIds.insert(profile.coarseModelAssetId);
            }
        }
        files.assign(paths.begin(), paths.end());
        models.assign(modelIds.begin(), modelIds.end());
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

    bool RestoreTerrainPropRevisions(wi::scene::Scene& scene, const fs::path& scenePath, std::string& error,
        std::vector<TerrainPropDocument>* documents, bool createCollision, bool useCoarse, std::vector<TerrainPropRangePart>* rangeParts)
    {
        std::vector<TerrainPropRangePart> stagedParts;
        std::vector<TerrainPropDocument> collected;
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
            TerrainPropProjection projection(layer, createCollision);
            if (rangeParts && useCoarse && !createCollision) projection.CollectRangeParts(&stagedParts);
            projection.SetLayerIdentity(doc.layerId);
            for (const auto& p : doc.profiles)
            {
                const bool coarse = useCoarse && !p.coarseModelAssetId.empty() && p.coarseModelAssetId != p.modelAssetId;
                auto model = ReusableAssetService().PrepareModelAssetPlacement({root.generic_u8string(), project, coarse ? p.coarseModelAssetId : p.modelAssetId});
                if (!model.IsReady()) { error = model.Result().error; return false; }
                if (!projection.Register(p.id, *model.PeekScene(), p.collision, error, p.drawDistance)) return false;
                if (coarse) projection.SetRepresentation(p.id, TerrainPropProjection::Representation::Coarse);
            }
            for (const auto& page : doc.pages)
                if (!projection.Place(page, error)) return false;
            if (!staged) staged = std::make_unique<wi::scene::Scene>();
            staged->Merge(layer);
            collected.push_back(std::move(doc));
        }
        if (staged) scene.Merge(*staged);
        if (documents) *documents = std::move(collected);
        if (rangeParts) *rangeParts = std::move(stagedParts);
        return true;
    }
}
