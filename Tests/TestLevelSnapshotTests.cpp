#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TestLevelSnapshotService.h"

namespace
{
    namespace fs = std::filesystem;

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    struct TemporaryDirectory
    {
        fs::path path;

        ~TemporaryDirectory()
        {
            std::error_code error;
            fs::remove_all(path, error);
        }
    };

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            return {};
        }
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            return {};
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (!stream)
            {
                return {};
            }
        }
        return bytes;
    }

    bool FindTranslationX(
        const renegade::bridge::SceneService& scenes,
        const std::string& name,
        float& value)
    {
        for (const auto& entity : scenes.ListEntities())
        {
            if (entity.name != name)
            {
                continue;
            }
            const auto* transform =
                scenes.GetScene().transforms.GetComponent(entity.entity);
            if (transform == nullptr)
            {
                return false;
            }
            value = transform->translation_local.x;
            return true;
        }
        return false;
    }
}

int main()
{
    TemporaryDirectory fixture;
    fixture.path = fs::temp_directory_path() /
        ("renegade-lp04-snapshot-" +
            std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()));

    const fs::path projectRoot = fixture.path / "Project";
    const fs::path scenePath =
        projectRoot / "Content" / "Scenes" / "Authoritative.wiscene";
    fs::create_directories(scenePath.parent_path());

    renegade::bridge::StudioSession session;
    const auto landmark = wi::ecs::CreateEntity();
    session.Scenes().GetScene().names.Create(landmark) = "Snapshot Landmark";
    auto& transform = session.Scenes().GetScene().transforms.Create(landmark);
    transform.translation_local = XMFLOAT3(1.0f, 2.0f, 3.0f);
    transform.SetDirty();
    transform.UpdateTransform();

    if (!session.SaveScene(scenePath.generic_u8string()))
    {
        return Fail("authoritative LP04 fixture scene could not be saved");
    }
    if (session.Commands().IsDirty() ||
        session.Scenes().CurrentPath() != scenePath.generic_u8string())
    {
        return Fail("authoritative LP04 fixture did not establish saved state");
    }

    const auto authoritativeBytes = ReadBytes(scenePath);
    if (authoritativeBytes.empty())
    {
        return Fail("authoritative LP04 fixture bytes could not be read");
    }

    if (!session.Commands().Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                session.Scenes().GetScene(),
                landmark,
                XMFLOAT3(9.0f, 2.0f, 3.0f))))
    {
        return Fail("unsaved LP04 edit did not execute");
    }
    if (!session.Commands().IsDirty())
    {
        return Fail("unsaved LP04 edit did not mark the scene dirty");
    }

    const std::size_t undoBeforeSnapshot = session.Commands().UndoCount();
    const std::size_t redoBeforeSnapshot = session.Commands().RedoCount();
    const std::string pathBeforeSnapshot = session.Scenes().CurrentPath();

    renegade::bridge::TestLevelSnapshotService snapshots(
        session.Scenes(),
        session.Commands());
    renegade::bridge::TestLevelSnapshot snapshot;
    std::string snapshotError;
    if (!snapshots.Create(
            projectRoot.generic_u8string(),
            snapshot,
            snapshotError))
    {
        std::cerr << snapshotError << '\n';
        return Fail("LP04 snapshot creation failed");
    }
    if (!snapshot.IsValid() || !fs::is_regular_file(snapshot.scenePath))
    {
        return Fail("LP04 snapshot did not produce a valid temporary WISCENE");
    }

    // The authoritative scene must be byte-for-byte unchanged, and snapshot
    // creation must not act like Save/Save As from Studio's perspective.
    if (ReadBytes(scenePath) != authoritativeBytes)
    {
        return Fail("LP04 snapshot modified the authoritative scene bytes");
    }
    if (session.Scenes().CurrentPath() != pathBeforeSnapshot ||
        !session.Commands().IsDirty() ||
        session.Commands().UndoCount() != undoBeforeSnapshot ||
        session.Commands().RedoCount() != redoBeforeSnapshot)
    {
        return Fail("LP04 snapshot changed path, dirty state or command history");
    }

    auto prepared = renegade::bridge::PrepareWickedSceneOpen(snapshot.scenePath);
    if (!prepared.IsReady())
    {
        std::cerr << prepared.Error() << '\n';
        return Fail(
            "LP04 snapshot did not validate through PrepareWickedSceneOpen");
    }

    renegade::bridge::StudioSession reopenedSnapshot;
    if (!reopenedSnapshot.LoadScene(snapshot.scenePath))
    {
        return Fail("LP04 snapshot could not be reopened");
    }
    float snapshotX = 0.0f;
    if (!FindTranslationX(
            reopenedSnapshot.Scenes(),
            "Snapshot Landmark",
            snapshotX) ||
        !NearlyEqual(snapshotX, 9.0f))
    {
        return Fail("LP04 snapshot did not contain the unsaved scene edit");
    }

    renegade::bridge::StudioSession reopenedAuthoritative;
    if (!reopenedAuthoritative.LoadScene(scenePath.generic_u8string()))
    {
        return Fail("authoritative LP04 fixture could not be reopened");
    }
    float authoritativeX = 0.0f;
    if (!FindTranslationX(
            reopenedAuthoritative.Scenes(),
            "Snapshot Landmark",
            authoritativeX) ||
        !NearlyEqual(authoritativeX, 1.0f))
    {
        return Fail(
            "LP04 snapshot leaked the unsaved edit into the source scene");
    }

    const fs::path successfulSession = fs::u8path(snapshot.sessionDirectory);
    if (!snapshots.Cleanup(snapshot, snapshotError))
    {
        std::cerr << snapshotError << '\n';
        return Fail("successful LP04 snapshot cleanup failed");
    }
    if (fs::exists(successfulSession) ||
        fs::exists(projectRoot / "Intermediate" / "TestLevelSnapshots"))
    {
        return Fail("successful LP04 snapshot left temporary data behind");
    }

    // Force a failure only after a real temporary archive exists. Create()
    // must remove that archive/session itself and preserve the editor state.
    if (!session.Commands().Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                session.Scenes().GetScene(),
                landmark,
                XMFLOAT3(12.0f, 2.0f, 3.0f))))
    {
        return Fail("LP04 failure-injection edit did not execute");
    }
    const std::size_t undoBeforeFailure = session.Commands().UndoCount();
    const std::size_t redoBeforeFailure = session.Commands().RedoCount();
    renegade::bridge::TestLevelSnapshot failedSnapshot;
    if (snapshots.Create(
            projectRoot.generic_u8string(),
            failedSnapshot,
            snapshotError,
            renegade::bridge::TestLevelSnapshotFailureInjection::
                AfterArchiveWrite))
    {
        return Fail("LP04 forced snapshot failure unexpectedly succeeded");
    }
    if (failedSnapshot.IsValid())
    {
        return Fail("LP04 forced failure returned a live snapshot handle");
    }
    if (fs::exists(projectRoot / "Intermediate" / "TestLevelSnapshots"))
    {
        return Fail("LP04 forced failure left temporary snapshot data behind");
    }
    if (ReadBytes(scenePath) != authoritativeBytes ||
        session.Scenes().CurrentPath() != pathBeforeSnapshot ||
        !session.Commands().IsDirty() ||
        session.Commands().UndoCount() != undoBeforeFailure ||
        session.Commands().RedoCount() != redoBeforeFailure)
    {
        return Fail("LP04 forced failure changed authoritative editor state");
    }

    std::cout << "PASS: LP04 unsaved Test Level snapshot primitive\n";
    return 0;
}
