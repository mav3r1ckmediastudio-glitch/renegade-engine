#include "renegade/bridge/ScreenAuthoringSession.h"

#include <chrono>
#include <filesystem>
#include <iostream>
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

    int Fail(const std::string& message)
    {
        std::cerr << "RenegadeScreenAuthoringSessionTests: " << message << '\n';
        return 1;
    }

    ScreenDocument MakeDocument(const StableId& projectId)
    {
        ScreenDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            RuntimeScreenDocumentType,
            "Content/Screens/Main.renegade-screen",
            "Gate 8C authoring fixture");
        document.designWidth = 1280.0f;
        document.designHeight = 720.0f;
        document.actions = {{RuntimeScreenPlayAction}, {RuntimeScreenQuitAction}};

        ScreenWidget title;
        title.id = GenerateStableId();
        title.kind = ScreenWidgetKind::Text;
        title.name = "Title";
        title.rect = {240.0f, 100.0f, 800.0f, 100.0f};
        title.text = "RENEGADE";
        title.style = MakeScreenWidgetStyleTemplate(title.kind, title.rect.height);

        ScreenWidget play;
        play.id = GenerateStableId();
        play.kind = ScreenWidgetKind::Button;
        play.name = "Play";
        play.rect = {440.0f, 330.0f, 400.0f, 76.0f};
        play.text = "PLAY";
        play.actionId = RuntimeScreenPlayAction;
        play.style = MakeScreenWidgetStyleTemplate(play.kind, play.rect.height);

        ScreenWidget quit;
        quit.id = GenerateStableId();
        quit.kind = ScreenWidgetKind::Button;
        quit.name = "Quit";
        quit.rect = {440.0f, 440.0f, 400.0f, 76.0f};
        quit.text = "QUIT";
        quit.actionId = RuntimeScreenQuitAction;
        quit.layoutMode = ScreenLayoutMode::Anchored;
        quit.anchors = {
            0.5f, 0.0f, 0.5f, 0.0f,
            -200.0f, 440.0f, 200.0f, 516.0f,
        };
        quit.style = MakeScreenWidgetStyleTemplate(quit.kind, quit.rect.height);

        document.widgets = {title, play, quit};
        document.focusOrder = {play.id, quit.id};
        return document;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade screen authoring " + std::to_string(unique))};
    const fs::path screenPath =
        temporary.path / "Content/Screens/Main.renegade-screen";
    fs::create_directories(screenPath.parent_path());

    const StableId projectId = GenerateStableId();
    ScreenDocument source = MakeDocument(projectId);
    const StableId titleId = source.widgets[0].id;
    const StableId quitId = source.widgets[2].id;
    const ScreenAnchors originalQuitAnchors = source.widgets[2].anchors;
    std::string error;
    if (!WriteScreenDocument(screenPath.generic_u8string(), source, error))
        return Fail("fixture write failed: " + error);

    ScreenAuthoringSession session;
    if (!session.Open(screenPath.generic_u8string(), projectId, error) ||
        !session.IsLoaded() || session.IsDirty() || session.CanUndo() ||
        session.CanRedo())
    {
        return Fail("Open did not establish a clean authoring session: " + error);
    }

    ScreenRect titleRect;
    if (!ResolveScreenWidgetRect(session.Document(), titleId, titleRect, error))
        return Fail("could not resolve title before edit: " + error);
    ScreenWidgetAuthoringEdit titleEdit;
    titleEdit.name = "Main Title";
    titleEdit.text = "THE PAGANACHT";
    titleEdit.resolvedRect = {220.0f, 90.0f, 840.0f, 110.0f};
    titleEdit.visible = true;
    titleEdit.enabled = true;
    if (!session.UpdateWidget(titleId, titleEdit, error) ||
        !session.IsDirty() || !session.CanUndo() || session.CanRedo())
    {
        return Fail("absolute widget edit did not enter history: " + error);
    }
    const auto* title = session.FindWidget(titleId);
    if (!title || title->name != "Main Title" ||
        title->text != "THE PAGANACHT" ||
        title->rect.x != 220.0f || title->rect.width != 840.0f)
    {
        return Fail("absolute widget edit was not applied exactly");
    }

    ScreenRect quitRect;
    if (!ResolveScreenWidgetRect(session.Document(), quitId, quitRect, error))
        return Fail("could not resolve anchored button before edit: " + error);
    ScreenWidgetAuthoringEdit quitEdit;
    quitEdit.name = "Exit Game";
    quitEdit.text = "EXIT";
    quitEdit.resolvedRect = {420.0f, 450.0f, 440.0f, 80.0f};
    quitEdit.visible = true;
    quitEdit.enabled = true;
    if (!session.UpdateWidget(quitId, quitEdit, error))
        return Fail("anchored widget edit failed: " + error);
    ScreenRect movedQuit;
    if (!ResolveScreenWidgetRect(
            session.Document(), quitId, movedQuit, error) ||
        movedQuit.x != 420.0f || movedQuit.y != 450.0f ||
        movedQuit.width != 440.0f || movedQuit.height != 80.0f ||
        session.FindWidget(quitId)->layoutMode != ScreenLayoutMode::Anchored ||
        session.FindWidget(quitId)->anchors.minimumX !=
            originalQuitAnchors.minimumX ||
        session.FindWidget(quitId)->anchors.minimumY !=
            originalQuitAnchors.minimumY ||
        session.FindWidget(quitId)->anchors.maximumX !=
            originalQuitAnchors.maximumX ||
        session.FindWidget(quitId)->anchors.maximumY !=
            originalQuitAnchors.maximumY)
    {
        return Fail("anchored edit did not preserve anchors and resolved layout");
    }

    const std::size_t undoBeforeNoOp = session.UndoCount();
    if (!session.UpdateWidget(quitId, quitEdit, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("identical edit polluted Screen history");
    }

    ScreenWidgetAuthoringEdit invalid = quitEdit;
    invalid.resolvedRect.width = -1.0f;
    if (session.UpdateWidget(quitId, invalid, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("invalid edit entered Screen history");
    }

    if (!session.Undo(error) || session.FindWidget(quitId)->text != "QUIT" ||
        !session.Undo(error) || session.FindWidget(titleId)->text != "RENEGADE" ||
        !session.Redo(error) || session.FindWidget(titleId)->text != "THE PAGANACHT" ||
        !session.Redo(error) || session.FindWidget(quitId)->text != "EXIT")
    {
        return Fail("Undo/Redo did not restore exact Screen snapshots: " + error);
    }

    if (!session.Save(error) || session.IsDirty())
        return Fail("transactional Save did not mark the Screen clean: " + error);

    ScreenAuthoringSession reopened;
    if (!reopened.Open(screenPath.generic_u8string(), projectId, error))
        return Fail("saved Screen did not reopen: " + error);
    ScreenRect reopenedQuit;
    if (!ResolveScreenWidgetRect(
            reopened.Document(), quitId, reopenedQuit, error) ||
        reopened.FindWidget(titleId)->text != "THE PAGANACHT" ||
        reopened.FindWidget(quitId)->text != "EXIT" ||
        reopenedQuit.x != 420.0f || reopenedQuit.width != 440.0f)
    {
        return Fail("saved Screen did not preserve authored content/layout");
    }

    std::cout << "PASS: Gate 8C Screen authoring session preserves validated edits, anchored layout, history and transactional Save/Open\n";
    return 0;
}
