#include "RuntimePlayerViewAsset.h"
#include "RuntimePlayerViewAnimation.h"
#include "RuntimePlayerViewRig.h"
#include "PlayerViewSocketFixture.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* P1ProjectId =
        "12121212-1212-4212-8212-121212121212";
    constexpr const char* P1ArmsAssetId =
        "34343434-3434-4434-8434-343434343434";
    constexpr const char* P1SourceAssetId =
        "56565656-5656-4656-8656-565656565656";
    constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    bool Near(
        const float left,
        const float right,
        const float epsilon = 0.002f)
    {
        return std::abs(left - right) <= epsilon;
    }

    bool Near3(
        const XMFLOAT3& left,
        const XMFLOAT3& right,
        const float epsilon = 0.002f)
    {
        return Near(left.x, right.x, epsilon) &&
            Near(left.y, right.y, epsilon) &&
            Near(left.z, right.z, epsilon);
    }

    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "PLAYER VIEW RIG FAIL // " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = FnvOffset;
        for (const std::uint8_t value : bytes)
        {
            hash ^= value;
            hash *= FnvPrime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    bool WriteBytes(
        const fs::path& path,
        const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        if (!bytes.empty())
        {
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(output);
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    wi::ecs::Entity AddPlayerViewAnimationClip(
        wi::scene::Scene& scene,
        const wi::ecs::Entity target,
        const std::string& name,
        const std::string& action)
    {
        const wi::ecs::Entity entity =
            scene.Entity_CreateTransform(name);
        auto& animation = scene.animations.Create(entity);
        animation.start = 0.0f;
        animation.end = 1.0f;
        wi::scene::AnimationComponent::AnimationChannel channel;
        channel.target = target;
        channel.path =
            wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;
        animation.channels.push_back(channel);
        auto& metadata = scene.metadatas.Create(entity);
        metadata.string_values.set(
            renegade::bridge::CreatorCharacterAnimationActionMetadataKey,
            action);
        return entity;
    }

    std::vector<std::uint8_t> SerializeGovernedViewAssetPayload()
    {
        using namespace renegade::bridge;

        wi::scene::Scene assetScene;
        renegade::tests::CreatePlayerViewSocketAsset(assetScene);
        const auto root =
            assetScene.Entity_CreateTransform("P1 Governed Arms Root");
        const auto mesh =
            assetScene.Entity_CreateTransform("P1 Governed Arms Mesh");
        assetScene.objects.Create(mesh);
        assetScene.Component_Attach(mesh, root, true);

        // Deliberately contaminate the reusable product with gameplay-facing
        // components. The P1 Runtime loader must remove all of them rather
        // than allowing a view model to become a second Player/Character.
        assetScene.characters.Create(root);
        assetScene.rigidbodies.Create(root);
        assetScene.colliders.Create(root);
        auto& metadata = assetScene.metadatas.Create(root);
        metadata.bool_values.set(CharacterAssetTemplateMetadataKey, true);
        metadata.int_values.set(CharacterAssetTemplateVersionMetadataKey, 1);
        metadata.string_values.set(
            PersistentEntityIdMetadataKey,
            "78787878-7878-4878-8878-787878787878");

        wi::Archive archive;
        assetScene.Serialize(archive);
        std::vector<std::uint8_t> bytes;
        archive.WriteData(bytes);
        return bytes;
    }

    fs::path WritePackagedViewAssetFixture()
    {
        using namespace renegade::bridge;

        const fs::path packageRoot =
            fs::temp_directory_path() / "renegade-p1-player-view-asset";
        const fs::path productPath =
            packageRoot / "GameData" / "Content" / "Models" / "p1-arms.rasset";
        const fs::path manifestPath =
            packageRoot / "GameData" / "content-manifest.json";

        std::error_code ec;
        fs::remove_all(packageRoot, ec);
        ec.clear();
        fs::create_directories(productPath.parent_path(), ec);
        if (ec)
            Fail("could not create governed view-asset package fixture");

        ReusableModelAssetDocument asset;
        asset.manifest.projectId = P1ProjectId;
        asset.manifest.assetId = P1ArmsAssetId;
        asset.manifest.sourceAssetId = P1SourceAssetId;
        asset.manifest.sourceFormat = "fbx";
        asset.manifest.importer = "wicked.ufbx";
        asset.manifest.importerVersion = 1;
        asset.manifest.settingsSchema = ReusableModelImportSettingsSchema;
        asset.manifest.settingsVersion = 1;
        asset.manifest.settingsJson =
            "{\"options\":{},\"source_format\":\"fbx\"}";
        asset.payload = SerializeGovernedViewAssetPayload();
        asset.manifest.payloadHash = HashBytes(asset.payload);

        std::string error;
        std::vector<std::uint8_t> productBytes;
        if (asset.payload.empty() ||
            !SerializeReusableModelAssetDocument(asset, productBytes, error) ||
            !WriteBytes(productPath, productBytes))
        {
            Fail("could not write governed view-asset product: " + error);
        }

        const std::string manifestJson =
            std::string("{\"files\":[{\"asset_id\":\"") +
            P1ArmsAssetId +
            "\",\"path\":\"GameData/Content/Models/p1-arms.rasset\","
            "\"source_hash\":\"" + HashBytes(productBytes) +
            "\"}],\"format\":\"renegade-content-manifest\","
            "\"project_id\":\"" + P1ProjectId +
            "\",\"schema_version\":1}";
        if (!WriteText(manifestPath, manifestJson))
            Fail("could not write governed view-asset content manifest");

        return packageRoot;
    }
}

#include "PlayerViewGripAuthoringTests.h"

int main()
{
    TestPlayerViewGripAuthoring();
    using namespace renegade::runtime;

    wi::scene::Scene scene;

    const auto player =
        scene.Entity_CreateTransform("__p1_authoritative_player");
    auto* playerTransform = scene.transforms.GetComponent(player);
    if (playerTransform == nullptr)
        Fail("could not create authoritative Player transform");

    playerTransform->translation_local = XMFLOAT3(8.0f, 0.75f, -3.0f);
    XMStoreFloat4(
        &playerTransform->rotation_local,
        XMQuaternionRotationRollPitchYaw(
            wi::math::DegreesToRadians(9.0f),
            wi::math::DegreesToRadians(35.0f),
            wi::math::DegreesToRadians(-6.0f)));
    playerTransform->SetDirty();
    playerTransform->UpdateTransform();

    RuntimePlayerViewRigState rig;
    RuntimePlayerViewRigSettings settings;
    settings.createProofGeometry = false;

    constexpr float eyeHeight = 1.72f;
    std::string error;
    if (!SpawnRuntimePlayerViewRig(
            scene, rig, player, eyeHeight, error, settings))
    {
        Fail("socket-only View Rig spawn failed: " + error);
    }

    if (!rig.IsSpawned() ||
        rig.playerEntity != player ||
        rig.presentationRoot == wi::ecs::INVALID_ENTITY ||
        rig.primaryHandSocket == wi::ecs::INVALID_ENTITY ||
        rig.offHandSocket == wi::ecs::INVALID_ENTITY ||
        rig.twoHandSupportSocket == wi::ecs::INVALID_ENTITY)
    {
        Fail("View Rig did not create all first-class hand sockets");
    }
    const auto* rootHierarchy = scene.hierarchy.GetComponent(rig.root);
    const auto* presentationHierarchy =
        scene.hierarchy.GetComponent(rig.presentationRoot);
    const auto* primaryHierarchy =
        scene.hierarchy.GetComponent(rig.primaryHandSocket);
    const auto* offHierarchy =
        scene.hierarchy.GetComponent(rig.offHandSocket);
    const auto* supportHierarchy =
        scene.hierarchy.GetComponent(rig.twoHandSupportSocket);
    if (rootHierarchy == nullptr ||
        rootHierarchy->parentID != player ||
        presentationHierarchy == nullptr ||
        presentationHierarchy->parentID != rig.root ||
        primaryHierarchy == nullptr ||
        offHierarchy == nullptr ||
        supportHierarchy == nullptr ||
        primaryHierarchy->parentID != rig.presentationRoot ||
        offHierarchy->parentID != rig.presentationRoot ||
        supportHierarchy->parentID != rig.presentationRoot)
    {
        Fail("View Rig is not parented through the authoritative Player");
    }

    const auto* primary =
        scene.transforms.GetComponent(rig.primaryHandSocket);
    const auto* off =
        scene.transforms.GetComponent(rig.offHandSocket);
    if (primary == nullptr || off == nullptr ||
        !Near(primary->translation_local.x, settings.primaryHandOffset.x) ||
        !Near(off->translation_local.x, settings.offHandOffset.x) ||
        !(primary->translation_local.x > 0.0f) ||
        !(off->translation_local.x < 0.0f))
    {
        Fail("primary/off-hand socket offsets are not independent");
    }

    // Prove the generic policy separately from GPU primitive creation.
    const auto modelRoot = scene.Entity_CreateTransform("View Model Root");
    const auto modelChild = scene.Entity_CreateTransform("View Model Child");
    scene.objects.Create(modelRoot);
    scene.objects.Create(modelChild);
    scene.Component_Attach(modelChild, modelRoot, true);

    const std::size_t configured =
        ConfigureRuntimeViewModelHierarchy(scene, modelRoot);
    const auto* rootObject = scene.objects.GetComponent(modelRoot);
    const auto* childObject = scene.objects.GetComponent(modelChild);
    if (configured != 2 ||
        rootObject == nullptr ||
        childObject == nullptr ||
        !rootObject->IsForeground() ||
        !childObject->IsForeground() ||
        rootObject->IsCastingShadow() ||
        childObject->IsCastingShadow() ||
        !rootObject->IsNotVisibleInReflections() ||
        !childObject->IsNotVisibleInReflections() ||
        rootObject->IsNotVisibleInMainCamera() ||
        childObject->IsNotVisibleInMainCamera())
    {
        Fail("Wicked foreground/reflection/shadow view-model policy is wrong");
    }
    const auto importedViewModel =
        scene.Entity_CreateTransform("Imported First Person Arms");
    const auto importedMesh =
        scene.Entity_CreateTransform("Imported First Person Arms Mesh");
    scene.objects.Create(importedMesh);
    scene.Component_Attach(importedMesh, importedViewModel, true);
    if (!AttachRuntimeViewModelHierarchy(
            scene,
            rig,
            importedViewModel,
            PlayerViewRigSocket::PresentationRoot,
            error))
    {
        Fail("real view-model hierarchy attachment seam failed: " + error);
    }
    const auto* importedHierarchy =
        scene.hierarchy.GetComponent(importedViewModel);
    const auto* importedObject = scene.objects.GetComponent(importedMesh);
    if (importedHierarchy == nullptr ||
        importedHierarchy->parentID != rig.presentationRoot ||
        importedObject == nullptr ||
        !importedObject->IsForeground() ||
        importedObject->IsCastingShadow() ||
        !importedObject->IsNotVisibleInReflections())
    {
        Fail("attached view-model hierarchy did not inherit P1 foreground policy");
    }

    // P1 movement presentation owns semantic native animation requests. It
    // must use Wicked's AnimationComponent on the loaded view model rather
    // than inventing a second skeleton/animation runtime.
    rig.viewModelRoot = importedViewModel;
    const auto viewIdle = AddPlayerViewAnimationClip(
        scene, importedMesh, "Arms_Idle_Breathe", "Idle");
    const auto viewWalk = AddPlayerViewAnimationClip(
        scene, importedMesh, "Arms_Walk", "Walk");
    const auto viewSprint = AddPlayerViewAnimationClip(
        scene, importedMesh, "Arms_Sprint", "Run");

    PlayerViewAction inferredAction = PlayerViewAction::Idle;
    if (!ResolvePlayerViewAnimationAction(
            "weapon sprint forward", inferredAction) ||
        inferredAction != PlayerViewAction::Sprint)
    {
        Fail("Player View native animation semantic inference is wrong");
    }

    RuntimePlayerViewAnimationState viewAnimation;
    if (!InitializeRuntimePlayerViewAnimations(
            scene, rig, viewAnimation, error))
    {
        Fail("Player View native animation setup failed: " + error);
    }
    if (viewAnimation.clips[PlayerViewActionIndex(PlayerViewAction::Idle)].size() != 1 ||
        viewAnimation.clips[PlayerViewActionIndex(PlayerViewAction::Walk)].size() != 1 ||
        viewAnimation.clips[PlayerViewActionIndex(PlayerViewAction::Sprint)].size() != 1 ||
        !RequestRuntimePlayerViewAnimation(
            scene, viewAnimation, PlayerViewAction::Idle) ||
        viewAnimation.activeClip != viewIdle ||
        !scene.animations.GetComponent(viewIdle)->IsPlaying() ||
        scene.animations.GetComponent(viewIdle)->IsRootMotion())
    {
        Fail("Player View Idle did not bind to native Wicked animation");
    }

    UpdateRuntimePlayerViewAnimations(
        scene, viewAnimation, PlayerViewAction::Walk, 0.10f);
    if (viewAnimation.activeClip != viewWalk ||
        viewAnimation.outgoingClip != viewIdle ||
        !scene.animations.GetComponent(viewWalk)->IsPlaying() ||
        scene.animations.GetComponent(viewWalk)->amount <= 0.0f ||
        scene.animations.GetComponent(viewIdle)->amount <= 0.0f)
    {
        Fail("Player View Walk did not crossfade native Wicked animation");
    }
    UpdateRuntimePlayerViewAnimations(
        scene, viewAnimation, PlayerViewAction::Walk, 0.11f);
    if (viewAnimation.outgoingClip != wi::ecs::INVALID_ENTITY ||
        scene.animations.GetComponent(viewIdle)->IsPlaying() ||
        !Near(scene.animations.GetComponent(viewWalk)->amount, 1.0f))
    {
        Fail("Player View Walk crossfade did not settle cleanly");
    }

    UpdateRuntimePlayerViewAnimations(
        scene, viewAnimation, PlayerViewAction::Sprint, 0.01f);
    if (viewAnimation.activeClip != viewSprint ||
        viewAnimation.activeAction != PlayerViewAction::Sprint ||
        !scene.animations.GetComponent(viewSprint)->IsPlaying())
    {
        Fail("Player View Sprint did not request its semantic native clip");
    }
    ResetRuntimePlayerViewAnimations(scene, viewAnimation);

    // Prove the actual packaged governed-asset path, not just an in-memory
    // hierarchy attachment. The fixture intentionally carries Character and
    // physics components that must never survive as first-person presentation.
    {
        wi::scene::Scene packagedScene;
        const auto packagedPlayer =
            packagedScene.Entity_CreateTransform("__p1_packaged_player");

        RuntimePlayerViewRigState packagedRig;
        RuntimePlayerViewRigSettings packagedSettings;
        packagedSettings.createProofGeometry = false;
        if (!SpawnRuntimePlayerViewRig(
                packagedScene,
                packagedRig,
                packagedPlayer,
                eyeHeight,
                error,
                packagedSettings))
        {
            Fail("packaged View Rig spawn failed: " + error);
        }

        const auto proofPrimary =
            packagedScene.Entity_CreateTransform("__p1_proxy_primary");
        const auto proofOff =
            packagedScene.Entity_CreateTransform("__p1_proxy_off");
        packagedScene.Component_Attach(
            proofPrimary, packagedRig.presentationRoot, true);
        packagedScene.Component_Attach(
            proofOff, packagedRig.presentationRoot, true);
        packagedRig.primaryArmProof = proofPrimary;
        packagedRig.offHandArmProof = proofOff;

        const fs::path packageRoot = WritePackagedViewAssetFixture();
        if (!LoadPackagedRuntimePlayerViewAsset(
                packagedScene,
                packagedRig,
                packageRoot.generic_u8string(),
                P1ProjectId,
                P1ArmsAssetId,
                error))
        {
            Fail("packaged governed first-person arms load failed: " + error);
        }

        if (packagedRig.viewModelRoot == wi::ecs::INVALID_ENTITY ||
            packagedRig.viewModelAssetId != P1ArmsAssetId)
        {
            Fail("governed first-person arms identity was not retained");
        }
        if (packagedRig.primaryArmProof != wi::ecs::INVALID_ENTITY ||
            packagedRig.offHandArmProof != wi::ecs::INVALID_ENTITY ||
            packagedScene.transforms.Contains(proofPrimary) ||
            packagedScene.transforms.Contains(proofOff))
        {
            Fail("real governed arms did not replace the P1 proxy geometry");
        }

        const auto* viewHierarchy =
            packagedScene.hierarchy.GetComponent(packagedRig.viewModelRoot);
        if (viewHierarchy == nullptr ||
            viewHierarchy->parentID != packagedRig.presentationRoot)
        {
            Fail("governed arms root was not attached to the presentation root");
        }

        const auto governedRoot = packagedScene.Entity_FindByName(
            "P1 Governed Arms Root", packagedRig.viewModelRoot);
        const auto governedMesh = packagedScene.Entity_FindByName(
            "P1 Governed Arms Mesh", packagedRig.viewModelRoot);
        const auto* governedObject =
            packagedScene.objects.GetComponent(governedMesh);
        if (governedRoot == wi::ecs::INVALID_ENTITY ||
            governedMesh == wi::ecs::INVALID_ENTITY ||
            governedObject == nullptr ||
            !governedObject->IsForeground() ||
            governedObject->IsCastingShadow() ||
            !governedObject->IsNotVisibleInReflections())
        {
            Fail("governed arms did not inherit the P1 foreground render policy");
        }

        for (std::size_t index = 0;
             index < packagedScene.transforms.GetCount();
             ++index)
        {
            const auto entity = packagedScene.transforms.GetEntity(index);
            if (entity != packagedRig.viewModelRoot &&
                !packagedScene.Entity_IsDescendant(
                    entity, packagedRig.viewModelRoot))
            {
                continue;
            }
            if (packagedScene.characters.Contains(entity) ||
                packagedScene.rigidbodies.Contains(entity) ||
                packagedScene.colliders.Contains(entity))
            {
                Fail("governed view model retained gameplay/physics components");
            }
        }

        const auto* governedMetadata =
            packagedScene.metadatas.GetComponent(governedRoot);
        if (governedMetadata != nullptr &&
            (governedMetadata->bool_values.has(
                 renegade::bridge::CharacterAssetTemplateMetadataKey) ||
             governedMetadata->int_values.has(
                 renegade::bridge::CharacterAssetTemplateVersionMetadataKey) ||
             governedMetadata->string_values.has(
                 renegade::bridge::PersistentEntityIdMetadataKey)))
        {
            Fail("governed view model retained Character/identity metadata");
        }

        for (const auto target : packagedRig.socketTargets)
            if (target == wi::ecs::INVALID_ENTITY ||
                !packagedScene.Entity_IsDescendant(target, packagedRig.viewModelRoot))
                Fail("packaged governed asset did not bind its remapped skeletal hand anchors");

        const auto loadedViewRoot = packagedRig.viewModelRoot;
        DespawnRuntimePlayerViewRig(packagedScene, packagedRig);
        if (packagedScene.transforms.Contains(loadedViewRoot) ||
            !packagedScene.transforms.Contains(packagedPlayer))
        {
            Fail("governed view-model cleanup escaped View Rig ownership");
        }

        std::error_code cleanupError;
        fs::remove_all(packageRoot, cleanupError);
    }

    renegade::bridge::PlayerInputFrame presentationInput;
    UpdateRuntimePlayerViewRigPresentation(
        scene, rig, presentationInput, 1.0f / 60.0f);
    if (rig.action != PlayerViewAction::Idle)
        Fail("stationary Player did not resolve Idle view action");

    presentationInput.moveForward = 1.0f;
    UpdateRuntimePlayerViewRigPresentation(
        scene, rig, presentationInput, 1.0f / 60.0f);
    if (rig.action != PlayerViewAction::Walk)
        Fail("moving Player did not resolve Walk view action");

    presentationInput.sprintDown = true;
    UpdateRuntimePlayerViewRigPresentation(
        scene, rig, presentationInput, 1.0f / 60.0f);
    if (rig.action != PlayerViewAction::Sprint)
        Fail("sprinting Player did not resolve Sprint view action");

    const auto* presentation =
        scene.transforms.GetComponent(rig.presentationRoot);
    if (presentation == nullptr ||
        (Near(presentation->translation_local.x, 0.0f) &&
         Near(presentation->translation_local.y, 0.0f)))
    {
        Fail("semantic movement presentation did not affect the View Rig");
    }

    const float desiredYaw = wi::math::DegreesToRadians(65.0f);
    const float desiredPitch = wi::math::DegreesToRadians(-20.0f);
    if (!PoseRuntimePlayerViewRig(
            scene, rig, desiredYaw, desiredPitch))
    {
        Fail("View Rig did not accept Player look state");
    }

    auto* rigRoot = scene.transforms.GetComponent(rig.root);
    const auto* playerCurrent = scene.transforms.GetComponent(player);
    if (rigRoot == nullptr || playerCurrent == nullptr)
        Fail("View Rig or authoritative Player transform disappeared");

    const XMFLOAT3 playerPosition = playerCurrent->GetPosition();
    const XMFLOAT3 expectedEye(
        playerPosition.x,
        playerPosition.y + eyeHeight,
        playerPosition.z);
    if (!Near3(rigRoot->GetPosition(), expectedEye))
        Fail("parented View Rig did not preserve world-space eye height");

    wi::scene::TransformComponent expectedCameraTransform;
    expectedCameraTransform.Translate(expectedEye);
    expectedCameraTransform.RotateRollPitchYaw(
        XMFLOAT3(desiredPitch, desiredYaw, 0.0f));
    expectedCameraTransform.UpdateTransform();

    if (!Near3(
            rigRoot->GetForward(),
            expectedCameraTransform.GetForward()) ||
        !Near3(
            rigRoot->GetRight(),
            expectedCameraTransform.GetRight()) ||
        !Near3(
            rigRoot->GetUp(),
            expectedCameraTransform.GetUp()))
    {
        Fail("View Rig world orientation double-applied the Player Start rotation");
    }

    // Simulate the Jolt parent moving before Wicked hierarchy propagation.
    auto* playerMoved = scene.transforms.GetComponent(player);
    rigRoot = scene.transforms.GetComponent(rig.root);
    if (playerMoved == nullptr || rigRoot == nullptr)
        Fail("View Rig or Player transform disappeared before movement proof");

    playerMoved->translation_local = XMFLOAT3(10.0f, 1.25f, -1.0f);
    playerMoved->SetDirty();
    playerMoved->UpdateTransform();
    rigRoot->UpdateTransform_Parented(*playerMoved);

    const XMFLOAT3 movedEye(10.0f, 1.25f + eyeHeight, -1.0f);
    if (!Near3(rigRoot->GetPosition(), movedEye))
        Fail("View Rig did not inherit authoritative Player movement");

    RuntimePlayerViewRigState duplicateState = rig;
    if (SpawnRuntimePlayerViewRig(
            scene,
            duplicateState,
            player,
            eyeHeight,
            error,
            settings))
    {
        Fail("a second spawn over an active View Rig was not rejected");
    }

    const auto rootEntity = rig.root;
    const auto primaryEntity = rig.primaryHandSocket;
    const auto offEntity = rig.offHandSocket;
    DespawnRuntimePlayerViewRig(scene, rig);
    if (rig.IsSpawned() ||
        scene.transforms.Contains(rootEntity) ||
        scene.transforms.Contains(primaryEntity) ||
        scene.transforms.Contains(offEntity) ||
        !scene.transforms.Contains(player))
    {
        Fail("View Rig despawn damaged Player ownership or left transient entities");
    }

    // Native serialized bone/grip identity, atomic failure and fallback proof.
    {
        wi::scene::Scene prepared;
        renegade::tests::CreatePlayerViewSocketAsset(prepared);
        wi::Archive write;
        prepared.Serialize(write);
        std::vector<std::uint8_t> bytes;
        write.WriteData(bytes);
        wi::scene::Scene reopened;
        wi::Archive read(bytes.data(), bytes.size());
        reopened.Serialize(read);

        wi::scene::Scene boundScene;
        const auto boundPlayer = boundScene.Entity_CreateTransform("Socket test Player");
        RuntimePlayerViewRigState boundRig;
        RuntimePlayerViewRigSettings boundSettings;
        boundSettings.createProofGeometry = false;
        if (!SpawnRuntimePlayerViewRig(boundScene, boundRig, boundPlayer, 1.65f, error, boundSettings) ||
            !CommitRuntimePlayerViewAsset(boundScene, boundRig, reopened, P1ArmsAssetId, error))
            Fail("serialized native skeleton socket binding failed: " + error);

        const std::array<wi::ecs::Entity, 3> sockets = {
            boundRig.primaryHandSocket, boundRig.offHandSocket, boundRig.twoHandSupportSocket,
        };
        for (std::size_t role = 0; role < sockets.size(); ++role)
        {
            const auto target = boundRig.socketTargets[role];
            if (target == wi::ecs::INVALID_ENTITY ||
                boundScene.hierarchy.GetComponent(sockets[role])->parentID != target ||
                !Near3(boundScene.transforms.GetComponent(sockets[role])->translation_local, XMFLOAT3(0,0,0)))
                Fail("native hand/grip target was not remapped and bound at its authored origin");
        }

        const auto originalTargets = boundRig.socketTargets;
        const auto duplicate = boundScene.Entity_CreateTransform("Ambiguous primary grip");
        boundScene.Component_Attach(duplicate, originalTargets[0], true);
        boundScene.metadatas.Create(duplicate).bool_values.set(PlayerViewSocketMetadataKeys[0], true);
        if (BindRuntimePlayerViewRigSockets(boundScene, boundRig, boundRig.viewModelRoot, error) ||
            error.find("Multiple") == std::string::npos ||
            boundRig.socketTargets != originalTargets)
            Fail("ambiguous anchor was not rejected atomically");
        for (std::size_t role = 0; role < sockets.size(); ++role)
            if (boundScene.hierarchy.GetComponent(sockets[role])->parentID != originalTargets[role])
                Fail("failed anchor validation changed an existing socket");
        boundScene.Entity_Remove(duplicate);

        // An unrelated world skeleton's tags must never become first-person anchors.
        const auto foreign = boundScene.Entity_CreateTransform("Unrelated world rig");
        boundScene.metadatas.Create(foreign).bool_values.set(PlayerViewSocketMetadataKeys[0], true);
        if (!BindRuntimePlayerViewRigSockets(boundScene, boundRig, boundRig.viewModelRoot, error))
            Fail("world metadata contaminated view-model socket resolution");

        const auto invalid = boundScene.Entity_CreateTransform("Non-skeletal primary anchor");
        boundScene.Component_Attach(invalid, boundRig.viewModelRoot, true);
        boundScene.metadatas.Create(invalid).bool_values.set(PlayerViewSocketMetadataKeys[0], true);
        if (BindRuntimePlayerViewRigSockets(boundScene, boundRig, boundRig.viewModelRoot, error) ||
            error.find("skeleton") == std::string::npos)
            Fail("a non-skeletal anchor was accepted");
        boundScene.Entity_Remove(invalid);

        // Missing roles restore their original fallback offsets without affecting others.
        boundScene.metadatas.GetComponent(originalTargets[1])->bool_values.erase(PlayerViewSocketMetadataKeys[1]);
        if (!BindRuntimePlayerViewRigSockets(boundScene, boundRig, boundRig.viewModelRoot, error) ||
            boundRig.socketTargets[0] != originalTargets[0] ||
            boundRig.socketTargets[1] != wi::ecs::INVALID_ENTITY ||
            boundRig.socketTargets[2] != originalTargets[2] ||
            boundScene.hierarchy.GetComponent(sockets[1])->parentID != boundRig.presentationRoot ||
            !Near3(boundScene.transforms.GetComponent(sockets[1])->translation_local, boundSettings.offHandOffset))
            Fail("partial hand-binding fallback changed the authored roles or original offsets");

        const auto cyclic = boundScene.Entity_CreateTransform("Cyclic view model");
        boundScene.Component_Attach(cyclic, sockets[0], true);
        if (BindRuntimePlayerViewRigSockets(boundScene, boundRig, cyclic, error) ||
            error.find("cyclic") == std::string::npos)
            Fail("binding accepted a view model below its own hand socket");
        boundScene.Entity_Remove(cyclic);

        const auto modelRoot = boundRig.viewModelRoot;
        DespawnRuntimePlayerViewRig(boundScene, boundRig);
        for (const auto socket : sockets)
            if (boundScene.transforms.Contains(socket))
                Fail("bone-bound socket leaked during despawn");
        if (boundScene.transforms.Contains(modelRoot) || !boundScene.transforms.Contains(boundPlayer))
            Fail("bone-bound hierarchy cleanup damaged Player ownership");
    }

    std::cout
        << "PASS: parented dual-hand Player View Rig, serialized skeletal sockets and Wicked foreground policy\n";
    return EXIT_SUCCESS;
}
