#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ProjectService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    struct TemporaryDirectory
    {
        fs::path path;
        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const fs::path& root, const char* message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "RenegadeScreenTests: " << message << '\n';
        return 1;
    }

    ScreenDocument MakeScreen(
        const StableId& projectId,
        const StableId& documentId,
        const std::string& pathHint)
    {
        ScreenDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            RuntimeScreenDocumentType,
            pathHint,
            "Renegade LP03 screen tests");
        document.envelope.documentId = documentId;
        document.designWidth = 1280.0f;
        document.designHeight = 720.0f;
        document.actions = {{RuntimeScreenPlayAction}, {RuntimeScreenQuitAction}};
        document.widgets = {
            {GenerateStableId(), ScreenWidgetKind::Image, "Background",
                {0, 0, 1280, 720}, true, false, {},
                "Content/UI/background.png", {}},
            {GenerateStableId(), ScreenWidgetKind::Text, "Title",
                {240, 120, 800, 100}, true, false, "RENEGADE", {}, {}},
            {GenerateStableId(), ScreenWidgetKind::Button, "Play",
                {440, 330, 400, 76}, true, true, "PLAY", {},
                RuntimeScreenPlayAction},
            {GenerateStableId(), ScreenWidgetKind::Button, "Quit",
                {440, 440, 400, 76}, true, true, "QUIT", {},
                RuntimeScreenQuitAction},
        };
        document.focusOrder = {
            document.widgets[2].id,
            document.widgets[3].id,
        };
        return document;
    }

    std::string ReadAll(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return {
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
        };
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade lp03 screen tests " + std::to_string(unique))
    };
    const fs::path root = temporary.path / "Project With Spaces";
    const fs::path actualScreen =
        root / "Content/UI/Renamed/Main.renegade-screen";
    const fs::path secondScreen =
        root / "Content/UI/Renamed/RoundTrip.renegade-screen";
    const fs::path background = root / "Content/UI/background.png";
    fs::create_directories(background.parent_path());
    {
        std::ofstream resource(background, std::ios::binary);
        resource << "LP03 deterministic resource fixture";
    }

    const StableId projectId = GenerateStableId();
    const StableId documentId = GenerateStableId();

    const fs::path startupScene = root / "Content/Scenes/Main.wiscene";
    fs::create_directories(startupScene.parent_path());
    {
        std::ofstream scene(startupScene, std::ios::binary);
        scene << "LP03 project descriptor fixture";
    }
    const fs::path descriptor = root / "Screen Project.renegade";
    {
        std::ofstream project(descriptor, std::ios::binary);
        project
            << "format = renegade-project\n"
            << "version = 1\n\n"
            << "[project]\n"
            << "project_id = " << projectId << '\n'
            << "name = LP03 Screen Project\n"
            << "startup_scene = Content/Scenes/Main.wiscene\n"
            << "startup_screen_id = " << documentId << '\n'
            << "startup_screen = Content/UI/Main.renegade-screen\n";
    }
    ProjectService projects;
    ProjectMetadata metadata;
    std::string projectError;
    if (!projects.InspectProject(
            descriptor.generic_u8string(),
            metadata,
            projectError) ||
        metadata.startupScreenId != documentId ||
        metadata.startupScreen != "Content/UI/Main.renegade-screen")
    {
        return Fail(temporary.path, "project startup screen reference did not round trip");
    }

    const fs::path incompleteDescriptor = root / "Incomplete Screen.renegade";
    {
        std::ofstream project(incompleteDescriptor, std::ios::binary);
        project
            << "format = renegade-project\n"
            << "version = 1\n\n"
            << "[project]\n"
            << "project_id = " << projectId << '\n'
            << "name = Incomplete Screen Project\n"
            << "startup_scene = Content/Scenes/Main.wiscene\n"
            << "startup_screen = Content/UI/Main.renegade-screen\n";
    }
    if (projects.InspectProject(
            incompleteDescriptor.generic_u8string(),
            metadata,
            projectError))
    {
        return Fail(temporary.path, "path-only startup screen reference was accepted");
    }

    ScreenDocument document = MakeScreen(
        projectId,
        documentId,
        "Content/UI/Renamed/Main.renegade-screen");
    std::string error;

    if (!WriteScreenDocument(
            actualScreen.generic_u8string(),
            document,
            error))
    {
        return Fail(temporary.path, "valid screen did not serialize");
    }

    ScreenDocument reloaded;
    if (!ReadScreenDocument(
            actualScreen.generic_u8string(),
            projectId,
            reloaded,
            error) ||
        reloaded.envelope.documentId != documentId ||
        reloaded.widgets.size() != document.widgets.size() ||
        reloaded.focusOrder != document.focusOrder ||
        reloaded.widgets[2].actionId != RuntimeScreenPlayAction)
    {
        return Fail(temporary.path, "screen reload was not structurally equivalent");
    }

    if (!WriteScreenDocument(
            secondScreen.generic_u8string(),
            reloaded,
            error) ||
        ReadAll(actualScreen) != ReadAll(secondScreen))
    {
        return Fail(temporary.path, "screen serialization was not deterministic");
    }
    fs::remove(secondScreen);

    std::string resolved;
    if (!ResolveRuntimeScreenDocumentPath(
            root.generic_u8string(),
            projectId,
            documentId,
            "Content/UI/Stale/Main.renegade-screen",
            resolved,
            error) ||
        fs::u8path(resolved) != fs::weakly_canonical(actualScreen))
    {
        return Fail(temporary.path, "stable screen ID did not survive a move");
    }

    if (!ResolveScreenResourcePath(
            root.generic_u8string(),
            "Content/UI/background.png",
            resolved,
            error) ||
        fs::u8path(resolved) != fs::weakly_canonical(background))
    {
        return Fail(temporary.path, "screen resource did not resolve inside Content");
    }
    if (ResolveScreenResourcePath(
            root.generic_u8string(),
            "Content/UI/missing.png",
            resolved,
            error) ||
        error.find("missing") == std::string::npos)
    {
        return Fail(temporary.path, "missing background did not fail closed");
    }
    if (ResolveScreenResourcePath(
            root.generic_u8string(),
            "../outside.png",
            resolved,
            error))
    {
        return Fail(temporary.path, "unsafe resource path was accepted");
    }

    ScreenDocument invalid = document;
    invalid.widgets[3].id = invalid.widgets[2].id;
    if (ValidateScreenDocument(invalid, projectId, error))
    {
        return Fail(temporary.path, "duplicate widget identity was accepted");
    }

    invalid = document;
    invalid.actions[1].id = invalid.actions[0].id;
    if (ValidateScreenDocument(invalid, projectId, error))
    {
        return Fail(temporary.path, "duplicate action identity was accepted");
    }

    invalid = document;
    invalid.actions[0].id = "continue";
    if (ValidateScreenDocument(invalid, projectId, error))
    {
        return Fail(temporary.path, "missing play action was accepted");
    }

    invalid = document;
    invalid.widgets[2].actionId = "launch.level.one";
    if (ValidateScreenDocument(invalid, projectId, error))
    {
        return Fail(temporary.path, "unknown widget action was accepted");
    }

    invalid = document;
    invalid.focusOrder[1] = invalid.widgets[1].id;
    if (ValidateScreenDocument(invalid, projectId, error))
    {
        return Fail(temporary.path, "invalid focus target was accepted");
    }

    ScreenDocument wrongOwner;
    if (ReadScreenDocument(
            actualScreen.generic_u8string(),
            GenerateStableId(),
            wrongOwner,
            error))
    {
        return Fail(temporary.path, "screen owned by another project was accepted");
    }

    const fs::path duplicate = root / "Content/UI/Duplicate.renegade-screen";
    fs::copy_file(actualScreen, duplicate);
    if (ResolveRuntimeScreenDocumentPath(
            root.generic_u8string(),
            projectId,
            documentId,
            "Content/UI/Renamed/Main.renegade-screen",
            resolved,
            error) ||
        error.find("ambiguous") == std::string::npos)
    {
        return Fail(temporary.path, "duplicate screen identity was not diagnosed");
    }

    const fs::path malformed = root / "Content/UI/Malformed.renegade-screen";
    {
        std::ofstream stream(malformed, std::ios::binary);
        stream << "not a Renegade document\n";
    }
    ScreenDocument malformedDocument;
    if (ReadScreenDocument(
            malformed.generic_u8string(),
            projectId,
            malformedDocument,
            error))
    {
        return Fail(temporary.path, "malformed screen document was accepted");
    }

    std::cout << "PASS: LP03 Runtime screen serialization and failures\n";
    return 0;
}
