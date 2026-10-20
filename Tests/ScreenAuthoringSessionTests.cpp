#include "renegade/bridge/ScreenInteractionPlan.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenWidgetPresets.h"
#include "renegade/bridge/ScreenSimulation.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/CreatorScreenFontImportService.h"
#include "renegade/bridge/ScreenCreatorResourceService.h"
#include "../Studio/src/ScreenCanvasGrid.h"
#include "../Studio/src/ScreenElementSnap.h"
#include "../Studio/src/ScreenEditorPanelLayout.h"
#include "../Studio/src/ScreenEditorUndoTimeline.h"
#include "../Studio/src/ScreenEditorWorkspacePreferences.h"
#include "../Studio/src/ScreenRuntimeLiveProbe.h"
#include "../Studio/src/ScreenEditorAutosavePolicy.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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
            "Gate 8D authoring fixture");
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

    ScreenWidgetCreatorEdit CreatorEditFor(const ScreenWidget& widget)
    {
        ScreenWidgetCreatorEdit edit;
        edit.resourcePath = widget.resourcePath;
        edit.actionId = widget.actionId;
        edit.parentId = widget.parentId;
        edit.layoutMode = widget.layoutMode;
        edit.anchors = widget.anchors;
        edit.style = widget.style;
        return edit;
    }

    bool SameRect(const ScreenRect& left, const ScreenRect& right)
    {
        return left.x == right.x && left.y == right.y &&
            left.width == right.width && left.height == right.height;
    }
}

int main()
{
    using namespace renegade::bridge;

    // UI-017: creator can bind a player accessibility action from one
    // native picker selection. The declaration and binding are one Undo step,
    // with no fabricated StoryFlow route or separate manual action creation.
    {
        const auto project=GenerateStableId();
        TemporaryDirectory temp{fs::temp_directory_path()/
            fs::u8path("renegade-ui17-creator-"+GenerateStableId())};
        const auto path=temp.path/"Content/Screens/UI17Accessibility.renegade-screen";
        fs::create_directories(path.parent_path());
        auto initial=MakeDocument(project);
        initial.envelope.pathHint="Content/Screens/UI17Accessibility.renegade-screen";
        std::string issue;
        if(!WriteScreenDocument(path.generic_u8string(),initial,issue))
            return Fail("UI-017 fixture could not be written: "+issue);
        ScreenAuthoringSession editor;
        if(!editor.Open(path.generic_u8string(),project,issue))
            return Fail("UI-017 fixture could not open: "+issue);
        const auto target=initial.widgets[1].id;
        const auto before=editor.UndoCount(),revision=editor.Revision();
        if(editor.BindRuntimeUiAccessibilityAction(initial.widgets[0].id,
                RuntimeUiReducedMotionToggleAction,issue)||
            editor.BindRuntimeUiAccessibilityAction(target,"quit",issue)||
            editor.UndoCount()!=before||editor.Revision()!=revision)
            return Fail("UI-017 invalid accessibility binding changed the Screen");
        if(!editor.BindRuntimeUiAccessibilityAction(target,
                RuntimeUiReducedMotionToggleAction,issue)||
            editor.UndoCount()!=before+1||
            editor.FindWidget(target)->actionId!=RuntimeUiReducedMotionToggleAction||
            std::count_if(editor.Document().actions.begin(),editor.Document().actions.end(),
                [](const auto& a){return a.id==RuntimeUiReducedMotionToggleAction;})!=1)
            return Fail("UI-017 atomic accessibility action binding: "+issue);
        if(!editor.Undo(issue)||editor.FindWidget(target)->actionId!=RuntimeScreenPlayAction||
            !editor.Redo(issue)||
            editor.FindWidget(target)->actionId!=RuntimeUiReducedMotionToggleAction||
            !editor.Save(issue))
            return Fail("UI-017 accessibility binding Undo/Redo/Save: "+issue);
        ScreenAuthoringSession reopened;
        if(!reopened.Open(path.generic_u8string(),project,issue)||
            reopened.FindWidget(target)->actionId!=RuntimeUiReducedMotionToggleAction)
            return Fail("UI-017 accessibility binding persistence: "+issue);
        const auto noOpRevision=reopened.Revision();
        if(!reopened.BindRuntimeUiAccessibilityAction(target,
                RuntimeUiReducedMotionToggleAction,issue)||
            reopened.Revision()!=noOpRevision)
            return Fail("UI-017 repeated choice unexpectedly mutated the Screen");
    }

    // UI-016 Live Test distinguishes transport availability, current gameplay
    // sample freshness and the precise authored project. No gameplay writes.
    {
        const std::string project=GenerateStableId();
        const std::string valid=std::string(R"({"schema":"renegade.diagnostics.v2","heartbeat_age_ms":25,"application_heartbeat_age_ms":25,"transport_alive":true,"state":{"process":{"pid":5678,"type":"runtime","started_unix_ms":1792312352207},"ui":{"project_id":")")
            +project+R"(","hud_loaded":true,"hud_bindings_ready":true,"hud_binding_issues":0,"ui_binding_revision":55}}})";
        const auto ready=renegade::studio::ProbeScreenRuntimeLive(valid,project);
        if(!ready.connected||!ready.gameplayFresh||!ready.projectMatched||
           ready.pid!=5678||ready.processStartedUnixMs!=1792312352207ull||
           !ready.hudLoaded||!ready.bindingsReady||ready.bindingIssues!=0||
           ready.bindingRevision!=55||ready.detail.find("MATCHED LIVE RUNTIME")==std::string::npos)
            return Fail("Read-only Live Test rejected a fresh matching Runtime session");
        if(renegade::studio::ProbeScreenRuntimeLive("").connected)
            return Fail("Live Test treated missing transport as gameplay simulation");
        auto stale=valid;
        const auto heartbeat=stale.find("\"application_heartbeat_age_ms\":25");
        stale.replace(heartbeat,std::string("\"application_heartbeat_age_ms\":25").size(),
            "\"application_heartbeat_age_ms\":9999");
        const auto suspended=renegade::studio::ProbeScreenRuntimeLive(stale,project);
        if(!suspended.connected||suspended.gameplayFresh||!suspended.projectMatched||
           suspended.detail.find("GAMEPLAY NOT ADVANCING")==std::string::npos)
            return Fail("Background Runtime was disconnected or stale gameplay was described as fresh");
        const auto mismatch=renegade::studio::ProbeScreenRuntimeLive(valid,GenerateStableId());
        if(!mismatch.connected||!mismatch.gameplayFresh||mismatch.projectMatched||
           mismatch.detail.find("OTHER PROJECT")==std::string::npos)
            return Fail("Live Test accepted data from the wrong project");
        const auto unidentified=renegade::studio::ProbeScreenRuntimeLive(valid);
        if(!unidentified.connected||unidentified.projectMatched||
           unidentified.detail.find("PROJECT IDENTITY UNVERIFIED")==std::string::npos)
            return Fail("Live Test claimed project identity without an editor project");
        auto foreign=valid;const auto kind=foreign.find("\"type\":\"runtime\"");
        foreign.replace(kind,std::string("\"type\":\"runtime\"").size(),"\"type\":\"studio\"");
        if(renegade::studio::ProbeScreenRuntimeLive(foreign,project).connected)
            return Fail("Live Test trusted a Studio process as Runtime");
    }

    // Responsive safe-area edits are governed Screen transactions, not an
    // editor-only visual offset. Undo/Redo and save/reopen share one authority.
    {
        TemporaryDirectory temp{fs::temp_directory_path()/
            fs::u8path("renegade-ui-safearea-"+GenerateStableId())};
        const auto project=GenerateStableId();
        const auto path=temp.path/"Content/Screens/Main.renegade-screen";
        fs::create_directories(path.parent_path());
        auto document=MakeDocument(project);
        std::string error;
        if(!WriteScreenDocument(path.generic_u8string(),document,error))
            return Fail("Safe-area session fixture could not save: "+error);
        ScreenAuthoringSession session;
        if(!session.Open(path.generic_u8string(),project,error))
            return Fail("Safe-area session could not open: "+error);
        const auto previous=session.UndoCount();
        if(!session.UpdateScreenSafeArea({24,32,48,40},error)||
           session.UndoCount()!=previous+1)
            return Fail("Safe-area transaction did not commit: "+error);
        ScreenRect anchored{},absolute{};
        if(!ResolveScreenWidgetRect(session.Document(),session.Document().widgets[2].id,anchored,error)||
           !ResolveScreenWidgetRect(session.Document(),session.Document().widgets[0].id,absolute,error)||
           anchored.x!=428||anchored.y!=472||absolute.x!=240||absolute.y!=100)
            return Fail("Safe-area editor geometry changed absolute art or ignored root anchor");
        // Moving a root-anchored element must translate the resolved rectangle
        // against the safe-area parent, not the full design canvas.
        const auto& rootAnchor=session.Document().widgets[2];
        const auto anchoredId=rootAnchor.id;
        ScreenWidgetAuthoringEdit move;
        move.name=rootAnchor.name;move.text=rootAnchor.text;
        move.visible=rootAnchor.visible;move.enabled=rootAnchor.enabled;
        move.resolvedRect=anchored;
        move.resolvedRect.x+=20;move.resolvedRect.y+=10;
        if(!session.UpdateWidget(anchoredId,move,error) ||
           !ResolveScreenWidgetRect(session.Document(),anchoredId,anchored,error)||
           anchored.x!=448||anchored.y!=482)
            return Fail("Safe-area anchored drag did not preserve intended canvas position: "+error);
        if(!session.Undo(error)||
           !ResolveScreenWidgetRect(session.Document(),anchoredId,anchored,error)||
           anchored.x!=428||anchored.y!=472)
            return Fail("Safe-area anchored drag Undo did not restore geometry");
        const auto beforeInvalid=session.UndoCount();
        if(session.UpdateScreenSafeArea({800,0,800,0},error)||
           session.UndoCount()!=beforeInvalid||session.Document().safeArea.left!=24)
            return Fail("Invalid safe-area edit changed document/history");
        if(!session.Undo(error)||session.Document().safeArea.left!=0||
           !session.Redo(error)||session.Document().safeArea.left!=24)
            return Fail("Safe-area Undo/Redo did not restore margins");
        if(!session.Save(error))
            return Fail("Safe-area save failed: "+error);
        ScreenAuthoringSession reopened;
        if(!reopened.Open(path.generic_u8string(),project,error)||
           reopened.Document().safeArea.left!=24||reopened.Document().safeArea.bottom!=40)
            return Fail("Safe-area Screen save/reopen lost project settings");
    }

    // UI-008: bounded size constraints do not overwrite authored geometry.
    // They use the shared renderer/input rect resolver and transaction history.
    {
        TemporaryDirectory temp{fs::temp_directory_path()/
            fs::u8path("renegade-ui-size-constraints-"+GenerateStableId())};
        const auto project=GenerateStableId();
        const auto path=temp.path/"Content/Screens/Main.renegade-screen";
        fs::create_directories(path.parent_path());
        auto document=MakeDocument(project);
        std::string error;
        if(!WriteScreenDocument(path.generic_u8string(),document,error))
            return Fail("Constraint fixture write failed: "+error);
        ScreenAuthoringSession session;
        if(!session.Open(path.generic_u8string(),project,error))
            return Fail("Constraint fixture open failed: "+error);
        const auto id=document.widgets[2].id;
        const auto undo=session.UndoCount();
        if(!session.UpdateWidgetSizeConstraints(id,{0,0,300,50},error)||
            session.UndoCount()!=undo+1)
            return Fail("Constraint edit transaction rejected: "+error);
        ScreenRect result{};
        if(!ResolveScreenWidgetRect(session.Document(),id,result,error)||
           result.x!=440||result.y!=440||result.width!=300||result.height!=50||
           session.Document().widgets[2].rect.width!=400)
            return Fail("Constraint resolution mutated authored fallback or failed to clamp");
        const auto preInvalid=session.UndoCount();
        if(session.UpdateWidgetSizeConstraints(id,{301,0,300,50},error)||
           session.UpdateWidgetSizeConstraints(id,{0,0,-1,50},error)||
           session.UndoCount()!=preInvalid)
            return Fail("Inverted/negative widget constraints modified Screen history");
        if(!session.Undo(error)||session.Document().widgets[2].sizeConstraints.maximumWidth!=0||
           !session.Redo(error)||session.Document().widgets[2].sizeConstraints.maximumWidth!=300)
            return Fail("Widget size constraint Undo/Redo failed");
        if(!session.Save(error))
            return Fail("Widget size constraint Save failed: "+error);
        ScreenAuthoringSession reopened;
        if(!reopened.Open(path.generic_u8string(),project,error)||
           reopened.Document().widgets[2].sizeConstraints.maximumWidth!=300||
           !ResolveScreenWidgetRect(reopened.Document(),id,result,error)||
           result.width!=300||result.height!=50)
            return Fail("Size constraint save/reopen did not preserve Runtime geometry");
        if(!reopened.UpdateWidgetSizeConstraints(id,{0,0,0,0},error)||
           !ResolveScreenWidgetRect(reopened.Document(),id,result,error)||
           result.width!=400||result.height!=76)
            return Fail("Removing size constraints failed to restore fallback geometry");
    }

    // UI-09: custom TTF has a governed LP08 identity and a playable
    // Content path; selection remains stable across Screen save/reopen.
    // This tiny Gate-2 fixture proves packaging/metadata/path governance,
    // not successful glyph rasterization of a real font.
    {
        const TemporaryDirectory temp{
            fs::temp_directory_path() /
                fs::u8path("renegade-ui09-font-" + GenerateStableId())};
        const auto root = temp.path;
        const auto project = GenerateStableId();
        fs::create_directories(root / "Content" / "Screens");
        const auto external = root / "Candidate.ttf";
        {
            std::ofstream bytes(external, std::ios::binary);
            const unsigned char sfnt[] =
                {0, 1, 0, 0, 0, 1, 0, 0};
            bytes.write(reinterpret_cast<const char*>(sfnt),
                static_cast<std::streamsize>(sizeof(sfnt)));
        }
        const auto imported = CreatorScreenFontImportService::Import(
            root.generic_u8string(), project, external.generic_u8string());
        if (!imported.succeeded ||
            !imported.governedCommitted ||
            !IsValidStableId(imported.assetId))
            return Fail("UI-09 governed TTF import failed: " + imported.error);

        const auto catalogue = EnumerateScreenCreatorResources(
            root.generic_u8string(), project, ScreenCreatorResourceKind::Font);
        if (!catalogue.succeeded ||
            catalogue.choices.size() != 1 ||
            catalogue.choices.front().projectRelativePath != imported.runtimePath)
            return Fail("UI-09 imported font did not enter Screen font picker");

        std::string resolved, error;
        if (!ResolveScreenResourcePath(
                root.generic_u8string(), imported.runtimePath, resolved, error) ||
            !fs::is_regular_file(fs::u8path(resolved)))
            return Fail("UI-09 Screen Runtime could not resolve imported font");

        const auto second = CreatorScreenFontImportService::Import(
            root.generic_u8string(), project, external.generic_u8string());
        if (!second.succeeded || second.runtimePath == imported.runtimePath)
            return Fail("UI-09 duplicate filename overwrote another font");

        const auto badPath = root / "Wrong.otf";
        {
            std::ofstream otf(badPath, std::ios::binary);
            otf << "OTTO";
        }
        if (CreatorScreenFontImportService::Import(
                root.generic_u8string(), project,
                badPath.generic_u8string()).succeeded)
            return Fail("UI-09 accepted OTF unsupported by Screen schema");

        auto screen = MakeDocument(project);
        screen.widgets.front().style.text.fontResource = imported.runtimePath;
        const auto savedPath = root / "Content" / "Screens" /
            "Main.renegade-screen";
        if (!WriteScreenDocument(savedPath.generic_u8string(), screen, error))
            return Fail("UI-09 font-selected Screen could not save: " + error);
        ScreenDocument reopened;
        if (!ReadScreenDocument(
                savedPath.generic_u8string(), project, reopened, error) ||
            reopened.widgets.front().style.text.fontResource != imported.runtimePath)
            return Fail("UI-09 selected project font did not persist");

        if (!fs::exists(root / "Content" / "Fonts") ||
            !fs::exists(root / "SourceAssets" / "Fonts") ||
            !fs::exists(root / "Content" / "UI" / "Fonts"))
            return Fail("UI-09 imported font did not retain its project files");
    }

    // UI-027: UI workspace state is editor-only, survives restart, and
    // cannot silently accept corrupt config or unsafe geometry.
    {
        namespace fs = std::filesystem;
        const fs::path config = fs::temp_directory_path() /
            ("renegade-ui06-layout-" + GenerateStableId() + ".txt");
        using Prefs = renegade::studio::ScreenEditorWorkspacePreferences;
        Prefs authored;
        authored.left = 318.0f;
        authored.right = 420.0f;
        authored.spacing = 32;
        authored.tab = 3;
        authored.grid = false;
        authored.snap = true;
        Prefs restored;
        if (!authored.Valid() || !authored.Save(config) ||
            !Prefs::Load(config, restored) ||
            restored.left != 318.0f || restored.right != 420.0f ||
            restored.spacing != 32 || restored.tab != 3 ||
            restored.grid || !restored.snap)
            return Fail("UI-06 workspace settings did not persist and restore");
        {
            std::ofstream invalid(config, std::ios::trunc);
            invalid << "renegade-ui-workspace-v1\n"
                    << "-200 999999 5000 12 2 9\n";
        }
        if (Prefs::Load(config, restored) ||
            restored.left != 318.0f)
            return Fail("UI-06 accepted corrupt or unsafe workspace preferences");
        Prefs unsafe;
        unsafe.left = -1.0f;
        if (unsafe.Valid() || unsafe.Save(config))
            return Fail("UI-06 saved unsafe editor pane dimensions");
        std::error_code cleanup;
        fs::remove(config, cleanup);
    }

    // UI-07 autosave: idle after edits, defer during drag, no infinite
    // retry on disk failure, re-arm on a new change, preserve history.
    {
        using Policy = renegade::studio::ScreenEditorAutosavePolicy;
        Policy p;
        p.Reset();
        for (int i = 0; i < 30; ++i)
            p.Observe(false, 0, 0, 0.1, false);
        if (p.Due()) return Fail("autosave tried to write a clean Screen");
        for (int i = 0; i < 25; ++i)
            p.Observe(true, 1, 0, 0.1, true);
        if (p.Due())
            return Fail("autosave ran during active canvas manipulation");
        for (int i = 0; i < 14; ++i)
            p.Observe(true, 1, 0, 0.1, false);
        if (p.Due()) return Fail("autosave ignored the quiet period");
        p.Observe(true, 1, 0, 0.2, false);
        if (!p.Due()) return Fail("autosave did not fire after idle period");
        p.Attempted(); // Simulate transactional disk failure.
        for (int i = 0; i < 50; ++i)
            p.Observe(true, 1, 0, 0.1, false);
        if (p.Due())
            return Fail("failed autosave retried endlessly without an edit");
        p.Observe(true, 2, 0, 0.1, false);
        for (int i = 0; i < 15; ++i)
            p.Observe(true, 2, 0, 0.1, false);
        if (!p.Due())
            return Fail("new Screen revision did not re-arm autosave");
        p.Attempted();
        p.Observe(false, 2, 0, 0.1, false);
        if (p.Due()) return Fail("successful save did not disarm autosave");
        p.Edited(); // Flow route changed but Screen Undo count stayed 2.
        for (int i = 0; i < 16; ++i)
            p.Observe(true, 2, 0, 0.1, false);
        if (!p.Due())
            return Fail("Flow-only destination change was not autosaved");
        p.Reset();
        if (p.Due()) return Fail("autosave timer leaked to next Screen");
    }

    // UI-024: Ctrl+Z and Ctrl+Y must traverse Screen edits and Flow
    // route changes in the same order the creator performed them.
    {
        using Timeline = renegade::studio::ScreenEditorUndoTimeline;
        Timeline timeline;
        timeline.Reset(0, 0);
        timeline.ObserveScreen(1, 0); // Author a Text element.
        timeline.RecordFlowRoute();   // Wire a Button destination.
        timeline.ObserveScreen(2, 0); // Resize the Text element.
        if (timeline.NextUndo() != Timeline::Kind::Screen)
            return Fail("unified Undo did not prioritize latest Screen edit");
        timeline.DidUndo(1, 1);
        if (timeline.NextUndo() != Timeline::Kind::FlowRoute)
            return Fail("unified Undo skipped the intervening Flow route");
        timeline.DidUndo(1, 1);
        if (timeline.NextUndo() != Timeline::Kind::Screen)
            return Fail("unified Undo lost the first Screen edit");
        timeline.DidUndo(0, 2);
        if (timeline.NextRedo() != Timeline::Kind::Screen)
            return Fail("unified Redo did not restart with Screen edit");
        timeline.DidRedo(1, 1);
        if (timeline.NextRedo() != Timeline::Kind::FlowRoute)
            return Fail("unified Redo skipped the Flow route");
        timeline.DidRedo(1, 1);
        if (timeline.NextRedo() != Timeline::Kind::Screen)
            return Fail("unified Redo lost the last Screen edit");
        timeline.DidRedo(2, 0);
        if (timeline.NextRedo() != Timeline::Kind::None)
            return Fail("unified Redo offered an exhausted operation");
        timeline.DidUndo(1, 1);
        timeline.ObserveScreen(2, 0); // New Screen edit discards Redo.
        if (timeline.NextRedo() != Timeline::Kind::None)
            return Fail("new Screen edit retained a stale Redo branch");
        timeline.ObserveScreen(0, 0); // Out-of-band history reset.
        if (timeline.NextUndo() != Timeline::Kind::None)
            return Fail("out-of-band history reset retained stale routing");
        timeline.Reset(1, 0);
        timeline.RecordFlowRoute();
        timeline.DidUndo(1, 0);
        timeline.ObserveScreen(2, 0);
        if (timeline.NextRedo() != Timeline::Kind::None)
            return Fail("Screen edit after route Undo did not truncate route Redo");
    }

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

    // UI-027: panel sizing never edits ScreenDocument geometry or allows
    // independent side panels to consume the minimum live canvas area.
    renegade::studio::ScreenEditorPanelLayout panels;
    panels.Clamp(1920.0f);
    if (panels.left != 260.0f || panels.right != 360.0f ||
        panels.CanvasWidth(1920.0f) != 1272.0f)
        return Fail("screen editor panel defaults did not preserve canvas bounds");
    panels.DragLeft(347.0f, 1920.0f);
    panels.DragRight(1493.0f, 1920.0f);
    if (panels.left != 340.0f || panels.right != 420.0f ||
        panels.CanvasWidth(1920.0f) != 1132.0f)
        return Fail("screen editor panel dividers did not resize independently");
    panels.Clamp(1024.0f);
    if (panels.left != 276.0f || panels.right != 340.0f ||
        panels.CanvasWidth(1024.0f) != 380.0f)
        return Fail("screen editor panel widths overlapped the live canvas");
    panels.DragLeft(3000.0f, 1024.0f);
    panels.DragRight(-100.0f, 1024.0f);
    if (panels.left != 276.0f || panels.right != 340.0f)
        return Fail("screen editor panel drag exceeded viewport budget");
    panels.Clamp(800.0f);
    if (panels.left != 200.0f || panels.right != 340.0f ||
        panels.CanvasWidth(800.0f) != 232.0f)
        return Fail("screen editor layout failed on a smaller desktop width");

    // UI-026: authoring grid uses design-space coordinates, not viewport pixels.
    using renegade::studio::ScreenGridGesture;
    using renegade::studio::SnapScreenGridCoordinate;
    using renegade::studio::SnapScreenGridRect;
    if (SnapScreenGridCoordinate(23.5f, 16.0f) != 16.0f ||
        SnapScreenGridCoordinate(26.0f, 16.0f) != 32.0f ||
        SnapScreenGridCoordinate(26.0f, 0.0f) != 26.0f)
    {
        return Fail("design grid coordinate snapping is incorrect");
    }
    // Near-sibling guides take precedence over grid steps independently by
    // axis. Centre-to-centre alignment and dragged resize edges are included.
    using renegade::studio::SnapScreenToSiblings;
    const ScreenRect alignedRaw{94.0f, 155.0f, 60.0f, 30.0f};
    const ScreenRect alignedFallback{96.0f, 160.0f, 60.0f, 30.0f};
    const auto aligned = SnapScreenToSiblings(
        alignedRaw, alignedFallback, ScreenGridGesture::Move,
        {{100.0f, 100.0f, 160.0f, 100.0f}}, 8.0f, 8.0f,
        1280.0f, 720.0f);
    if (!aligned.guideX || !aligned.guideY ||
        aligned.rect.x != 100.0f || aligned.rect.y != 150.0f ||
        aligned.guideCoordinateX != 100.0f ||
        aligned.guideCoordinateY != 150.0f)
        return Fail("element alignment did not override the grid on both axes");

    const auto centered = SnapScreenToSiblings(
        {144.0f, 450.0f, 60.0f, 30.0f},
        {144.0f, 448.0f, 60.0f, 30.0f},
        ScreenGridGesture::Move,
        {{100.0f, 100.0f, 160.0f, 100.0f}}, 8.0f, 8.0f,
        1280.0f, 720.0f);
    if (!centered.guideX || centered.guideY ||
        centered.rect.x != 150.0f || centered.rect.y != 448.0f ||
        centered.guideCoordinateX != 180.0f)
        return Fail("element centre alignment or unmatched grid fallback failed");

    const auto snappedRight = SnapScreenToSiblings(
        {50.0f, 50.0f, 148.0f, 64.0f},
        {50.0f, 50.0f, 142.0f, 62.0f},
        ScreenGridGesture::ResizeBottomRight,
        {{200.0f, 300.0f, 100.0f, 50.0f}}, 8.0f, 8.0f,
        1280.0f, 720.0f);
    if (!snappedRight.guideX || snappedRight.guideY ||
        snappedRight.rect.x != 50.0f ||
        snappedRight.rect.width != 150.0f ||
        snappedRight.rect.height != 62.0f)
        return Fail("right-edge sibling snap changed the opposite resize edge");

    const auto snappedLeft = SnapScreenToSiblings(
        {102.0f, 50.0f, 48.0f, 40.0f},
        {96.0f, 50.0f, 54.0f, 40.0f},
        ScreenGridGesture::ResizeTopLeft,
        {{100.0f, 300.0f, 100.0f, 40.0f}}, 8.0f, 8.0f,
        1280.0f, 720.0f);
    if (!snappedLeft.guideX || snappedLeft.guideY ||
        snappedLeft.rect.x != 100.0f ||
        snappedLeft.rect.width != 50.0f)
        return Fail("left-edge sibling snap moved the fixed right edge");

    const auto noPeers = SnapScreenToSiblings(
        alignedRaw, alignedFallback, ScreenGridGesture::Move, {},
        8.0f, 8.0f, 1280.0f, 720.0f);
    if (noPeers.guideX || noPeers.guideY ||
        noPeers.rect.x != alignedFallback.x ||
        noPeers.rect.y != alignedFallback.y)
        return Fail("unmatched sibling snap failed to preserve grid fallback");

    const ScreenRect inputRect{23.0f, 29.0f, 101.0f, 53.0f};
    const auto gridMove = SnapScreenGridRect(
        inputRect, ScreenGridGesture::Move, 16.0f, 1280.0f, 720.0f);
    const auto gridTL = SnapScreenGridRect(
        inputRect, ScreenGridGesture::ResizeTopLeft, 16.0f, 1280.0f, 720.0f);
    const auto gridTR = SnapScreenGridRect(
        inputRect, ScreenGridGesture::ResizeTopRight, 16.0f, 1280.0f, 720.0f);
    const auto gridBL = SnapScreenGridRect(
        inputRect, ScreenGridGesture::ResizeBottomLeft, 16.0f, 1280.0f, 720.0f);
    const auto gridBR = SnapScreenGridRect(
        inputRect, ScreenGridGesture::ResizeBottomRight, 16.0f, 1280.0f, 720.0f);
    if (gridMove.x != 16.0f || gridMove.y != 32.0f ||
        gridMove.width != 101.0f || gridMove.height != 53.0f ||
        gridTL.x != 16.0f || gridTL.y != 32.0f ||
        gridTL.x + gridTL.width != 124.0f ||
        gridTL.y + gridTL.height != 82.0f ||
        gridTR.x != 23.0f || gridTR.y != 32.0f ||
        gridTR.x + gridTR.width != 128.0f ||
        gridTR.y + gridTR.height != 82.0f ||
        gridBL.x != 16.0f || gridBL.y != 29.0f ||
        gridBL.x + gridBL.width != 124.0f ||
        gridBL.y + gridBL.height != 80.0f ||
        gridBR.x != 23.0f || gridBR.y != 29.0f ||
        gridBR.x + gridBR.width != 128.0f ||
        gridBR.y + gridBR.height != 80.0f)
    {
        return Fail("grid snap altered the wrong resize edge or dimension");
    }
    const auto boundedGrid = SnapScreenGridRect(
        {1250.0f, 690.0f, 40.0f, 40.0f},
        ScreenGridGesture::Move, 16.0f, 1280.0f, 720.0f);
    if (boundedGrid.x != 1240.0f || boundedGrid.y != 680.0f)
        return Fail("grid snapping permitted an out-of-canvas move");

    const StableId titleId = source.widgets[0].id;
    const StableId playId = source.widgets[1].id;
    const StableId quitId = source.widgets[2].id;
    const ScreenAnchors originalQuitAnchors = source.widgets[2].anchors;
    std::string error;
    if (!WriteScreenDocument(screenPath.generic_u8string(), source, error))
        return Fail("fixture write failed: " + error);

    // Gate 8C baseline: basic Inspector edits, anchored geometry, history and
    // transactional Save/Open remain intact under Gate 8D.
    ScreenAuthoringSession session;
    if (!session.Open(screenPath.generic_u8string(), projectId, error) ||
        session.IsDirty() || session.CanUndo() || session.CanRedo())
    {
        return Fail("Open did not establish a clean authoring session: " + error);
    }

    ScreenWidgetAuthoringEdit titleEdit;
    titleEdit.name = "Main Title";
    titleEdit.text = "THE PAGANACHT";
    titleEdit.resolvedRect = {220.0f, 90.0f, 840.0f, 110.0f};
    titleEdit.visible = true;
    titleEdit.enabled = true;
    if (!session.UpdateWidget(titleId, titleEdit, error) ||
        !session.IsDirty() || !session.CanUndo())
    {
        return Fail("absolute widget edit did not enter history: " + error);
    }

    ScreenWidgetAuthoringEdit quitEdit;
    quitEdit.name = "Exit Game";
    quitEdit.text = "EXIT";
    quitEdit.resolvedRect = {420.0f, 450.0f, 440.0f, 80.0f};
    quitEdit.visible = true;
    quitEdit.enabled = true;
    if (!session.UpdateWidget(quitId, quitEdit, error))
        return Fail("anchored widget edit failed: " + error);
    ScreenRect movedQuit;
    if (!ResolveScreenWidgetRect(session.Document(), quitId, movedQuit, error) ||
        movedQuit.x != 420.0f || movedQuit.y != 450.0f ||
        movedQuit.width != 440.0f || movedQuit.height != 80.0f ||
        session.FindWidget(quitId)->layoutMode != ScreenLayoutMode::Anchored ||
        session.FindWidget(quitId)->anchors.minimumX != originalQuitAnchors.minimumX ||
        session.FindWidget(quitId)->anchors.maximumX != originalQuitAnchors.maximumX)
    {
        return Fail("anchored edit did not preserve anchors/resolved layout");
    }

    const std::size_t undoBeforeNoOp = session.UndoCount();
    if (!session.UpdateWidget(quitId, quitEdit, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("identical edit polluted Screen history");
    }
    ScreenWidgetAuthoringEdit invalidEdit = quitEdit;
    invalidEdit.resolvedRect.width = -1.0f;
    if (session.UpdateWidget(quitId, invalidEdit, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("invalid basic edit entered Screen history");
    }
    if (!session.Undo(error) || session.FindWidget(quitId)->text != "QUIT" ||
        !session.Redo(error) || session.FindWidget(quitId)->text != "EXIT")
    {
        return Fail("basic Undo/Redo did not restore exact Screen snapshots: " + error);
    }
    if (!session.Save(error) || session.IsDirty())
        return Fail("baseline Save did not mark the Screen clean: " + error);

    ScreenAuthoringSession authored;
    if (!authored.Open(screenPath.generic_u8string(), projectId, error) ||
        authored.FindWidget(titleId)->text != "THE PAGANACHT" ||
        authored.FindWidget(quitId)->text != "EXIT")
    {
        return Fail("baseline Save/Open did not preserve authored content: " + error);
    }

    // UI-10: a fresh button must own its outcome; otherwise a new Button
    // silently shares the first route and Inspector Open Screen rejects it.
    {
        const fs::path buttonPath =
            temporary.path / "Content/Screens/UI10Buttons.renegade-screen";
        if (!WriteScreenDocument(buttonPath.generic_u8string(),
                source, error))
            return Fail("UI-10 button fixture write failed: " + error);
        ScreenAuthoringSession buttons;
        if (!buttons.Open(buttonPath.generic_u8string(), projectId, error))
            return Fail("UI-10 button session open failed: " + error);
        ScreenWidget button;
        button.kind = ScreenWidgetKind::Button;
        button.name = "Settings";
        button.text = "SETTINGS";
        button.rect = {180.0f, 280.0f, 260.0f, 64.0f};
        button.style = MakeScreenWidgetStyleTemplate(
            ScreenWidgetKind::Button, button.rect.height);
        StableId first;
        const auto beforeActions = buttons.Document().actions.size();
        const auto beforeUndo = buttons.UndoCount();
        if (!buttons.CreateButtonWithUniqueAction(
                button, first, error) ||
            buttons.Document().actions.size() != beforeActions + 1 ||
            buttons.UndoCount() != beforeUndo + 1 ||
            buttons.FindWidget(first) == nullptr)
            return Fail("UI-10 Button/action did not commit in one step: " + error);
        const auto ownedAction = buttons.FindWidget(first)->actionId;
        if (ownedAction.empty() ||
            ownedAction == source.widgets[1].actionId ||
            ownedAction == source.widgets[2].actionId)
            return Fail("UI-10 Button silently reused an existing outcome");

        StableId second;
        if (!buttons.CreateButtonWithUniqueAction(
                button, second, error) || second == first ||
            buttons.FindWidget(second)->actionId == ownedAction)
            return Fail("UI-10 subsequent Button reused an outcome: " + error);
        const std::string secondAction = buttons.FindWidget(second)->actionId;

        const auto beforeDuplicateUndo = buttons.UndoCount();
        StableId duplicate;
        if (!buttons.DuplicateWidget(first, duplicate, error) ||
            buttons.FindWidget(duplicate) == nullptr ||
            buttons.FindWidget(duplicate)->actionId == ownedAction ||
            buttons.FindWidget(duplicate)->actionId == secondAction ||
            buttons.UndoCount() != beforeDuplicateUndo + 1)
            return Fail("UI-10 duplicated Button shares action or lost single Undo");
        if (!buttons.Undo(error) ||
            buttons.FindWidget(duplicate) != nullptr ||
            !buttons.Redo(error) ||
            buttons.FindWidget(duplicate) == nullptr)
            return Fail("UI-10 Button/owned-action Undo/Redo failed: " + error);
        // A creator can explicitly keep an existing Button's governed action
        // identity yet suppress Flow dispatch after its local interactions.
        if(!buttons.SetButtonLocalOnly(first,true,error)||!buttons.FindWidget(first)->localOnly||
           buttons.FindWidget(second)->localOnly||buttons.FindWidget(duplicate)->localOnly)
            return Fail("local-only Button mode did not remain per instance: "+error);
        if(!buttons.Undo(error)||buttons.FindWidget(first)->localOnly||
           !buttons.Redo(error)||!buttons.FindWidget(first)->localOnly)
            return Fail("local-only Button mode lacked atomic Undo/Redo: "+error);
        const auto beforeInvalid=buttons.UndoCount();
        if(buttons.SetButtonLocalOnly(source.widgets[0].id,true,error)||
           buttons.UndoCount()!=beforeInvalid)
            return Fail("non-Button was able to suppress Flow");
        if (!buttons.Save(error))
            return Fail("UI-10 saved Buttons with owned outcomes failed: " + error);
        ScreenAuthoringSession reopenedButtons;
        if (!reopenedButtons.Open(
                buttonPath.generic_u8string(), projectId, error) ||
            reopenedButtons.FindWidget(first)->actionId != ownedAction ||
            reopenedButtons.FindWidget(second)->actionId != secondAction ||
            !reopenedButtons.FindWidget(first)->localOnly ||
            reopenedButtons.FindWidget(second)->localOnly ||
            reopenedButtons.FindWidget(duplicate)->localOnly)
            return Fail("UI-10 owned outcomes did not persist: " + error);

        // Guard the 64-action schema limit without partially creating a Button.
        ScreenDocument full = source;
        while (full.actions.size() < 64)
            full.actions.push_back(
                {"extra_action_" + std::to_string(full.actions.size())});
        const fs::path fullPath =
            temporary.path / "Content/Screens/UI10Full.renegade-screen";
        if (!WriteScreenDocument(
                fullPath.generic_u8string(), full, error))
            return Fail("UI-10 full-action fixture invalid: " + error);
        ScreenAuthoringSession maxActions;
        StableId refused;
        if (!maxActions.Open(fullPath.generic_u8string(), projectId, error) ||
            maxActions.CreateButtonWithUniqueAction(
                button, refused, error) ||
            !refused.empty() || maxActions.IsDirty())
            return Fail("UI-10 exceeded the 64-action Screen limit");
    }

    // Gate 8D foundation creator transactions.
    ScreenWidget creatorText;
    creatorText.kind = ScreenWidgetKind::Text;
    creatorText.name = "Subtitle";
    creatorText.rect = {300.0f, 220.0f, 680.0f, 70.0f};
    creatorText.visible = true;
    creatorText.enabled = false;
    creatorText.text = "CREATOR TEXT";
    creatorText.style = MakeScreenWidgetStyleTemplate(
        creatorText.kind, creatorText.rect.height);
    StableId creatorTextId;
    if (!authored.CreateWidget(creatorText, false, creatorTextId, error) ||
        !IsValidStableId(creatorTextId))
    {
        return Fail("Text creation failed: " + error);
    }

    ScreenWidget creatorButton;
    creatorButton.kind = ScreenWidgetKind::Button;
    creatorButton.name = "Continue";
    creatorButton.rect = {440.0f, 550.0f, 400.0f, 76.0f};
    creatorButton.visible = true;
    creatorButton.enabled = true;
    creatorButton.text = "CONTINUE";
    creatorButton.actionId = RuntimeScreenPlayAction;
    creatorButton.style = MakeScreenWidgetStyleTemplate(
        creatorButton.kind, creatorButton.rect.height);
    StableId creatorButtonId;
    if (!authored.CreateWidget(creatorButton, false, creatorButtonId, error) ||
        authored.Document().focusOrder.back() != creatorButtonId)
    {
        return Fail("Button creation did not establish focus identity: " + error);
    }

    StableId duplicateButtonId;
    if (!authored.DuplicateWidget(creatorButtonId, duplicateButtonId, error) ||
        duplicateButtonId == creatorButtonId ||
        authored.FindWidget(duplicateButtonId) == nullptr ||
        authored.FindWidget(duplicateButtonId)->actionId.empty() ||
        authored.FindWidget(duplicateButtonId)->actionId == RuntimeScreenPlayAction ||
        authored.FindWidget(creatorButtonId)->actionId != RuntimeScreenPlayAction)
    {
        return Fail("Button duplication must preserve original identity and allocate a new action: " + error);
    }
    auto sourceFocus = std::find(
        authored.Document().focusOrder.begin(), authored.Document().focusOrder.end(),
        creatorButtonId);
    auto copyFocus = std::find(
        authored.Document().focusOrder.begin(), authored.Document().focusOrder.end(),
        duplicateButtonId);
    if (sourceFocus == authored.Document().focusOrder.end() ||
        copyFocus != sourceFocus + 1)
    {
        return Fail("duplicated Button was not inserted after source focus entry");
    }

    if (!authored.MoveWidgetToBack(duplicateButtonId, error) ||
        authored.Document().widgets.front().id != duplicateButtonId ||
        !authored.MoveWidgetToFront(duplicateButtonId, error) ||
        authored.Document().widgets.back().id != duplicateButtonId)
    {
        return Fail("layer transactions did not preserve authored order: " + error);
    }

    const auto layerIndex = [&authored](const StableId& id)
    {
        const auto& widgets = authored.Document().widgets;
        return static_cast<std::size_t>(std::distance(
            widgets.begin(), std::find_if(widgets.begin(), widgets.end(),
                [&id](const ScreenWidget& widget) { return widget.id == id; })));
    };
    const std::size_t lastLayer = authored.Document().widgets.size() - 1;
    const std::vector<StableId> originalFocus = authored.Document().focusOrder;
    if (!authored.MoveWidgetBackward(duplicateButtonId, error) ||
        layerIndex(duplicateButtonId) != lastLayer - 1 ||
        !authored.Undo(error) || layerIndex(duplicateButtonId) != lastLayer ||
        !authored.Redo(error) || layerIndex(duplicateButtonId) != lastLayer - 1 ||
        !authored.MoveWidgetForward(duplicateButtonId, error) ||
        layerIndex(duplicateButtonId) != lastLayer ||
        authored.Document().focusOrder != originalFocus)
    {
        return Fail("one-step layer movement or Undo/Redo changed order/focus: " + error);
    }
    const std::size_t undoBeforeBoundary = authored.UndoCount();
    if (!authored.MoveWidgetForward(duplicateButtonId, error) ||
        authored.UndoCount() != undoBeforeBoundary)
    {
        return Fail("layer boundary must not create a redundant history mutation");
    }

    ScreenWidget creatorImage;
    creatorImage.kind = ScreenWidgetKind::Image;
    creatorImage.name = "Artwork";
    creatorImage.rect = {80.0f, 80.0f, 320.0f, 180.0f};
    creatorImage.visible = false;
    creatorImage.enabled = false;
    creatorImage.style = MakeScreenWidgetStyleTemplate(
        creatorImage.kind, creatorImage.rect.height);
    creatorImage.style.normal.background = {60, 70, 80, 100};
    creatorImage.style.hover = creatorImage.style.normal;
    creatorImage.style.pressed = creatorImage.style.normal;
    creatorImage.style.focused = creatorImage.style.normal;
    creatorImage.style.disabled = creatorImage.style.normal;
    StableId creatorImageId;
    if (!authored.CreateWidget(creatorImage, true, creatorImageId, error) ||
        authored.Document().widgets.front().id != creatorImageId ||
        authored.FindWidget(creatorImageId)->visible ||
        !authored.FindWidget(creatorImageId)->resourcePath.empty())
    {
        return Fail("hidden/unassigned Image creation failed: " + error);
    }

    if (!authored.DeleteWidget(duplicateButtonId, error) ||
        authored.FindWidget(duplicateButtonId) != nullptr ||
        std::find(authored.Document().focusOrder.begin(),
            authored.Document().focusOrder.end(), duplicateButtonId) !=
            authored.Document().focusOrder.end())
    {
        return Fail("deletion did not remove Button/focus identity: " + error);
    }
    if (!authored.Undo(error) || authored.FindWidget(duplicateButtonId) == nullptr ||
        !authored.Redo(error) || authored.FindWidget(duplicateButtonId) != nullptr)
    {
        return Fail("creator transaction Undo/Redo failed: " + error);
    }

    // Advanced style/state/typography transaction.
    auto styleEdit = CreatorEditFor(*authored.FindWidget(creatorButtonId));
    styleEdit.style.normal.background = {21, 22, 23, 24};
    styleEdit.style.hover.background = {1, 2, 3, 4};
    styleEdit.style.pressed.foreground = {5, 6, 7, 8};
    styleEdit.style.focused.imageTint = {9, 10, 11, 12};
    styleEdit.style.disabled.imageResourcePath = "Content/UI/disabled.png";
    styleEdit.style.borderColor = {13, 14, 15, 16};
    styleEdit.style.borderWidth = 3.0f;
    styleEdit.style.cornerRadius = 11.0f;
    styleEdit.style.opacity = 0.75f;
    styleEdit.style.text.fontSize = 33.0f;
    styleEdit.style.text.characterSpacing = 1.5f;
    styleEdit.style.text.lineSpacing = 2.5f;
    styleEdit.style.text.softness = 0.25f;
    styleEdit.style.text.bolden = 0.35f;
    styleEdit.style.text.shadowColor = {17, 18, 19, 20};
    styleEdit.style.text.shadowOffsetX = 2.0f;
    styleEdit.style.text.shadowOffsetY = 4.0f;
    styleEdit.style.text.shadowSoftness = 0.45f;
    styleEdit.style.text.shadowBolden = 0.55f;
    styleEdit.style.text.horizontalAlignment = ScreenHorizontalAlignment::Right;
    styleEdit.style.text.verticalAlignment = ScreenVerticalAlignment::Bottom;
    styleEdit.style.text.wrap = true;
    if (!authored.UpdateWidgetCreatorFields(creatorButtonId, styleEdit, error))
        return Fail("advanced style/typography transaction failed: " + error);
    const auto* styled = authored.FindWidget(creatorButtonId);
    if (!styled || styled->style.normal.background.red != 21 ||
        styled->style.hover.background.green != 2 ||
        styled->style.pressed.foreground.blue != 7 ||
        styled->style.focused.imageTint.alpha != 12 ||
        styled->style.disabled.imageResourcePath != "Content/UI/disabled.png" ||
        styled->style.borderWidth != 3.0f ||
        styled->style.cornerRadius != 11.0f ||
        styled->style.opacity != 0.75f ||
        styled->style.text.fontSize != 33.0f ||
        styled->style.text.horizontalAlignment != ScreenHorizontalAlignment::Right ||
        styled->style.text.verticalAlignment != ScreenVerticalAlignment::Bottom ||
        !styled->style.text.wrap)
    {
        return Fail("advanced authored style/typography values were not exact");
    }

    // Parent/layout authority preserves resolved geometry and rejects cycles.
    ScreenRect textBeforeParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId,
            textBeforeParent, error))
        return Fail("could not resolve Text before reparent: " + error);
    auto textParentEdit = CreatorEditFor(*authored.FindWidget(creatorTextId));
    textParentEdit.parentId = creatorImageId;
    textParentEdit.layoutMode = ScreenLayoutMode::Anchored;
    textParentEdit.anchors.minimumX = 0.25f;
    textParentEdit.anchors.minimumY = 0.25f;
    textParentEdit.anchors.maximumX = 0.25f;
    textParentEdit.anchors.maximumY = 0.25f;
    if (!authored.UpdateWidgetCreatorFields(creatorTextId, textParentEdit, error))
        return Fail("legal anchored reparent failed: " + error);
    ScreenRect textAfterParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId,
            textAfterParent, error) || !SameRect(textBeforeParent, textAfterParent))
    {
        return Fail("reparent/layout-mode change moved resolved geometry");
    }

    auto selfParent = CreatorEditFor(*authored.FindWidget(creatorImageId));
    selfParent.parentId = creatorImageId;
    if (authored.UpdateWidgetCreatorFields(creatorImageId, selfParent, error) ||
        error.find("cannot parent itself") == std::string::npos)
    {
        return Fail("self-parent mutation was not rejected by authority");
    }
    auto descendantParent = CreatorEditFor(*authored.FindWidget(creatorImageId));
    descendantParent.parentId = creatorTextId;
    if (authored.UpdateWidgetCreatorFields(creatorImageId, descendantParent, error) ||
        error.find("descendants") == std::string::npos)
    {
        return Fail("descendant-parent cycle was not rejected by authority");
    }

    // Add a Button child so reusable subtree duplication proves both parent-ID
    // remapping and focus-order remapping.
    ScreenRect buttonBeforeParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorButtonId,
            buttonBeforeParent, error))
        return Fail("could not resolve Button before reparent: " + error);
    auto buttonParentEdit = CreatorEditFor(*authored.FindWidget(creatorButtonId));
    buttonParentEdit.parentId = creatorImageId;
    if (!authored.UpdateWidgetCreatorFields(creatorButtonId, buttonParentEdit, error))
        return Fail("component Button reparent failed: " + error);
    ScreenRect buttonAfterParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorButtonId,
            buttonAfterParent, error) || !SameRect(buttonBeforeParent, buttonAfterParent))
    {
        return Fail("component Button reparent changed resolved geometry");
    }

    // A layer command inside a container must not reorder the other parents.
    const auto rootLayerIds = [&authored]()
    {
        std::vector<StableId> ids;
        for (const auto& widget : authored.Document().widgets)
            if (widget.parentId.empty()) ids.push_back(widget.id);
        return ids;
    };
    const auto rootOrder = rootLayerIds();
    if (!(layerIndex(creatorTextId) < layerIndex(creatorButtonId)) ||
        !authored.MoveWidgetBackward(creatorButtonId, error) ||
        !(layerIndex(creatorButtonId) < layerIndex(creatorTextId)) ||
        rootLayerIds() != rootOrder ||
        !authored.Undo(error) ||
        !(layerIndex(creatorTextId) < layerIndex(creatorButtonId)) ||
        !authored.Redo(error) ||
        !(layerIndex(creatorButtonId) < layerIndex(creatorTextId)) ||
        !authored.MoveWidgetForward(creatorButtonId, error) ||
        !(layerIndex(creatorTextId) < layerIndex(creatorButtonId)) ||
        rootLayerIds() != rootOrder)
    {
        return Fail("nested one-step layer ordering changed another container: " + error);
    }

    // A layer drag must be one transaction, preserve sibling/parent boundaries,
    // and restore exactly through Undo/Redo without rewriting focus order.
    const std::size_t beforeDragHistory = authored.UndoCount();
    const auto focusBeforeDrag = authored.Document().focusOrder;
    if (!authored.MoveWidgetAdjacentTo(
            creatorButtonId, creatorTextId, false, error) ||
        !(layerIndex(creatorButtonId) < layerIndex(creatorTextId)) ||
        authored.UndoCount() != beforeDragHistory + 1 ||
        rootLayerIds() != rootOrder ||
        authored.Document().focusOrder != focusBeforeDrag ||
        !authored.Undo(error) ||
        !(layerIndex(creatorTextId) < layerIndex(creatorButtonId)) ||
        !authored.Redo(error) ||
        !(layerIndex(creatorButtonId) < layerIndex(creatorTextId)) ||
        !authored.MoveWidgetAdjacentTo(
            creatorButtonId, creatorTextId, true, error) ||
        !(layerIndex(creatorTextId) < layerIndex(creatorButtonId)))
    {
        return Fail("layer drag transaction/undo/redo changed sibling or focus order: " + error);
    }
    const std::size_t noOpHistory = authored.UndoCount();
    if (!authored.MoveWidgetAdjacentTo(
            creatorButtonId, creatorTextId, true, error) ||
        authored.UndoCount() != noOpHistory ||
        authored.MoveWidgetAdjacentTo(
            creatorButtonId, creatorImageId, false, error) ||
        error.find("same parent") == std::string::npos ||
        authored.UndoCount() != noOpHistory ||
        rootLayerIds() != rootOrder)
    {
        return Fail("layer drag no-op/cross-parent rejection changed authoring history");
    }

    // A copied Button must target the copied child, not its source sibling.
    if (!authored.UpdateWidgetInteractions(creatorButtonId,
            {{ScreenInteractionEvent::Activate, ScreenLocalActionKind::SetVisible,
                creatorTextId, false, {}}}, error))
        return Fail("component source interaction setup failed: " + error);
    const std::size_t widgetsBeforeTreeCopy = authored.Document().widgets.size();
    const std::size_t undoBeforeTreeCopy = authored.UndoCount();
    StableId duplicateRootId;
    if (!authored.DuplicateWidgetTree(creatorImageId, duplicateRootId, error) ||
        !IsValidStableId(duplicateRootId) || duplicateRootId == creatorImageId ||
        authored.Document().widgets.size() != widgetsBeforeTreeCopy + 3)
    {
        return Fail("nested reusable component duplication failed: " + error);
    }
    const auto* duplicateRoot = authored.FindWidget(duplicateRootId);
    if (!duplicateRoot || !duplicateRoot->parentId.empty())
        return Fail("duplicated component root lost external-parent contract");

    StableId duplicateTextId;
    StableId duplicateComponentButtonId;
    for (const auto& widget : authored.Document().widgets)
    {
        if (widget.parentId != duplicateRootId) continue;
        if (widget.kind == ScreenWidgetKind::Text) duplicateTextId = widget.id;
        if (widget.kind == ScreenWidgetKind::Button)
            duplicateComponentButtonId = widget.id;
    }
    if (!IsValidStableId(duplicateTextId) ||
        !IsValidStableId(duplicateComponentButtonId) ||
        duplicateTextId == creatorTextId ||
        duplicateComponentButtonId == creatorButtonId ||
        authored.FindWidget(duplicateComponentButtonId)->actionId ==
            authored.FindWidget(creatorButtonId)->actionId ||
        authored.FindWidget(duplicateComponentButtonId)->actionId.empty() ||
        authored.FindWidget(duplicateComponentButtonId)->interactions.size()!=1 ||
        authored.FindWidget(duplicateComponentButtonId)->interactions[0].targetId!=duplicateTextId ||
        authored.FindWidget(creatorButtonId)->interactions[0].targetId!=creatorTextId ||
        std::none_of(authored.Document().actions.begin(),authored.Document().actions.end(),
            [&](const ScreenAction& a){return a.id==authored.FindWidget(duplicateComponentButtonId)->actionId;}))
    {
        return Fail("component descendants were not remapped with fresh IDs/action");
    }
    sourceFocus = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), creatorButtonId);
    copyFocus = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), duplicateComponentButtonId);
    if (sourceFocus == authored.Document().focusOrder.end() ||
        copyFocus != sourceFocus + 1)
    {
        return Fail("component Button copy was not inserted after source focus entry");
    }

    if (!authored.Undo(error) || authored.FindWidget(duplicateRootId) != nullptr ||
        authored.UndoCount() != undoBeforeTreeCopy ||
        !authored.Redo(error) || authored.FindWidget(duplicateRootId) == nullptr)
    {
        return Fail("component duplication did not round-trip through Undo/Redo: " + error);
    }

    // Symbolic action identity authoring; routing remains Story Flow-owned.
    if (!authored.AddAction("open_audio_settings", error))
        return Fail("custom action creation failed: " + error);
    if (!authored.RenameAction(RuntimeScreenPlayAction, "start_game", error))
        return Fail("action rename failed: " + error);
    if (authored.FindWidget(playId)->actionId != "start_game" ||
        authored.FindWidget(creatorButtonId)->actionId != "start_game" ||
        authored.FindWidget(duplicateComponentButtonId)->actionId == "start_game" ||
        authored.FindWidget(duplicateComponentButtonId)->actionId.empty())
    {
        return Fail("action rename did not update every Button reference atomically");
    }
    if (authored.DeleteAction("start_game", error) ||
        error.find("still referenced") == std::string::npos)
    {
        return Fail("referenced action deletion was not rejected");
    }
    if (!authored.DeleteAction("open_audio_settings", error))
        return Fail("unreferenced action deletion failed: " + error);

    // Focus order authoring is independent of visual layer order.
    const auto focusBeforeMove = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), creatorButtonId);
    if (focusBeforeMove == authored.Document().focusOrder.begin() ||
        focusBeforeMove == authored.Document().focusOrder.end())
    {
        return Fail("fixture Button did not have a movable focus position");
    }
    const StableId precedingFocusId = *(focusBeforeMove - 1);
    if (!authored.MoveButtonFocusEarlier(creatorButtonId, error))
        return Fail("focus move earlier failed: " + error);
    const auto focusAfterEarlier = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), creatorButtonId);
    if (focusAfterEarlier == authored.Document().focusOrder.end() ||
        focusAfterEarlier + 1 == authored.Document().focusOrder.end() ||
        *(focusAfterEarlier + 1) != precedingFocusId)
    {
        return Fail("focus move earlier did not swap adjacent Buttons");
    }
    if (!authored.MoveButtonFocusLater(creatorButtonId, error))
        return Fail("focus move later failed: " + error);

    ScreenRect directBefore;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId, directBefore, error)) return Fail("direct geometry fixture failed: " + error);
    const std::size_t directUndo = authored.UndoCount();
    const auto* directWidget = authored.FindWidget(creatorTextId);
    authored.BeginCoalescedEdit();
    ScreenWidgetAuthoringEdit directEdit{directWidget->name, directWidget->text, {directBefore.x + 20.0f, directBefore.y + 10.0f, directBefore.width + 20.0f, directBefore.height + 10.0f}, directWidget->visible, directWidget->enabled};
    if (!authored.UpdateWidget(creatorTextId, directEdit, error)) return Fail("direct frame one failed: " + error);
    directEdit.resolvedRect.x += 20.0f;
    if (!authored.UpdateWidget(creatorTextId, directEdit, error)) return Fail("direct frame two failed: " + error);
    authored.EndCoalescedEdit();
    if (authored.UndoCount() != directUndo + 1) return Fail("direct gesture polluted Undo history");
    if (!authored.Undo(error)) return Fail("direct Undo failed: " + error);
    ScreenRect directUndone;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId, directUndone, error) || !SameRect(directBefore, directUndone)) return Fail("direct Undo geometry mismatch");
    if (!authored.Redo(error)) return Fail("direct Redo failed: " + error);

    if (!authored.Save(error) || authored.IsDirty())
        return Fail("advanced Gate 8D state did not save transactionally: " + error);

    ScreenAuthoringSession persisted;
    if (!persisted.Open(screenPath.generic_u8string(), projectId, error))
        return Fail("advanced Gate 8D Screen did not reopen: " + error);
    const auto* persistedButton = persisted.FindWidget(creatorButtonId);
    const auto* persistedText = persisted.FindWidget(creatorTextId);
    const auto* persistedDuplicateButton =
        persisted.FindWidget(duplicateComponentButtonId);
    if (!persistedButton || !persistedText || !persistedDuplicateButton ||
        !persisted.FindWidget(duplicateRootId) ||
        persistedButton->style.hover.background.green != 2 ||
        persistedButton->style.text.fontSize != 33.0f ||
        persistedButton->actionId != "start_game" ||
        persistedButton->parentId != creatorImageId ||
        persistedText->parentId != creatorImageId ||
        persistedText->layoutMode != ScreenLayoutMode::Anchored ||
        persistedDuplicateButton->parentId != duplicateRootId ||
        persistedDuplicateButton->actionId == "start_game" ||
        persistedDuplicateButton->actionId.empty() ||
        std::find(persisted.Document().focusOrder.begin(),
            persisted.Document().focusOrder.end(), duplicateComponentButtonId) ==
            persisted.Document().focusOrder.end())
    {
        return Fail("advanced style/binding/component/action/focus state did not persist");
    }


    // Batch selection contract: anchored geometry, stable identities, independent
    // actions and failure atomicity are verified through real persistence/history.
    const auto batchPath=screenPath.parent_path() / "Batch.renegade-screen";
    auto batchDoc=MakeDocument(projectId);
    batchDoc.usage=ScreenUsage::Hud;
    if(!WriteScreenDocument(batchPath.generic_u8string(),batchDoc,error))
        return Fail("batch fixture write: "+error);
    ScreenAuthoringSession batch;
    if(!batch.Open(batchPath.generic_u8string(),projectId,error))return Fail(error);
    const auto a=batchDoc.widgets[0].id,b=batchDoc.widgets[1].id,c=batchDoc.widgets[2].id;
    ScreenRect beforeA,beforeC;
    if(!ResolveScreenWidgetRect(batch.Document(),a,beforeA,error)||
       !ResolveScreenWidgetRect(batch.Document(),c,beforeC,error))return Fail(error);
    const auto undoBatch=batch.UndoCount();
    if(!batch.TranslateWidgets({a,c},13,7,error)||batch.UndoCount()!=undoBatch+1)
        return Fail("batch movement not atomic: "+error);
    ScreenRect movedA,movedC;
    if(!ResolveScreenWidgetRect(batch.Document(),a,movedA,error)||
       !ResolveScreenWidgetRect(batch.Document(),c,movedC,error)||
       movedA.x!=beforeA.x+13||movedC.x!=beforeC.x+13||movedC.y!=beforeC.y+7)
        return Fail("anchored multi-move geometry incorrect");
    if(!batch.Undo(error))return Fail(error);
    StableId group;
    if(!batch.GroupWidgets({a,c},group,error))return Fail("group: "+error);
    ScreenRect groupedA,groupedC;
    if(!ResolveScreenWidgetRect(batch.Document(),a,groupedA,error)||
       !ResolveScreenWidgetRect(batch.Document(),c,groupedC,error)||
       !SameRect(groupedA,beforeA)||!SameRect(groupedC,beforeC))
        return Fail("group changed resolved child geometry");
    if(!batch.TranslateWidgets({group,a},10,0,error))return Fail(error);
    if(!ResolveScreenWidgetRect(batch.Document(),a,groupedA,error)||groupedA.x!=beforeA.x+10)
        return Fail("selected descendant moved twice");
    ScreenWidgetClipboard clip;
    if(!batch.CopyWidgets({group,a},clip,error)||clip.widgets.size()!=3)
        return Fail("clipboard subtree closure or deduplication failed");
    auto wrongProject=clip;wrongProject.projectId=GenerateStableId();
    std::vector<StableId> pasted;
    const auto beforeReject=batch.UndoCount();
    if(batch.PasteWidgets(wrongProject,20,20,pasted,error)||batch.UndoCount()!=beforeReject)
        return Fail("foreign clipboard changed document");
    if(!batch.PasteWidgets(clip,20,20,pasted,error)||pasted.size()!=1||
       batch.UndoCount()!=beforeReject+1)
        return Fail("subtree paste not atomic: "+error);
    const auto pasteRoot=pasted.front();
    if(pasteRoot==group||batch.Document().widgets.size()!=7)
        return Fail("paste identities/count incorrect");
    StableId pastedButton;
    for(const auto& w:batch.Document().widgets)
        if(w.parentId==pasteRoot&&w.kind==ScreenWidgetKind::Button)pastedButton=w.id;
    if(pastedButton.empty()||batch.FindWidget(pastedButton)->actionId==batch.FindWidget(c)->actionId)
        return Fail("pasted Button shares source outcome");
    if(!batch.Undo(error)||batch.FindWidget(pasteRoot)||!batch.Redo(error)||!batch.FindWidget(pasteRoot))
        return Fail("paste Undo/Redo did not retain identity");
    std::vector<StableId> ungrouped;
    if(!batch.UngroupWidget(group,ungrouped,error)||ungrouped.size()!=2||batch.FindWidget(group))
        return Fail("ungroup failed: "+error);
    if(!ResolveScreenWidgetRect(batch.Document(),a,groupedA,error)||groupedA.x!=beforeA.x+10)
        return Fail("ungroup changed child geometry");
    if(!batch.MoveWidgetsLayer({a,c},1,true,error))return Fail(error);
    auto indexOf=[&](const StableId& id){
        return std::distance(batch.Document().widgets.begin(),
            std::find_if(batch.Document().widgets.begin(),batch.Document().widgets.end(),
                [&](const auto& w){return w.id==id;}));};
    if(!(indexOf(b)<indexOf(a)&&indexOf(a)<indexOf(c)))
        return Fail("group front ordering did not preserve selected order");
    if(!batch.MoveWidgetsAdjacentTo({a,c},b,false,error)||
       !(indexOf(a)<indexOf(c)&&indexOf(c)<indexOf(b)))
        return Fail("batch sibling drag ordering failed");
    const auto rejectHistory=batch.UndoCount();
    if(batch.MoveWidgetsAdjacentTo({a,c},a,false,error)||batch.UndoCount()!=rejectHistory)
        return Fail("invalid batch reorder was not atomic");
    if(!batch.Save(error))return Fail("batch save: "+error);
    ScreenAuthoringSession batchReopened;
    if(!batchReopened.Open(batchPath.generic_u8string(),projectId,error)||
       !batchReopened.FindWidget(pasteRoot)||!batchReopened.FindWidget(pastedButton)||
       batchReopened.FindWidget(pastedButton)->actionId!=batch.FindWidget(pastedButton)->actionId)
        return Fail("batch save/reopen lost identities/actions");
    const auto deleteUndo=batch.UndoCount();
    if(!batch.DeleteWidgets({pasteRoot,pastedButton},error)||
       batch.FindWidget(pasteRoot)||batch.FindWidget(pastedButton)||
       batch.UndoCount()!=deleteUndo+1||!batch.Undo(error)||!batch.FindWidget(pastedButton))
        return Fail("batch recursive delete/history failed: "+error);


    // Every enabled preset is a real existing widget with persistent bindings.
    for(const auto preset : {ScreenWidgetPreset::Panel,ScreenWidgetPreset::Heading,
        ScreenWidgetPreset::Icon,ScreenWidgetPreset::Divider,ScreenWidgetPreset::Spacer,
        ScreenWidgetPreset::Counter,ScreenWidgetPreset::PlayerHealthBar,
        ScreenWidgetPreset::PlayerHealthText,ScreenWidgetPreset::PlayerAmmoText,ScreenWidgetPreset::Group})
    {
        StableId created;
        auto w=MakeScreenWidgetPreset(preset,1280,720);
        if(!batch.CreateWidget(w,false,created,error))return Fail("preset creation: "+error);
        if(preset==ScreenWidgetPreset::PlayerHealthBar &&
            (batch.FindWidget(created)->bindings.size()!=1||
             batch.FindWidget(created)->bindings.front().denominator.attributeId!="health.maximum"))
            return Fail("health preset lost explicit ratio binding");
        if(preset==ScreenWidgetPreset::PlayerAmmoText &&
            batch.FindWidget(created)->bindings.front().source.attributeId!="ammo.loaded")
            return Fail("ammo preset did not use authoritative loaded source");
    }
    if(!batch.Save(error)||!batchReopened.Reload(error)||
        batchReopened.Document().widgets.size()!=batch.Document().widgets.size())
        return Fail("preset persistence/reopen: "+error);
    StableId orderGroup;
    // Group contiguous roots and move the container behind/in front of siblings.
    if(!batch.GroupWidgets({a,c},orderGroup,error)||
       !batch.MoveWidgetsLayer({orderGroup},1,true,error))
        return Fail("container paint-order move: "+error);
    auto paintIndex=[&](const StableId& id){
        return std::distance(batch.Document().widgets.begin(),
            std::find_if(batch.Document().widgets.begin(),batch.Document().widgets.end(),
                [&](const auto& w){return w.id==id;}));};
    if(paintIndex(orderGroup)>=paintIndex(a)||paintIndex(a)+1!=paintIndex(c)||
       paintIndex(c)!=static_cast<std::ptrdiff_t>(batch.Document().widgets.size()-1))
        return Fail("container descendants did not move together in shared render/hit order");


    // Equal-cell container layout uses the same resolver as rendering/hit geometry.
    const auto layoutPath=screenPath.parent_path()/"Containers.renegade-screen";
    ScreenDocument layoutDoc;
    layoutDoc.envelope=CreateDocumentEnvelope(projectId,RuntimeScreenDocumentType,
        "Content/UI/Containers.renegade-screen","Container regression");
    layoutDoc.usage=ScreenUsage::Hud;
    auto container=MakeScreenWidgetPreset(ScreenWidgetPreset::Group,1280,720);
    container.id=GenerateStableId();container.rect={100,50,600,400};
    layoutDoc.widgets.push_back(container);
    std::vector<StableId> cellIds;
    for(int n=0;n<4;++n)
    {
        auto cell=MakeScreenWidgetPreset(ScreenWidgetPreset::Counter,1280,720);
        cell.id=GenerateStableId();cell.parentId=container.id;
        cell.rect={20,20+float(n)*60,100,40};cellIds.push_back(cell.id);
        layoutDoc.widgets.push_back(cell);
    }
    if(!WriteScreenDocument(layoutPath.generic_u8string(),layoutDoc,error))return Fail(error);
    ScreenAuthoringSession layouts;
    if(!layouts.Open(layoutPath.generic_u8string(),projectId,error))return Fail(error);
    ScreenContainerLayout flow;flow.mode=ScreenChildLayout::Horizontal;flow.padding=10;flow.gap=10;
    const auto layoutUndo=layouts.UndoCount();
    if(!layouts.UpdateWidgetContainer(container.id,flow,error)||layouts.UndoCount()!=layoutUndo+1)
        return Fail("container layout mutation: "+error);
    const auto nearlyEqual=[](float a,float b){return std::abs(a-b)<0.001f;};
    ScreenRect cellRect;
    if(!ResolveScreenWidgetRect(layouts.Document(),cellIds[0],cellRect,error)||
       !nearlyEqual(cellRect.x,110)||!nearlyEqual(cellRect.y,60)||!nearlyEqual(cellRect.width,137.5f)||!nearlyEqual(cellRect.height,380))
        return Fail("horizontal container geometry incorrect");
    flow.mode=ScreenChildLayout::Vertical;
    if(!layouts.UpdateWidgetContainer(container.id,flow,error)||
       !ResolveScreenWidgetRect(layouts.Document(),cellIds[3],cellRect,error)||
       !nearlyEqual(cellRect.x,110)||!nearlyEqual(cellRect.y,352.5f)||!nearlyEqual(cellRect.width,580)||!nearlyEqual(cellRect.height,87.5f))
        return Fail("vertical container geometry incorrect: "+error);
    flow.mode=ScreenChildLayout::Grid;
    if(!layouts.UpdateWidgetContainer(container.id,flow,error)||
       !ResolveScreenWidgetRect(layouts.Document(),cellIds[2],cellRect,error)||
       !nearlyEqual(cellRect.x,110)||!nearlyEqual(cellRect.y,255)||!nearlyEqual(cellRect.width,285)||!nearlyEqual(cellRect.height,185))
        return Fail("grid container geometry incorrect: "+error);
    const auto managedUndo=layouts.UndoCount();
    if(layouts.TranslateWidgets({cellIds[0]},1,0,error)||layouts.UndoCount()!=managedUndo)
        return Fail("layout-managed child silently accepted movement");
    StableId forbiddenGroup;
    if(layouts.GroupWidgets({cellIds[0],cellIds[1]},forbiddenGroup,error)||
        layouts.UndoCount()!=managedUndo)
        return Fail("layout-managed child grouping was not guarded");
    if(!ResolveScreenWidgetRect(layouts.Document(),cellIds[0],cellRect,error))return Fail(error);
    const auto* managed=layouts.FindWidget(cellIds[0]);
    ScreenWidgetAuthoringEdit textEdit{managed->name,"Edited in a cell",cellRect,managed->visible,managed->enabled};
    if(!layouts.UpdateWidget(cellIds[0],textEdit,error)||
       layouts.FindWidget(cellIds[0])->rect.x!=20||layouts.FindWidget(cellIds[0])->rect.width!=100)
        return Fail("managed content editing drifted fallback geometry: "+error);
    if(!layouts.MoveWidgetsLayer({cellIds[0]},1,true,error)||
       !ResolveScreenWidgetRect(layouts.Document(),cellIds[0],cellRect,error)||
       !nearlyEqual(cellRect.x,405)||!nearlyEqual(cellRect.y,255))
        return Fail("container reorder did not change allocation order");
    if(!layouts.Undo(error))return Fail(error);
    auto badFlow=flow;badFlow.padding=500;
    const auto rejectLayout=layouts.UndoCount();
    if(layouts.UpdateWidgetContainer(container.id,badFlow,error)||layouts.UndoCount()!=rejectLayout)
        return Fail("impossible container layout changed history");
    badFlow=flow;badFlow.gap=-1;
    if(layouts.UpdateWidgetContainer(container.id,badFlow,error)||layouts.UndoCount()!=rejectLayout)
        return Fail("negative container spacing accepted");
    // Nested grids/stacks resolve through their actual allocated parent rectangle.
    auto nested=MakeScreenWidgetPreset(ScreenWidgetPreset::VerticalStack,1280,720);
    nested.parentId=container.id;nested.container.padding=4;nested.container.gap=2;
    StableId nestedId;
    if(!layouts.CreateWidget(nested,false,nestedId,error))return Fail(error);
    for(int n=0;n<2;++n)
    {
        auto leaf=MakeScreenWidgetPreset(ScreenWidgetPreset::Counter,1280,720);leaf.parentId=nestedId;
        StableId leafId;
        if(!layouts.CreateWidget(leaf,false,leafId,error))return Fail(error);
        if(n==1)
        {
            ScreenRect parentRect,leafRect;
            if(!ResolveScreenWidgetRect(layouts.Document(),nestedId,parentRect,error)||
               !ResolveScreenWidgetRect(layouts.Document(),leafId,leafRect,error)||
               !nearlyEqual(leafRect.x,parentRect.x+4)||!nearlyEqual(leafRect.width,parentRect.width-8)||
               !nearlyEqual(leafRect.height,(parentRect.height-10)/2))
                return Fail("nested container geometry did not use allocated cell");
        }
    }
    if(!ResolveScreenWidgetRect(layouts.Document(),container.id,cellRect,error))return Fail(error);
    const auto* parentWidget=layouts.FindWidget(container.id);
    ScreenWidgetAuthoringEdit resizeParent{parentWidget->name,parentWidget->text,
        {cellRect.x,cellRect.y,800,cellRect.height},true,false};
    if(!layouts.UpdateWidget(container.id,resizeParent,error)||
       !ResolveScreenWidgetRect(layouts.Document(),cellIds[0],cellRect,error)||!nearlyEqual(cellRect.width,385))
        return Fail("resized parent did not reflow children");
    ScreenWidgetClipboard layoutCopy;std::vector<StableId> pastedLayout;
    if(!layouts.CopyWidgets({container.id},layoutCopy,error)||
       !layouts.PasteWidgets(layoutCopy,20,20,pastedLayout,error)||pastedLayout.size()!=1||
       layouts.FindWidget(pastedLayout.front())->container.mode!=ScreenChildLayout::Grid)
        return Fail("container subtree clipboard lost layout: "+error);
    if(!layouts.Save(error))return Fail(error);
    ScreenAuthoringSession reopenedLayout;
    if(!reopenedLayout.Open(layoutPath.generic_u8string(),projectId,error)||
       reopenedLayout.FindWidget(container.id)->container.gap!=10||
       reopenedLayout.FindWidget(nestedId)->container.mode!=ScreenChildLayout::Vertical)
        return Fail("container save/reopen lost configuration: "+error);
    auto compact=layouts.FindWidget(container.id)->container;
    compact.compactBelowWidth=1000;compact.compactMode=ScreenChildLayout::Vertical;
    compact.compactColumns=1;
    const auto beforeCompact=layouts.UndoCount();
    if(!layouts.UpdateWidgetContainer(container.id,compact,error)||
        layouts.UndoCount()!=beforeCompact+1||!layouts.Save(error))
        return Fail("compact layout did not enter authoring history: "+error);
    if(!reopenedLayout.Open(layoutPath.generic_u8string(),projectId,error)||
        reopenedLayout.FindWidget(container.id)->container.compactBelowWidth!=1000||
        reopenedLayout.FindWidget(container.id)->container.compactMode!=ScreenChildLayout::Vertical)
        return Fail("compact layout save/reopen lost fields");
    if(!layouts.Undo(error)||layouts.FindWidget(container.id)->container.compactBelowWidth!=0||
        !layouts.Redo(error)||layouts.FindWidget(container.id)->container.compactBelowWidth!=1000)
        return Fail("compact layout undo/redo");
    auto badCompact=compact;badCompact.compactColumns=0;
    if(layouts.UpdateWidgetContainer(container.id,badCompact,error))
        return Fail("invalid compact grid accepted");
    ScreenContainerLayout manual=flow;manual.mode=ScreenChildLayout::Free;
    if(!layouts.UpdateWidgetContainer(container.id,manual,error)||
       !ResolveScreenWidgetRect(layouts.Document(),cellIds[0],cellRect,error)||
       !nearlyEqual(cellRect.x,120)||!nearlyEqual(cellRect.y,70)||!nearlyEqual(cellRect.width,100))
        return Fail("manual mode did not restore authored fallback geometry: "+error);
    // Strict optional-field parsing cannot accept fractional columns.
    std::ifstream malformedIn(layoutPath,std::ios::binary);
    std::string malformed((std::istreambuf_iterator<char>(malformedIn)),{});
    const auto columnsAt=malformed.find("child_columns = 2");
    if(columnsAt==std::string::npos)return Fail("container serialization field missing");
    malformed.replace(columnsAt,std::string("child_columns = 2").size(),"child_columns = 2.5");
    malformedIn.close();
    {std::ofstream out(layoutPath,std::ios::binary);out<<malformed;}
    ScreenDocument malformedDoc;
    if(ReadScreenDocument(layoutPath.generic_u8string(),projectId,malformedDoc,error))
        return Fail("fractional container columns silently accepted");
    // Bound v3 containers explicitly upgrade a legacy menu without altering identity.
    const auto legacyLayoutPath=screenPath.parent_path()/"LegacyContainers.renegade-screen";
    auto legacyLayout=MakeDocument(projectId);legacyLayout.schemaVersion=2;
    if(!WriteScreenDocument(legacyLayoutPath.generic_u8string(),legacyLayout,error))return Fail(error);
    ScreenAuthoringSession legacyLayouts;
    if(!legacyLayouts.Open(legacyLayoutPath.generic_u8string(),projectId,error))return Fail(error);
    auto stackPreset=MakeScreenWidgetPreset(ScreenWidgetPreset::HorizontalStack,1280,720);
    StableId createdStack;
    if(!legacyLayouts.CreateWidget(stackPreset,false,createdStack,error)||
       legacyLayouts.Document().schemaVersion!=3||!legacyLayouts.Save(error))
        return Fail("container creation did not govern legacy upgrade: "+error);


    auto edgeLayout=layoutDoc;
    edgeLayout.widgets.resize(4); // Parent and three children.
    edgeLayout.widgets.front().rect={0,0,1280,720};
    edgeLayout.widgets.front().container.mode=ScreenChildLayout::Horizontal;
    edgeLayout.widgets.front().container.padding=0;edgeLayout.widgets.front().container.gap=0;
    if(!ValidateScreenDocument(edgeLayout,projectId,error)||
       !ResolveScreenWidgetRect(edgeLayout,cellIds[2],cellRect,error)||
       !nearlyEqual(cellRect.x+cellRect.width,1280))
        return Fail("zero-padding fractional cells escaped canvas through rounding: "+error);


    // Legacy anchored documents may legally leave unused absolute rect fields zero.
    auto anchorCompatibility=MakeDocument(projectId);anchorCompatibility.schemaVersion=2;
    anchorCompatibility.widgets.back().rect={};
    if(!ValidateScreenDocument(anchorCompatibility,projectId,error))
        return Fail("container extension rejected valid legacy anchored geometry: "+error);
    const auto anchorCompatibilityPath=screenPath.parent_path()/"LegacyAnchorCompatibility.renegade-screen";
    if(!WriteScreenDocument(anchorCompatibilityPath.generic_u8string(),anchorCompatibility,error))
        return Fail("legacy anchored round-trip write: "+error);
    ScreenDocument anchorReopened;
    if(!ReadScreenDocument(anchorCompatibilityPath.generic_u8string(),projectId,anchorReopened,error)||
       anchorReopened.schemaVersion!=2||
       anchorReopened.widgets.back().container.mode!=ScreenChildLayout::Free)
        return Fail("legacy anchored round-trip changed layout contract: "+error);


    // Animation tracks are authored state, not captured gameplay or a second history.
    const auto animationPath=screenPath.parent_path()/"Animated.renegade-screen";
    auto animatedDoc=MakeDocument(projectId);animatedDoc.schemaVersion=2;
    const auto animatedId=animatedDoc.widgets.front().id;
    if(!WriteScreenDocument(animationPath.generic_u8string(),animatedDoc,error))return Fail(error);
    ScreenAuthoringSession animatedSession;
    if(!animatedSession.Open(animationPath.generic_u8string(),projectId,error))return Fail(error);
    ScreenAnimationTrack fade;fade.keys={{0,0},{0.123456789123,1}};fade.delay=.0123456789123;
    const auto animationUndo=animatedSession.UndoCount();
    if(!animatedSession.UpdateWidgetAnimations(animatedId,{fade},error)||
       animatedSession.Document().schemaVersion!=3||animatedSession.UndoCount()!=animationUndo+1)
        return Fail("animation mutation/explicit upgrade failed: "+error);
    auto invalidTrack=fade;invalidTrack.keys[1].time=0;
    if(animatedSession.UpdateWidgetAnimations(animatedId,{invalidTrack},error)||
       animatedSession.UndoCount()!=animationUndo+1)
        return Fail("invalid animation changed document/history");
    if(animatedSession.UpdateWidgetAnimations(animatedId,{fade,fade},error))
        return Fail("two tracks acquired same animation property");
    invalidTrack=fade;invalidTrack.property=ScreenAnimationProperty::Fill;
    if(animatedSession.UpdateWidgetAnimations(animatedId,{invalidTrack},error))
        return Fail("non-progress widget accepted fill animation");
    if(!animatedSession.Undo(error)||animatedSession.Document().schemaVersion!=2||
       !animatedSession.FindWidget(animatedId)->animations.empty()||
       !animatedSession.Redo(error)||!animatedSession.Save(error))return Fail("animation undo/redo/save: "+error);
    ScreenAuthoringSession animationReopened;
    if(!animationReopened.Open(animationPath.generic_u8string(),projectId,error)||
       animationReopened.FindWidget(animatedId)->animations.size()!=1)
        return Fail("animation did not reopen: "+error);
    const auto& exactAnimation=animationReopened.FindWidget(animatedId)->animations.front();
    if(exactAnimation.keys[1].time!=fade.keys[1].time||exactAnimation.delay!=fade.delay)
        return Fail("animation time precision changed on disk");
    ScreenWidgetClipboard animationClipboard;std::vector<StableId> animationPasted;
    if(!animationReopened.CopyWidgets({animatedId},animationClipboard,error)||
       !animationReopened.PasteWidgets(animationClipboard,0,0,animationPasted,error)||
       animationReopened.FindWidget(animationPasted.front())->animations.size()!=1)
        return Fail("animation subtree clipboard lost track");
    std::ifstream animationIn(animationPath,std::ios::binary);
    std::string corruptedAnimation((std::istreambuf_iterator<char>(animationIn)),{});animationIn.close();
    const auto animationCountAt=corruptedAnimation.find("animation_count = 1");
    if(animationCountAt==std::string::npos)return Fail("animation_count missing");
    corruptedAnimation.replace(animationCountAt,std::string("animation_count = 1").size(),"animation_count = 1.5");
    {std::ofstream out(animationPath,std::ios::binary);out<<corruptedAnimation;}
    ScreenDocument rejectedAnimation;
    if(ReadScreenDocument(animationPath.generic_u8string(),projectId,rejectedAnimation,error))
        return Fail("fractional animation count accepted");


    ScreenDocument simDoc;simDoc.usage=ScreenUsage::Hud;
    simDoc.envelope=CreateDocumentEnvelope(projectId,RuntimeScreenDocumentType,
        "Content/UI/Simulation.renegade-screen","Simulation isolation");
    auto simText=MakeScreenWidgetPreset(ScreenWidgetPreset::PlayerHealthText,1280,720);
    auto simBar=MakeScreenWidgetPreset(ScreenWidgetPreset::PlayerHealthBar,1280,720);
    simText.id=GenerateStableId();simBar.id=GenerateStableId();simDoc.widgets={simText,simBar};
    ScreenSimulation simulation;ScreenBindingFrame simulated;
    if(ScreenSimulation::Sources(simDoc).size()!=2||
       !simulation.Evaluate(simDoc,simulated,error)||simulated.ready||simulated.issues.empty())
        return Fail("simulation fabricated missing gameplay samples");
    auto healthSource=simText.bindings.front().source;
    auto maximumSource=simBar.bindings.front().denominator;
    if(!simulation.SetSample(healthSource,25.0,error)||!simulation.SetSample(maximumSource,100.0,error)||
       !simulation.Evaluate(simDoc,simulated,error)||!simulated.ready||
       simulated.widgets[0].text!="HEALTH: 25"||std::abs(simulated.widgets[1].fill-.25f)>.001f||
       simDoc.widgets[0].text!=simText.text||simDoc.widgets[1].fill!=simBar.fill)
        return Fail("isolated simulation evaluator or authored fallback changed");
    simulation.ClearSample(healthSource);
    if(!simulation.Evaluate(simDoc,simulated,error)||simulated.ready||
       simulated.widgets[0].text!=simText.text)
        return Fail("simulation missing-source fallback did not restore authored state");
    if(!simulation.SetSample(healthSource,std::string("wrong type"),error)||
       !simulation.Evaluate(simDoc,simulated,error)||simulated.ready)
        return Fail("simulation hid type diagnostics");
    simulation.Reset();
    if(simulation.Sample(healthSource))return Fail("simulation reset retained samples");


    {
        const TemporaryDirectory themeTemp{fs::temp_directory_path()/fs::u8path("renegade-ui-theme-"+GenerateStableId())};
        const auto root=themeTemp.path;fs::create_directories(root/"Content/UI");
        const auto sourcePath=root/"Content/UI/ProjectTheme.renegade-screen";
        const auto targetPath=root/"Content/UI/Themed.renegade-screen";
        ScreenDocument palette=MakeDocument(projectId);
        palette.envelope.pathHint="Content/UI/ProjectTheme.renegade-screen";
        palette.widgets.front().style.text.fontSize=63;
        palette.widgets.front().style.opacity=.75f;
        ScreenAnimationTrack themeAnimation;themeAnimation.keys={{0,.25f},{1,1}};
        palette.widgets.front().animations={themeAnimation};
        if(!WriteScreenDocument(sourcePath.generic_u8string(),palette,error))return Fail(error);
        ScreenDocument target=MakeDocument(projectId);
        target.envelope.pathHint="Content/UI/Themed.renegade-screen";
        const auto targetId=target.widgets.front().id;
        target.theme=ScreenThemeReference{palette.envelope.documentId,palette.envelope.pathHint};
        target.widgets.front().themeOverrides=0;
        ScreenDocument themed;
        if(!ResolveScreenTheme(root.generic_u8string(),target,themed,error)||
           themed.widgets.front().style.text.fontSize!=63||themed.widgets.front().style.opacity!=.75f||
           themed.widgets.front().animations.size()!=1||
           target.widgets.front().style.text.fontSize==63)
            return Fail("theme did not inherit without mutating fallback: "+error);
        target.widgets.front().themeOverrides=4;
        if(!ResolveScreenTheme(root.generic_u8string(),target,themed,error)||
           themed.widgets.front().style.text.fontSize!=target.widgets.front().style.text.fontSize||
           themed.widgets.front().style.opacity!=.75f)
            return Fail("theme group override did not preserve local typography");
        palette.widgets.front().style.opacity=.5f;
        if(!WriteScreenDocument(sourcePath.generic_u8string(),palette,error)||
           !ResolveScreenTheme(root.generic_u8string(),target,themed,error)||
           themed.widgets.front().style.opacity!=.5f)
            return Fail("project theme source update did not propagate");
        if(!WriteScreenDocument(targetPath.generic_u8string(),target,error))return Fail(error);
        ScreenAuthoringSession themeSession;
        if(!themeSession.Open(targetPath.generic_u8string(),projectId,error)||
           !themeSession.UpdateWidgetThemeOverrides(targetId,0,error)||
           !themeSession.Save(error)||!themeSession.Reload(error)||
           !themeSession.Document().theme||themeSession.FindWidget(targetId)->themeOverrides!=0)
            return Fail("theme history/save/reload lost source or overrides: "+error);
        ScreenWidgetClipboard themeCopy;std::vector<StableId> themePasted;
        if(!themeSession.CopyWidgets({targetId},themeCopy,error)||themeCopy.widgets.front().themeOverrides!=63||
           themeCopy.widgets.front().style.opacity!=.5f||themeCopy.widgets.front().style.text.fontSize!=63||
           !themeSession.PasteWidgets(themeCopy,0,0,themePasted,error))
            return Fail("independent copy did not bake theme presentation: "+error);
        const auto dependencies=MakeRuntimeScreenDependencyReader(projectId);
        RuntimeScreenDependencyDocument themeDependencies;
        if(!dependencies(targetPath.generic_u8string(),themeDependencies,error)||
           themeDependencies.screenPaths.size()!=1||
           themeDependencies.screenPaths.front()!="Content/UI/ProjectTheme.renegade-screen")
            return Fail("theme typed build dependency missing: "+error);
        palette.theme=ScreenThemeReference{target.envelope.documentId,target.envelope.pathHint};
        if(!WriteScreenDocument(sourcePath.generic_u8string(),palette,error))return Fail(error);
        if(ResolveScreenTheme(root.generic_u8string(),target,themed,error))
            return Fail("theme cycle accepted");
        palette.theme.reset();palette.envelope.projectId=GenerateStableId();
        if(!WriteScreenDocument(sourcePath.generic_u8string(),palette,error))return Fail(error);
        if(ResolveScreenTheme(root.generic_u8string(),target,themed,error))
            return Fail("cross-project theme reference accepted");
        auto localStyle=target.widgets.front().style,changedStyle=localStyle;
        changedStyle.text.fontSize+=1;
        if(ScreenStyleChangedGroups(localStyle,changedStyle)!=4)
            return Fail("typography edit claimed unrelated theme groups");
        changedStyle=localStyle;changedStyle.normal.background.red^=1;
        if(ScreenStyleChangedGroups(localStyle,changedStyle)!=1)
            return Fail("color edit claimed unrelated theme groups");
    }


    const auto controlsPath=screenPath.parent_path()/"ValueControls.renegade-screen";
    auto controlsDoc=MakeDocument(projectId);controlsDoc.schemaVersion=2;
    if(!WriteScreenDocument(controlsPath.generic_u8string(),controlsDoc,error))return Fail(error);
    ScreenAuthoringSession controlsSession;
    if(!controlsSession.Open(controlsPath.generic_u8string(),projectId,error))return Fail(error);
    std::vector<StableId> authoredControls;
    for(auto preset:{ScreenWidgetPreset::Toggle,ScreenWidgetPreset::Slider,ScreenWidgetPreset::Selector,ScreenWidgetPreset::TextInput})
    {
        auto w=MakeScreenWidgetPreset(preset,1280,720);
        if(w.kind==ScreenWidgetKind::TextInput)w.control.value=std::string("  Name ; # \n");
        if(w.kind==ScreenWidgetKind::Selector)w.control.options={" First ","Second"};
        StableId id;if(!controlsSession.CreateWidget(w,false,id,error))return Fail("control creation: "+error);
        authoredControls.push_back(id);
    }
    if(controlsSession.Document().schemaVersion!=3||
       controlsSession.Document().focusOrder.size()!=controlsDoc.focusOrder.size()+4)
        return Fail("value controls missing explicit upgrade or focus order");
    auto sliderConfig=controlsSession.FindWidget(authoredControls[1])->control;
    sliderConfig.minimum=-10;sliderConfig.maximum=10;sliderConfig.step=.5;sliderConfig.value=2.5;
    const auto controlsUndo=controlsSession.UndoCount();
    if(!controlsSession.UpdateWidgetControl(authoredControls[1],sliderConfig,error)||
       controlsSession.UndoCount()!=controlsUndo+1||!controlsSession.Undo(error)||!controlsSession.Redo(error))
        return Fail("control edit/history: "+error);
    auto invalidControl=sliderConfig;invalidControl.step=0;
    if(controlsSession.UpdateWidgetControl(authoredControls[1],invalidControl,error))
        return Fail("zero-step slider accepted");
    if(!controlsSession.MoveButtonFocusEarlier(authoredControls[0],error)||!controlsSession.Save(error))
        return Fail("control focus order/save: "+error);
    ScreenAuthoringSession controlsReopened;
    if(!controlsReopened.Open(controlsPath.generic_u8string(),projectId,error)||
       controlsReopened.FindWidget(authoredControls[1])->control.minimum!=-10||
       std::get<double>(controlsReopened.FindWidget(authoredControls[1])->control.value)!=2.5||
       std::get<std::string>(controlsReopened.FindWidget(authoredControls[3])->control.value)!="  Name ; # \n"||
       controlsReopened.FindWidget(authoredControls[2])->control.options.front()!=" First ")
        return Fail("control persisted data/whitespace did not reopen");
    ScreenWidgetClipboard controlsClipboard;std::vector<StableId> controlsPasted;
    if(!controlsReopened.CopyWidgets(authoredControls,controlsClipboard,error)||
       !controlsReopened.PasteWidgets(controlsClipboard,0,0,controlsPasted,error)||controlsPasted.size()!=4||
       controlsReopened.Document().focusOrder.size()!=controlsDoc.focusOrder.size()+8)
        return Fail("control clipboard lost values or focus identities: "+error);
    ScreenControlData endpoint;endpoint.step=.3;
    if(QuantizeScreenSlider(endpoint,1)!=1||std::abs(QuantizeScreenSlider(endpoint,.2)-.3)>1e-9)
        return Fail("slider endpoint/step quantization");


    {
        const TemporaryDirectory linkedTemp{fs::temp_directory_path()/fs::u8path("renegade-ui-linked-"+GenerateStableId())};
        const auto root=linkedTemp.path;fs::create_directories(root/"Content/UI");
        const auto sourcePath=root/"Content/UI/Component.renegade-screen";
        const auto instancePath=root/"Content/UI/Instance.renegade-screen";
        auto source=MakeDocument(projectId);source.envelope.pathHint="Content/UI/Component.renegade-screen";
        source.widgets[0].text="SOURCE TITLE";source.widgets[0].style.text.fontSize=58;
        source.widgets[1].text="SOURCE BUTTON";source.widgets[1].actionId=RuntimeScreenQuitAction;
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))return Fail(error);
        auto instance=MakeDocument(projectId);instance.envelope.pathHint="Content/UI/Instance.renegade-screen";
        const auto instanceId=instance.widgets[0].id;
        instance.widgets[0].component=ScreenComponentReference{source.envelope.documentId,source.envelope.pathHint,source.widgets[0].id,0};
        instance.widgets[1].component=ScreenComponentReference{source.envelope.documentId,source.envelope.pathHint,source.widgets[1].id,0};
        const auto originalRect=instance.widgets[0].rect;
        ScreenDocument effective;
        if(!ResolveScreenTheme(root.generic_u8string(),instance,effective,error)||
            effective.widgets[0].text!="SOURCE TITLE"||effective.widgets[0].style.text.fontSize!=58||
            !SameRect(effective.widgets[0].rect,originalRect)||effective.widgets[0].id!=instanceId||
            effective.widgets[1].actionId!=instance.widgets[1].actionId||instance.widgets[0].text=="SOURCE TITLE")
            return Fail("linked source inheritance/local identity failed: "+error);
        if(!WriteScreenDocument(instancePath.generic_u8string(),instance,error))return Fail(error);
        ScreenAuthoringSession linked;
        if(!linked.Open(instancePath.generic_u8string(),projectId,error))return Fail(error);
        const auto undo=linked.UndoCount();
        auto creator=CreatorEditFor(*linked.FindWidget(instanceId));creator.style.text.fontSize=47;
        if(!linked.UpdateWidgetCreatorFields(instanceId,creator,error)||
            linked.FindWidget(instanceId)->component->overrides!=4||!linked.Undo(error)||!linked.Redo(error)||
            linked.UndoCount()!=undo+1||!linked.Save(error)||!linked.Reload(error))
            return Fail("linked per-group override/history/persistence: "+error);
        const auto refreshRevision=linked.Revision();
        const auto refreshUndoCount=linked.UndoCount();
        const auto refreshDraftText=linked.FindWidget(instanceId)->text;
        source.widgets[0].text="UPDATED SOURCE";
        source.widgets[0].style.text.fontSize=73;source.widgets[0].style.opacity=.4f;
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error)||
            !ResolveScreenTheme(root.generic_u8string(),linked.Document(),effective,error)||
            effective.widgets[0].text!="UPDATED SOURCE"||effective.widgets[0].style.text.fontSize!=47||
            effective.widgets[0].style.opacity!=.4f||linked.Revision()!=refreshRevision||
            linked.UndoCount()!=refreshUndoCount||linked.FindWidget(instanceId)->text!=refreshDraftText)
            return Fail("linked source refresh overrode local group or mutated authoring history: "+error);
        ScreenWidgetClipboard independent;
        if(!linked.CopyWidgets({instanceId},independent,error)||independent.widgets.front().component||
            independent.widgets.front().text!="UPDATED SOURCE"||independent.widgets.front().style.text.fontSize!=47)
            return Fail("independent linked copy was not baked: "+error);
        RuntimeScreenDependencyDocument closure;
        if(!MakeRuntimeScreenDependencyReader(projectId)(instancePath.generic_u8string(),closure,error)||
            closure.screenPaths.size()!=1||closure.screenPaths.front()!=source.envelope.pathHint)
            return Fail("linked source dependency closure: "+error);
        if(!linked.UpdateWidgetComponent(instanceId,std::nullopt,error)||linked.FindWidget(instanceId)->component||
            linked.FindWidget(instanceId)->text!="UPDATED SOURCE"||
            linked.FindWidget(instanceId)->style.text.fontSize!=47||!linked.Undo(error)||
            !linked.FindWidget(instanceId)->component||!linked.Redo(error)||!linked.Save(error)||!linked.Reload(error)||
            linked.FindWidget(instanceId)->component)
            return Fail("detach did not bake exact appearance or preserve undo/save: "+error);
        auto missing=instance;missing.widgets[0].component->widgetId=GenerateStableId();
        if(ResolveScreenTheme(root.generic_u8string(),missing,effective,error))return Fail("missing linked source widget accepted");
        auto wrongKind=instance;wrongKind.widgets[0].component->widgetId=source.widgets[1].id;
        if(ResolveScreenTheme(root.generic_u8string(),wrongKind,effective,error))return Fail("incompatible linked source kind accepted");
        source.widgets[0].component=ScreenComponentReference{instance.envelope.documentId,instance.envelope.pathHint,instance.widgets[0].id,0};
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))return Fail(error);
        if(ResolveScreenTheme(root.generic_u8string(),instance,effective,error))return Fail("linked source cycle accepted");
        source.widgets[0].component.reset();source.envelope.projectId=GenerateStableId();
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))return Fail(error);
        if(ResolveScreenTheme(root.generic_u8string(),instance,effective,error))return Fail("cross-project linked source accepted");
        auto invalid=instance;invalid.widgets[0].component->overrides=1024;
        if(ValidateScreenDocument(invalid,projectId,error))return Fail("unknown component override bit accepted");
    }


    {
        const TemporaryDirectory treeTemp{fs::temp_directory_path()/fs::u8path("renegade-ui-linked-tree-"+GenerateStableId())};
        const auto root=treeTemp.path;fs::create_directories(root/"Content/UI");
        const auto sourcePath=root/"Content/UI/LinkedTreeSource.renegade-screen";
        const auto instancePath=root/"Content/UI/LinkedTreeInstance.renegade-screen";
        auto source=MakeDocument(projectId);
        source.envelope.pathHint="Content/UI/LinkedTreeSource.renegade-screen";
        auto group=MakeScreenWidgetPreset(ScreenWidgetPreset::Group,1280,720);
        group.id=GenerateStableId();group.name="Source Group";
        const auto sourceRootId=group.id;
        source.widgets[0].parentId=sourceRootId;
        source.widgets[1].parentId=sourceRootId;
        const auto sourceTextId=source.widgets[0].id;
        const auto sourceButtonId=source.widgets[1].id;
        source.widgets.push_back(group);
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))
            return Fail("linked tree source setup: "+error);
        auto instance=MakeDocument(projectId);
        instance.envelope.pathHint="Content/UI/LinkedTreeInstance.renegade-screen";
        if(!WriteScreenDocument(instancePath.generic_u8string(),instance,error))
            return Fail("linked tree instance setup: "+error);
        ScreenAuthoringSession tree;
        if(!tree.Open(instancePath.generic_u8string(),projectId,error))
            return Fail("linked tree instance open: "+error);
        const auto before=tree.Document().widgets.size();
        StableId treeId;
        const ScreenComponentReference ref{source.envelope.documentId,source.envelope.pathHint,sourceRootId,0};
        if(!tree.ImportLinkedWidgetTree(ref,treeId,error)||tree.Document().widgets.size()!=before+3)
            return Fail("linked subtree import failed: "+error);
        StableId textId,buttonId;
        for(const auto& widget:tree.Document().widgets)
        {
            if(!widget.component||widget.component->treeRootId!=treeId)continue;
            if(widget.component->widgetId==sourceTextId)textId=widget.id;
            if(widget.component->widgetId==sourceButtonId)buttonId=widget.id;
        }
        if(textId.empty()||buttonId.empty()||
            tree.FindWidget(textId)->parentId!=treeId||
            tree.FindWidget(buttonId)->parentId!=treeId||
            tree.FindWidget(buttonId)->actionId==source.widgets[1].actionId)
            return Fail("linked subtree identities, parents or independent Flow actions failed");
        // Copying a linked hierarchy produces a truly independent hierarchy:
        // no source references, fresh widget IDs and distinct Flow actions.
        ScreenWidgetClipboard independentTree;
        if(!tree.CopyWidgets({treeId},independentTree,error)||
            independentTree.widgets.size()!=3||independentTree.focusOrder.size()!=1||
            std::any_of(independentTree.widgets.begin(),independentTree.widgets.end(),
                [](const auto& widget){return widget.component.has_value();}))
            return Fail("linked subtree independent clipboard did not bake its source: "+error);
        ScreenAuthoringSession independentTarget;
        if(!independentTarget.Open(instancePath.generic_u8string(),projectId,error))
            return Fail("independent subtree destination open failed: "+error);
        std::vector<StableId> independentRoots;
        if(!independentTarget.PasteWidgets(independentTree,24,24,independentRoots,error)||
            independentRoots.size()!=1||independentRoots[0]==treeId||
            independentTarget.Document().widgets.size()!=before+3||
            std::any_of(independentTarget.Document().widgets.begin(),independentTarget.Document().widgets.end(),
                [](const auto& widget){return widget.component.has_value();}))
            return Fail("linked subtree independent paste identities or source isolation failed: "+error);
        // Editing an individual linked child's parent would make its local
        // structure disagree with the linked source. It must not autosave
        // an unusable Screen or consume an Undo step.
        const auto parentHistory=tree.UndoCount(),parentRevision=tree.Revision();
        auto illegalParent=CreatorEditFor(*tree.FindWidget(textId));
        illegalParent.parentId=buttonId;
        if(tree.UpdateWidgetCreatorFields(textId,illegalParent,error)||
            error.find("Linked hierarchy member") == std::string::npos||
            tree.UndoCount()!=parentHistory||tree.Revision()!=parentRevision||
            tree.FindWidget(textId)->parentId!=treeId)
            return Fail("linked subtree member reparent mutation was not rejected atomically");
        auto edit=CreatorEditFor(*tree.FindWidget(textId));edit.style.text.fontSize=47;
        if(!tree.UpdateWidgetCreatorFields(textId,edit,error))
            return Fail("linked subtree override failed: "+error);
        source.widgets[0].text="LINKED TREE UPDATED";
        source.widgets[0].style.text.fontSize=83;
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))
            return Fail("linked tree source update: "+error);
        const auto beforeHistory=tree.UndoCount(),beforeRevision=tree.Revision();
        if(!tree.RefreshLinkedWidgetTree(treeId,error)||
            tree.UndoCount()!=beforeHistory||tree.Revision()!=beforeRevision)
            return Fail("no-op subtree refresh changed draft history: "+error);
        ScreenDocument effective;
        if(!ResolveScreenTheme(root.generic_u8string(),tree.Document(),effective,error))
            return Fail("linked subtree resolution: "+error);
        const auto inherited=std::find_if(effective.widgets.begin(),effective.widgets.end(),
            [&](const auto& w){return w.id==textId;});
        if(inherited==effective.widgets.end()||inherited->text!="LINKED TREE UPDATED"||
            inherited->style.text.fontSize!=47)
            return Fail("linked subtree source propagation erased its local typography override");
        auto newText=source.widgets[0];newText.id=GenerateStableId();newText.name="New Source Child";
        newText.text="SOURCE CHILD BETA";source.widgets.push_back(newText);
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error)||
            !tree.RefreshLinkedWidgetTree(treeId,error)||
            tree.Document().widgets.size()!=before+4)
            return Fail("new source descendant was not reconciled atomically: "+error);
        StableId newChildId;
        for(const auto& widget:tree.Document().widgets)
            if(widget.component&&widget.component->treeRootId==treeId&&
                widget.component->widgetId==newText.id)newChildId=widget.id;
        if(newChildId.empty()||tree.FindWidget(newChildId)->parentId!=treeId||
            !tree.Save(error)||!tree.Reload(error)||
            !tree.FindWidget(newChildId)->component||
            tree.FindWidget(newChildId)->component->treeRootId!=treeId)
            return Fail("linked subtree reconciliation did not survive save/reopen: "+error);
        const auto history=tree.UndoCount();
        const auto removedText=source.widgets[0];
        source.widgets.erase(std::remove_if(source.widgets.begin(),source.widgets.end(),
            [&](const auto& widget){return widget.id==sourceTextId;}),source.widgets.end());
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error)||
            tree.RefreshLinkedWidgetTree(treeId,error)||tree.UndoCount()!=history||
            !tree.FindWidget(textId))
            return Fail("deleted source member was silently removed or changed local authoring");
        // A conflict elsewhere in the Screen must not authorize emergency
        // detachment of an otherwise healthy selected component.
        auto mixed=tree.Document();
        mixed.envelope.documentId=GenerateStableId();
        mixed.envelope.pathHint="Content/UI/LinkedTreeMixedSources.renegade-screen";
        const auto independentId=mixed.widgets.front().id;
        mixed.widgets.front().component=ScreenComponentReference{
            source.envelope.documentId,source.envelope.pathHint,newText.id,0};
        const auto mixedPath=root/"Content/UI/LinkedTreeMixedSources.renegade-screen";
        if(!WriteScreenDocument(mixedPath.generic_u8string(),mixed,error))
            return Fail("linked recovery mixed dependency fixture write: "+error);
        ScreenAuthoringSession mixedSession;
        if(!mixedSession.Open(mixedPath.generic_u8string(),projectId,error))
            return Fail("linked recovery mixed dependency fixture open: "+error);
        const auto mixedHistory=mixedSession.UndoCount();
        if(mixedSession.DetachUnavailableWidgetSource(independentId,error)||
            mixedSession.UndoCount()!=mixedHistory||
            !mixedSession.FindWidget(independentId)->component)
            return Fail("broken source elsewhere authorized detachment of a healthy linked component");
        if(!mixedSession.DetachUnavailableWidgetSource(textId,error)||
            mixedSession.FindWidget(textId)->component||
            !mixedSession.FindWidget(independentId)->component)
            return Fail("broken tree recovery altered an unrelated healthy link: "+error);
        // A missing source cannot be baked via ordinary Detach. Recovery must
        // be a deliberate second command keeping only authored local values,
        // with the entire linked hierarchy detached in one Undoable step.
        ScreenAuthoringSession rescue;
        if(!rescue.Open(instancePath.generic_u8string(),projectId,error))
            return Fail("broken component recovery fixture open: "+error);
        const auto rescueHistory=rescue.UndoCount();
        if(rescue.UpdateWidgetComponent(textId,std::nullopt,error)||
            rescue.UndoCount()!=rescueHistory||
            !rescue.DetachUnavailableWidgetSource(textId,error)||
            rescue.UndoCount()!=rescueHistory+1||
            rescue.FindWidget(textId)->component||rescue.FindWidget(treeId)->component||
            rescue.FindWidget(newChildId)->component||
            rescue.FindWidget(textId)->style.text.fontSize!=47||
            !rescue.Undo(error)||!rescue.FindWidget(textId)->component||
            !rescue.Redo(error)||rescue.FindWidget(textId)->component)
            return Fail("broken linked source emergency detach/undo fallback: "+error);
        ScreenDocument recovered;
        if(!ResolveScreenTheme(root.generic_u8string(),rescue.Document(),recovered,error))
            return Fail("explicit broken source recovery did not restore independent preview: "+error);
        auto corrupt=tree.Document();
        for(auto& widget:corrupt.widgets)
            if(widget.id==newChildId)widget.component->treeRootId=GenerateStableId();
        if(ValidateScreenDocument(corrupt,projectId,error))
            return Fail("orphaned linked subtree membership was accepted");
        source.widgets.push_back(removedText);
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))
            return Fail("linked tree source restore failed: "+error);
        // A valid source Screen may reparent a widget without changing its
        // ID or kind. Shared preview/Runtime resolution must reject the
        // structural mismatch even if Refresh has not been pressed.
        const auto reparented=std::find_if(source.widgets.begin(),source.widgets.end(),
            [&](const auto& widget){return widget.id==sourceTextId;});
        if(reparented==source.widgets.end())return Fail("linked tree source fixture missing child");
        reparented->parentId.clear();
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error))
            return Fail("linked tree source reparent fixture invalid: "+error);
        RuntimeScreenDependencyDocument treeClosure;
        const auto dependencyReader=MakeRuntimeScreenDependencyReader(projectId);
        if(ResolveScreenTheme(root.generic_u8string(),tree.Document(),effective,error)||
            dependencyReader(instancePath.generic_u8string(),treeClosure,error)||
            tree.RefreshLinkedWidgetTree(treeId,error)||tree.UndoCount()!=history)
            return Fail("linked source reparent was not reported by shared resolver/dependencies");
        reparented->parentId=sourceRootId;
        if(!WriteScreenDocument(sourcePath.generic_u8string(),source,error)||
            !ResolveScreenTheme(root.generic_u8string(),tree.Document(),effective,error))
            return Fail("linked source reparent rollback did not restore resolution: "+error);
        if(!dependencyReader(instancePath.generic_u8string(),treeClosure,error)||
            treeClosure.screenPaths.size()!=1||
            treeClosure.screenPaths.front()!=source.envelope.pathHint)
            return Fail("linked subtree packaging dependency closure: "+error);
        if(tree.DeleteWidgets({newChildId},error)||!tree.FindWidget(newChildId))
            return Fail("partial linked tree deletion was accepted");
        const auto detachHistory=tree.UndoCount();
        if(!tree.UpdateWidgetComponent(textId,std::nullopt,error)||
            tree.FindWidget(textId)->component||tree.FindWidget(treeId)->component||
            tree.FindWidget(newChildId)->component||
            tree.FindWidget(textId)->style.text.fontSize!=47||
            !tree.Undo(error)||!tree.FindWidget(textId)->component||
            !tree.Redo(error)||tree.FindWidget(textId)->component||
            tree.UndoCount()!=detachHistory+1||!tree.Save(error)||!tree.Reload(error)||
            tree.FindWidget(treeId)->component)
            return Fail("linked subtree atomic detach/history/baked values/persistence: "+error);
    }

    {
        const TemporaryDirectory localTemp{fs::temp_directory_path()/fs::u8path("renegade-ui-actions-"+GenerateStableId())};
        const auto root=localTemp.path;fs::create_directories(root/"Content/UI");
        const auto path=root/"Content/UI/Actions.renegade-screen";
        auto doc=MakeDocument(projectId);doc.envelope.pathHint="Content/UI/Actions.renegade-screen";
        auto toggle=MakeScreenWidgetPreset(ScreenWidgetPreset::Toggle,1280,720);toggle.id=GenerateStableId();
        auto slider=MakeScreenWidgetPreset(ScreenWidgetPreset::Slider,1280,720);slider.id=GenerateStableId();
        auto input=MakeScreenWidgetPreset(ScreenWidgetPreset::TextInput,1280,720);input.id=GenerateStableId();
        input.control.value=std::string("  001+2 ; # ");
        doc.widgets.insert(doc.widgets.end(),{toggle,slider,input});
        doc.focusOrder.insert(doc.focusOrder.end(),{toggle.id,slider.id,input.id});
        const auto button=doc.widgets[1].id,title=doc.widgets[0].id;
        std::vector<ScreenLocalAction> steps{
            {ScreenInteractionEvent::Activate,ScreenLocalActionKind::SetValue,toggle.id,true,{}},
            {ScreenInteractionEvent::Activate,ScreenLocalActionKind::SetValue,slider.id,.8,ScreenLocalCondition{toggle.id,ScreenLocalComparison::Equal,true}},
            {ScreenInteractionEvent::Activate,ScreenLocalActionKind::SetVisible,title,false,ScreenLocalCondition{input.id,ScreenLocalComparison::Equal,std::string("  001+2 ; # ")}},
            {ScreenInteractionEvent::Activate,ScreenLocalActionKind::Focus,input.id,true,{}}
        };
        if(!WriteScreenDocument(path.generic_u8string(),doc,error))return Fail(error);
        ScreenAuthoringSession actions;
        if(!actions.Open(path.generic_u8string(),projectId,error)||
            !actions.UpdateWidgetInteractions(button,steps,error)||!actions.Undo(error)||!actions.Redo(error)||
            !actions.Save(error)||!actions.Reload(error)||actions.FindWidget(button)->interactions.size()!=4||
            std::get<std::string>(actions.FindWidget(button)->interactions[2].condition->value)!="  001+2 ; # ")
            return Fail("ordered action authoring/history/persistence: "+error);
        ScreenInteractionPlan plan;
        if(!BuildScreenInteractionPlan(actions.Document(),button,ScreenInteractionEvent::Activate,plan,error)||
            plan.executed!=4||plan.focusWidgetId!=input.id||
            std::get<double>(FindScreenInteractionWidget(plan.document,slider.id)->control.value)!=.8||
            FindScreenInteractionWidget(plan.document,title)->visible||
            std::get<bool>(FindScreenInteractionWidget(actions.Document(),toggle.id)->control.value))
            return Fail("ordered condition evaluation or authored isolation: "+error);
        ScreenWidgetClipboard copied;std::vector<StableId> pasted;
        if(!actions.CopyWidgets({button,toggle.id,slider.id,input.id},copied,error)||
            !actions.PasteWidgets(copied,0,0,pasted,error))return Fail("interaction clipboard: "+error);
        const ScreenWidget* copiedButton=nullptr;
        for(const auto& id:pasted){const auto* w=actions.FindWidget(id);if(w->kind==ScreenWidgetKind::Button)copiedButton=w;}
        if(!copiedButton||copiedButton->interactions[0].targetId==toggle.id||
            copiedButton->interactions[1].condition->widgetId==toggle.id||
            !actions.FindWidget(copiedButton->interactions[0].targetId))
            return Fail("copied action/condition references were not remapped");
        const auto beforeDelete=actions.Document().widgets.size();
        if(actions.DeleteWidgets({toggle.id},error)||actions.Document().widgets.size()!=beforeDelete)
            return Fail("deletion silently weakened an action condition");
        auto invalid=steps;invalid[0].targetId=GenerateStableId();
        if(actions.UpdateWidgetInteractions(button,invalid,error))return Fail("missing action target accepted");
        invalid=steps;invalid[1].value=2.0;
        if(actions.UpdateWidgetInteractions(button,invalid,error))return Fail("out-of-bounds action value accepted");
        invalid=steps;invalid[1].condition->value=std::string("wrong type");
        if(actions.UpdateWidgetInteractions(button,invalid,error))return Fail("condition type mismatch accepted");
        invalid.assign(33,steps.front());
        if(actions.UpdateWidgetInteractions(button,invalid,error))return Fail("unbounded action list accepted");
    }

    {
        const TemporaryDirectory temp{fs::temp_directory_path()/fs::u8path("renegade-transitions-"+GenerateStableId())};
        fs::create_directories(temp.path/"Content/UI");
        const auto path=temp.path/"Content/UI/Transitions.renegade-screen";
        auto doc=MakeDocument(projectId);doc.schemaVersion=2;
        doc.envelope.pathHint="Content/UI/Transitions.renegade-screen";
        if(!WriteScreenDocument(path.generic_u8string(),doc,error))return Fail(error);
        ScreenAuthoringSession session;
        if(!session.Open(path.generic_u8string(),projectId,error))return Fail(error);
        ScreenAnimationTrack enter;enter.delay=.1;enter.keys={{0,0},{.25,1}};
        ScreenAnimationTrack exit;exit.keys={{0,1},{.3,0}};
        ScreenTransitionPair pair{{enter},{exit}};
        if(!session.UpdateScreenTransitions(pair,error)||session.Document().schemaVersion!=3||
            !session.Undo(error)||session.Document().schemaVersion!=2||
            !session.Document().transitions.enter.empty()||!session.Redo(error))
            return Fail("transition explicit upgrade/history failed: "+error);
        const auto button=doc.widgets[1].id;
        auto overridePolicy=pair;
        overridePolicy.enter[0].property=ScreenAnimationProperty::Scale;
        overridePolicy.enter[0].keys={{0,.8f},{.35,1}};
        if(!session.UpdateWidgetTransitionOverride(button,overridePolicy,error)||
            session.FindWidget(doc.widgets[2].id)->transitionOverride||
            !session.Save(error)||!session.Reload(error)||
            session.Document().transitions.enter[0].delay!=.1||
            session.FindWidget(button)->transitionOverride->enter[0].property!=ScreenAnimationProperty::Scale)
            return Fail("transition override isolation/save/reopen failed: "+error);
        const auto history=session.UndoCount();
        auto invalid=pair;invalid.exit[0].loop=ScreenAnimationLoop::Repeat;
        if(session.UpdateScreenTransitions(invalid,error)||session.UndoCount()!=history||
            session.UpdateWidgetTransitionOverride(doc.widgets[0].id,pair,error))
            return Fail("invalid transition changed document/history");
        invalid=pair;invalid.enter[0].keys.back().value=.5f;
        if(session.UpdateScreenTransitions(invalid,error))
            return Fail("enter transition may not leave steady presentation displaced");
        if(!session.UpdateWidgetTransitionOverride(button,std::nullopt,error)||
            session.FindWidget(button)->transitionOverride||!session.Undo(error)||
            !session.FindWidget(button)->transitionOverride||!session.Redo(error)||
            session.FindWidget(button)->transitionOverride)
            return Fail("transition default inheritance undo/redo failed");
        if(!session.Save(error))return Fail(error);
        std::ifstream input(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});
        const auto section=bytes.find("[screen_exit]");
        if(section==std::string::npos)return Fail("transition serialization section missing");
        bytes.replace(section,std::string("[screen_exit]").size(),"[broken_screen_exit]");
        {std::ofstream bad(path,std::ios::binary|std::ios::trunc);bad<<bytes;}
        ScreenDocument rejected;
        if(ReadScreenDocument(path.generic_u8string(),projectId,rejected,error))
            return Fail("incomplete transition pair silently accepted");
    }

    std::cout << "PASS: Gate 8D Screen authoring preserves basic and advanced edits, creator transactions, style states, typography, parent/layout geometry, action/focus identity, reusable component remapping, history and transactional Save/Open\n";
    return 0;
}
