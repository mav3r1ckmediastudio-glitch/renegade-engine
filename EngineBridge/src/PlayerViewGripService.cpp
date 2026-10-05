#include "renegade/bridge/PlayerViewGripService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;
        constexpr const char* OwnedGripKey = "renegade.player.view_grip.owned";
        const std::array<const char*, 3> RoleKeys = {"primary", "off_hand", "support"};

        std::string BonePath(const wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            std::vector<std::string> names;
            std::set<wi::ecs::Entity> visited;
            while (entity != wi::ecs::INVALID_ENTITY && visited.insert(entity).second)
            {
                const auto* name = scene.names.GetComponent(entity);
                if (name == nullptr || name->name.empty()) return {};
                if (name->name != "__renegade_creator_authored_transform")
                    names.push_back(name->name);
                const auto* parent = scene.hierarchy.GetComponent(entity);
                entity = parent == nullptr ? wi::ecs::INVALID_ENTITY : parent->parentID;
            }
            if (entity != wi::ecs::INVALID_ENTITY) return {};
            std::reverse(names.begin(), names.end());
            return nlohmann::json(names).dump();
        }

        XMMATRIX NativeWorld(const wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            XMMATRIX result = XMMatrixIdentity();
            std::set<wi::ecs::Entity> visited;
            while (entity != wi::ecs::INVALID_ENTITY && visited.insert(entity).second)
            {
                const auto* t = scene.transforms.GetComponent(entity);
                if (t == nullptr) break;
                result = result * XMMatrixScaling(t->scale_local.x, t->scale_local.y, t->scale_local.z) *
                    XMMatrixRotationQuaternion(XMLoadFloat4(&t->rotation_local)) *
                    XMMatrixTranslation(t->translation_local.x, t->translation_local.y, t->translation_local.z);
                const auto* p = scene.hierarchy.GetComponent(entity);
                entity = p == nullptr ? wi::ecs::INVALID_ENTITY : p->parentID;
            }
            return result;
        }
        bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input) return false;
            bytes.assign(std::istreambuf_iterator<char>(input), {});
            return !input.bad();
        }
        std::string Hash(const std::vector<std::uint8_t>& bytes)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (auto byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
            std::ostringstream out;
            out << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
            return out.str();
        }
        bool Within(const fs::path& path, const fs::path& root)
        {
            auto p = path.begin();
            for (auto r = root.begin(); r != root.end(); ++r, ++p)
                if (p == path.end() || *p != *r) return false;
            return true;
        }
        ProjectDocumentWrite ExactWrite(const fs::path& path, const std::vector<std::uint8_t>& bytes)
        {
            ProjectDocumentWrite write;
            write.destinationPath = path.generic_u8string();
            write.content = bytes;
            write.validator = [bytes](const std::string& staged, std::string& error) {
                std::vector<std::uint8_t> actual;
                if (!ReadBytes(fs::u8path(staged), actual) || actual != bytes)
                { error = "Hand grip transaction staging did not preserve exact bytes."; return false; }
                return true;
            };
            return write;
        }
        std::vector<std::uint8_t> Bytes(const std::string& text)
        { return {text.begin(), text.end()}; }
    }

    bool CollectPlayerViewBones(const wi::scene::Scene& scene,
        std::vector<PlayerViewBoneChoice>& bones, std::string& error)
    {
        bones.clear(); error.clear();
        std::set<wi::ecs::Entity> entities;
        std::set<std::string> paths;
        for (std::size_t a = 0; a < scene.armatures.GetCount(); ++a)
            for (const auto entity : scene.armatures[a].boneCollection)
            {
                if (!entities.insert(entity).second) continue;
                if (!scene.transforms.Contains(entity))
                { error = "The skeleton contains a missing bone transform."; return false; }
                const auto path = BonePath(scene, entity);
                if (path.empty() || !paths.insert(path).second)
                { error = "Bone hierarchy paths are unnamed or ambiguous; hand bindings cannot be guessed."; return false; }
                const auto names = nlohmann::json::parse(path);
                std::string label;
                for (const auto& name : names)
                { if (!label.empty()) label += " > "; label += name.get<std::string>(); }
                bones.push_back({path, label, entity});
            }
        std::sort(bones.begin(), bones.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
        return true;
    }

    bool ValidatePlayerViewGrips(const PlayerViewGripSettings& settings,
        const std::vector<PlayerViewBoneChoice>* bones, std::string& error)
    {
        error.clear();
        const auto finite = [](const XMFLOAT3& v, float limit) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
                std::abs(v.x) <= limit && std::abs(v.y) <= limit && std::abs(v.z) <= limit;
        };
        for (const auto& binding : settings)
        {
            if (!finite(binding.position, 2) || !finite(binding.rotationDegrees, 180))
            { error = "Grip positions must be within +/-2m and rotations within +/-180 degrees."; return false; }
            if (binding.bonePath.empty()) continue;
            const auto names = nlohmann::json::parse(binding.bonePath, nullptr, false);
            if (binding.bonePath.size() > 8192 || !names.is_array() || names.empty() ||
                names.dump() != binding.bonePath ||
                std::any_of(names.begin(), names.end(), [](const auto& name) { return !name.is_string() || name.get<std::string>().empty(); }))
            { error = "Hand grip bone path is malformed."; return false; }
            if (bones != nullptr && std::none_of(bones->begin(), bones->end(),
                [&binding](const auto& bone) { return bone.path == binding.bonePath; }))
            { error = "The chosen bone no longer exists in this skeleton."; return false; }
        }
        return true;
    }

    bool CapturePlayerViewGrips(const wi::scene::Scene& scene,
        PlayerViewGripSettings& settings, std::string& error)
    {
        settings = {};
        std::vector<PlayerViewBoneChoice> bones;
        if (!CollectPlayerViewBones(scene, bones, error)) return false;
        std::array<bool, 3> found = {};
        for (std::size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto anchor = scene.metadatas.GetEntity(i);
            const auto& meta = scene.metadatas[i];
            for (std::size_t role = 0; role < 3; ++role)
            {
                const auto key = PlayerViewSocketMetadataKeys[role];
                if (!meta.bool_values.has(key) || !meta.bool_values.get(key)) continue;
                if (found[role]) { error = "Several anchors claim one hand role."; return false; }
                const PlayerViewBoneChoice* selected = nullptr;
                auto cursor = anchor;
                std::set<wi::ecs::Entity> visited;
                while (cursor != wi::ecs::INVALID_ENTITY && visited.insert(cursor).second)
                {
                    const auto bone = std::find_if(bones.begin(), bones.end(),
                        [cursor](const auto& b) { return b.entity == cursor; });
                    if (bone != bones.end()) { selected = &*bone; break; }
                    const auto* parent = scene.hierarchy.GetComponent(cursor);
                    cursor = parent == nullptr ? wi::ecs::INVALID_ENTITY : parent->parentID;
                }
                if (selected == nullptr || !scene.transforms.Contains(anchor))
                { error = "A hand anchor is outside the asset skeleton."; return false; }
                const XMMATRIX relative = NativeWorld(scene, anchor) *
                    XMMatrixInverse(nullptr, NativeWorld(scene, selected->entity));
                XMVECTOR scale, rotation, position;
                if (!XMMatrixDecompose(&scale, &rotation, &position, relative))
                { error = "The authored grip transform cannot be decomposed."; return false; }
                XMFLOAT3 s;
                XMStoreFloat3(&s, scale);
                if (std::abs(s.x-1) > 0.001f || std::abs(s.y-1) > 0.001f || std::abs(s.z-1) > 0.001f)
                { error = "Grip anchor scale must be one; bone scale stays on the skeleton."; return false; }
                auto& binding = settings[role];
                binding.bonePath = selected->path;
                XMStoreFloat3(&binding.position, position);
                // Inverse of DirectX's roll -> pitch -> yaw convention used by
                // XMQuaternionRotationRollPitchYaw, including the gimbal limit.
                XMFLOAT4X4 matrix;
                XMStoreFloat4x4(&matrix, XMMatrixRotationQuaternion(rotation));
                const float pitch = std::asin(std::clamp(-matrix._32, -1.0f, 1.0f));
                const bool gimbal = std::abs(std::cos(pitch)) < 0.00001f;
                binding.rotationDegrees = XMFLOAT3(XMConvertToDegrees(pitch),
                    gimbal ? 0.0f : XMConvertToDegrees(std::atan2(matrix._31, matrix._33)),
                    XMConvertToDegrees(gimbal ? std::atan2(-matrix._21, matrix._11) :
                        std::atan2(matrix._12, matrix._22)));
                found[role] = true;
            }
        }
        return ValidatePlayerViewGrips(settings, &bones, error);
    }

    bool ApplyPlayerViewGrips(wi::scene::Scene& scene,
        const PlayerViewGripSettings& settings, std::string& error)
    {
        std::vector<PlayerViewBoneChoice> bones;
        if (!CollectPlayerViewBones(scene, bones, error) ||
            !ValidatePlayerViewGrips(settings, &bones, error)) return false;
        std::vector<wi::ecs::Entity> owned;
        for (std::size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            auto& meta = scene.metadatas[i];
            if (meta.bool_values.has(OwnedGripKey) && meta.bool_values.get(OwnedGripKey))
            {
                const auto entity = scene.metadatas.GetEntity(i);
                const bool isBone = std::any_of(bones.begin(), bones.end(),
                    [entity](const auto& bone) { return bone.entity == entity; });
                bool hasChildren = false;
                for (std::size_t child = 0; child < scene.hierarchy.GetCount(); ++child)
                    hasChildren |= scene.hierarchy[child].parentID == entity;
                if (isBone || hasChildren || !scene.transforms.Contains(entity))
                { error = "An owned grip anchor is not an isolated transform."; return false; }
                owned.push_back(entity);
            }
        }
        for (std::size_t i = 0; i < scene.metadatas.GetCount(); ++i)
            for (const auto key : PlayerViewSocketMetadataKeys) scene.metadatas[i].bool_values.erase(key);
        for (const auto entity : owned) scene.Entity_Remove(entity);
        for (std::size_t role = 0; role < 3; ++role)
        {
            const auto& binding = settings[role];
            if (binding.bonePath.empty()) continue;
            const auto bone = std::find_if(bones.begin(), bones.end(),
                [&binding](const auto& b) { return b.path == binding.bonePath; });
            const auto grip = scene.Entity_CreateTransform(std::string("__renegade_player_grip_") + RoleKeys[role]);
            auto* transform = scene.transforms.GetComponent(grip);
            transform->translation_local = binding.position;
            XMStoreFloat4(&transform->rotation_local, XMQuaternionRotationRollPitchYaw(
                XMConvertToRadians(binding.rotationDegrees.x), XMConvertToRadians(binding.rotationDegrees.y),
                XMConvertToRadians(binding.rotationDegrees.z)));
            transform->SetDirty(); transform->UpdateTransform();
            scene.Component_Attach(grip, bone->entity, true);
            auto& meta = scene.metadatas.Create(grip);
            meta.bool_values.set(OwnedGripKey, true);
            meta.bool_values.set(PlayerViewSocketMetadataKeys[role], true);
        }
        return true;
    }

    bool SerializePlayerViewGripOptions(const PlayerViewGripSettings& settings,
        std::string& json, std::string& error)
    {
        if (!ValidatePlayerViewGrips(settings, nullptr, error)) return false;
        nlohmann::json result = {{"schema_version", 1}};
        for (std::size_t role = 0; role < 3; ++role)
        {
            const auto& b = settings[role];
            result[RoleKeys[role]] = {{"bone_path",b.bonePath},
                {"position",{b.position.x,b.position.y,b.position.z}},
                {"rotation_degrees",{b.rotationDegrees.x,b.rotationDegrees.y,b.rotationDegrees.z}}};
        }
        json = result.dump(); return true;
    }

    bool ParsePlayerViewGripOptions(const std::string& json,
        PlayerViewGripSettings& settings, std::string& error)
    {
        settings = {};
        try {
            const auto root = nlohmann::json::parse(json);
            if (!root.is_object() || root.at("schema_version") != 1) throw std::runtime_error("schema");
            for (std::size_t role = 0; role < 3; ++role)
            {
                const auto& value = root.at(RoleKeys[role]);
                auto& b = settings[role];
                b.bonePath = value.at("bone_path").get<std::string>();
                const auto& p = value.at("position"); const auto& r = value.at("rotation_degrees");
                if (!p.is_array() || p.size() != 3 || !r.is_array() || r.size() != 3)
                    throw std::runtime_error("vector");
                b.position = XMFLOAT3(p[0].get<float>(),p[1].get<float>(),p[2].get<float>());
                b.rotationDegrees = XMFLOAT3(r[0].get<float>(),r[1].get<float>(),r[2].get<float>());
            }
        } catch (const std::exception&) { error = "Hand grip recipe is malformed or has an unsupported schema."; return false; }
        return ValidatePlayerViewGrips(settings, nullptr, error);
    }

    struct PlayerViewGripSession::Impl
    {
        fs::path root, asset, projection;
        std::string projectId, assetId, relative;
        ReusableModelAssetDocument document;
        std::vector<std::uint8_t> originalBytes, originalProjection;
        wi::scene::Scene scene;
        std::vector<PlayerViewBoneChoice> bones;
        PlayerViewGripSettings settings;
        CommandService commands;
    };

    namespace
    {
        class GripCommand final : public ICommand
        {
        public:
            GripCommand(PlayerViewGripSettings& state, const PlayerViewGripSettings& after)
                : state_(state), before_(state), after_(after) {}
            bool Execute() override { state_ = after_; return true; }
            void Undo() override { state_ = before_; }
        private:
            PlayerViewGripSettings& state_;
            PlayerViewGripSettings before_, after_;
        };
    }

    PlayerViewGripSession::PlayerViewGripSession() = default;
    PlayerViewGripSession::~PlayerViewGripSession() = default;
    bool PlayerViewGripSession::Open(const std::string& projectRoot, const std::string& projectId,
        const std::string& assetId, std::string& error)
    {
        error.clear();
        auto candidate = std::make_unique<Impl>();
        std::error_code ec;
        candidate->root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
        AssetRegistry registry;
        if (ec || !IsValidStableId(assetId) || !ReadAssetRegistry(projectRoot, projectId, registry, error))
        { if (error.empty()) error = "The arms project or asset identity is unavailable."; return false; }
        const auto record = std::find_if(registry.records.begin(), registry.records.end(),
            [&assetId](const auto& r) { return r.assetId == assetId; });
        if (record == registry.records.end() || !record->sourceAvailable ||
            fs::u8path(record->projectRelativePath).extension() != ".rasset")
        { error = "Choose an available governed model asset before editing hand grips."; return false; }
        candidate->relative = record->projectRelativePath;
        candidate->asset = fs::weakly_canonical(candidate->root / fs::u8path(candidate->relative), ec);
        if (ec || !Within(candidate->asset, candidate->root))
        { error = "The arms product resolves outside its project."; return false; }
        candidate->projection = fs::weakly_canonical(candidate->root / fs::u8path(
            ResolveReusableModelManagedProjectionPath(candidate->relative)), ec);
        if (ec || !Within(candidate->projection, candidate->root) ||
            !ReadBytes(candidate->asset, candidate->originalBytes) ||
            !ReadBytes(candidate->projection, candidate->originalProjection) ||
            !DeserializeReusableModelAssetDocument(candidate->originalBytes, candidate->document, error))
        { if (error.empty()) error = "The governed model product or metadata cannot be read."; return false; }
        const auto& manifest = candidate->document.manifest;
        if (manifest.projectId != projectId || manifest.assetId != assetId ||
            std::none_of(registry.importedProducts.begin(), registry.importedProducts.end(),
                [&manifest](const auto& p) { return p.productAssetId == manifest.assetId && p.sourceAssetId == manifest.sourceAssetId; }))
        { error = "The model product does not match its governed identity and source association."; return false; }
        try {
            const auto meta = nlohmann::json::parse(candidate->originalProjection);
            if (meta.at("project_id") != projectId || meta.at("asset_id") != assetId ||
                meta.at("payload_hash") != manifest.payloadHash ||
                meta.at("settings") != nlohmann::json::parse(manifest.settingsJson))
            { error = "The model metadata is stale or belongs to another asset."; return false; }
        } catch (const std::exception&) { error = "The model metadata is malformed."; return false; }
        wi::Archive payload(candidate->document.payload.data(), candidate->document.payload.size());
        candidate->scene.Serialize(payload);
        if (!CollectPlayerViewBones(candidate->scene, candidate->bones, error) ||
            candidate->bones.empty() || !CapturePlayerViewGrips(candidate->scene, candidate->settings, error))
        { if (error.empty()) error = "This asset has no native skeleton to bind hands to."; return false; }
        candidate->projectId = projectId;
        candidate->assetId = assetId;
        candidate->commands.MarkSaved();
        impl_ = std::move(candidate);
        return true;
    }
    const std::vector<PlayerViewBoneChoice>& PlayerViewGripSession::Bones() const { return impl_->bones; }
    const PlayerViewGripSettings& PlayerViewGripSession::Settings() const { return impl_->settings; }
    const std::string& PlayerViewGripSession::AssetPath() const { return impl_->relative; }
    bool PlayerViewGripSession::CanUndo() const { return impl_ && impl_->commands.CanUndo(); }
    bool PlayerViewGripSession::CanRedo() const { return impl_ && impl_->commands.CanRedo(); }
    bool PlayerViewGripSession::IsDirty() const { return impl_ && impl_->commands.IsDirty(); }
    bool PlayerViewGripSession::Undo() { return impl_ && impl_->commands.Undo(); }
    bool PlayerViewGripSession::Redo() { return impl_ && impl_->commands.Redo(); }
    bool PlayerViewGripSession::SetBinding(std::size_t role, const PlayerViewGripBinding& binding, std::string& error)
    {
        error.clear();
        if (!impl_ || role >= 3) { error = "Hand grip editor is not open."; return false; }
        auto next = impl_->settings;
        next[role] = binding;
        if (!ValidatePlayerViewGrips(next, &impl_->bones, error)) return false;
        std::string beforeJson, afterJson;
        if (!SerializePlayerViewGripOptions(impl_->settings, beforeJson, error) ||
            !SerializePlayerViewGripOptions(next, afterJson, error)) return false;
        if (beforeJson == afterJson) return true;
        return impl_->commands.Execute(std::make_unique<GripCommand>(impl_->settings, next));
    }
    bool PlayerViewGripSession::Save(std::string& error, ProjectDocumentTransactionHook hook)
    {
        error.clear();
        if (!impl_) { error = "Hand grip editor is not open."; return false; }
        if (!IsDirty()) return true;
        auto& state = *impl_;
        std::vector<std::uint8_t> current, projectionBytes;
        if (!ReadBytes(state.asset, current) || current != state.originalBytes ||
            !ReadBytes(state.projection, projectionBytes) || projectionBytes != state.originalProjection)
        { error = "The arms asset changed outside this editor. Close and reopen before saving."; return false; }

        AssetRegistry registry;
        if (!ReadAssetRegistry(state.root.generic_u8string(), state.projectId, registry, error)) return false;
        auto record = std::find_if(registry.records.begin(), registry.records.end(),
            [&state](const auto& r) { return r.assetId == state.assetId; });
        auto product = std::find_if(registry.importedProducts.begin(), registry.importedProducts.end(),
            [&state](const auto& p) { return p.productAssetId == state.assetId; });
        if (record == registry.records.end() || record->projectRelativePath != state.relative ||
            record->contentHash != Hash(current) || product == registry.importedProducts.end() ||
            product->sourceAssetId != state.document.manifest.sourceAssetId)
        { error = "The governed arms registry changed. Reopen this editor before saving."; return false; }

        if (!ApplyPlayerViewGrips(state.scene, state.settings, error)) return false;
        wi::Archive archive;
        state.scene.Serialize(archive);
        archive.WriteData(state.document.payload);
        // Native save/reopen verifies the resulting grip metadata before disk replacement.
        wi::scene::Scene reopened;
        wi::Archive read(state.document.payload.data(), state.document.payload.size());
        reopened.Serialize(read);
        PlayerViewGripSettings verified;
        if (!CapturePlayerViewGrips(reopened, verified, error)) return false;
        for (std::size_t role = 0; role < 3; ++role)
        {
            const auto& wanted = state.settings[role]; const auto& actual = verified[role];
            if (wanted.bonePath != actual.bonePath) { error = "Saved hand anchor lost its bone identity."; return false; }
            if (wanted.bonePath.empty()) continue;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float p = axis == 0 ? wanted.position.x-actual.position.x : axis == 1 ? wanted.position.y-actual.position.y : wanted.position.z-actual.position.z;
                if (std::abs(p) > 0.0001f)
                { error = "Saved hand grip did not preserve its authored position."; return false; }
            }
            const auto quaternion = [](const XMFLOAT3& euler)
            { return XMQuaternionRotationRollPitchYaw(XMConvertToRadians(euler.x),
                XMConvertToRadians(euler.y), XMConvertToRadians(euler.z)); };
            const float dot = std::abs(XMVectorGetX(XMQuaternionDot(
                quaternion(wanted.rotationDegrees), quaternion(actual.rotationDegrees))));
            if (dot < 0.999999f)
            { error = "Saved hand grip did not preserve its authored rotation."; return false; }
        }

        std::string gripJson;
        if (!SerializePlayerViewGripOptions(state.settings, gripJson, error)) return false;
        auto recipe = nlohmann::json::parse(state.document.manifest.settingsJson);
        recipe["options"]["hand_grips"] = nlohmann::json::parse(gripJson);
        state.document.manifest.settingsJson = recipe.dump();
        state.document.manifest.payloadHash = Hash(state.document.payload);
        std::vector<std::uint8_t> output;
        if (!SerializeReusableModelAssetDocument(state.document, output, error)) return false;
        auto projection = nlohmann::json::parse(state.originalProjection);
        projection["payload_hash"] = state.document.manifest.payloadHash;
        projection["settings"] = recipe;
        const auto projectionOutput = Bytes(projection.dump());
        record->contentHash = Hash(output);
        product->settingsJson = state.document.manifest.settingsJson;
        product->productContentHashAtImport = record->contentHash;
        const auto projectionRelative = state.projection.lexically_relative(state.root).generic_u8string();
        for (auto& r : registry.records)
            if (r.projectRelativePath == projectionRelative) r.contentHash = Hash(projectionOutput);
        std::string registryJson, registryPath;
        if (!SerializeAssetRegistry(registry, registryJson, error) ||
            !ResolveAssetRegistryDocumentPath(state.root.generic_u8string(), registryPath, error)) return false;
        ProjectDocumentTransactionOptions options;
        options.allowedRoot = state.root.generic_u8string();
        options.journalDirectory = (state.root / "Intermediate/Transactions").generic_u8string();
        options.operationHook = std::move(hook);
        const auto result = ProjectDocumentTransaction().Execute({
            ExactWrite(state.asset, output), ExactWrite(state.projection, projectionOutput),
            ExactWrite(fs::u8path(registryPath), Bytes(registryJson)),
        }, options);
        if (!result.success) { error = result.message; return false; }
        state.originalBytes = std::move(output);
        state.originalProjection = projectionOutput;
        state.commands.MarkSaved();
        return true;
    }
}
