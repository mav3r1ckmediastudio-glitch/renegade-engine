#include "StudioApplication.h"

namespace renegade::studio
{
    const char* StudioRenderPath::DiagnosticActionName(EditorAction action)
    {
        switch (action)
        {
        case EditorAction::None: return "None";
        case EditorAction::Undo: return "Undo";
        case EditorAction::Redo: return "Redo";
        case EditorAction::FocusSelection: return "FocusSelection";
        case EditorAction::DuplicateSelection: return "DuplicateSelection";
        case EditorAction::DeleteSelection: return "DeleteSelection";
        case EditorAction::CreateLight: return "CreateLight";
        case EditorAction::CreatePlayerStart: return "CreatePlayerStart";
        case EditorAction::CreateCamera: return "CreateCamera";
        case EditorAction::CreateDecal: return "CreateDecal";
        case EditorAction::CreateEnvironmentProbe: return "CreateEnvironmentProbe";
        case EditorAction::OpenScene: return "OpenScene";
        case EditorAction::SaveScene: return "SaveScene";
        case EditorAction::SaveSceneAs: return "SaveSceneAs";
        case EditorAction::ReopenScene: return "ReopenScene";
        case EditorAction::ProjectHub: return "ProjectHub";
        case EditorAction::SelectTool: return "SelectTool";
        case EditorAction::TranslateTool: return "TranslateTool";
        case EditorAction::RotateTool: return "RotateTool";
        case EditorAction::ScaleTool: return "ScaleTool";
        case EditorAction::ToggleGrid: return "ToggleGrid";
        case EditorAction::OpenEnvironmentWorkspace: return "OpenEnvironmentWorkspace";
        case EditorAction::OpenTerrainWorkspace: return "OpenTerrainWorkspace";
        case EditorAction::OpenRenderWorkspace: return "OpenRenderWorkspace";
        case EditorAction::OpenSceneWorkspace: return "OpenSceneWorkspace";
        case EditorAction::StartTestLevel: return "StartTestLevel";
        case EditorAction::StartProjectPlay: return "StartProjectPlay";
        case EditorAction::StopTestLevel: return "StopTestLevel";
        case EditorAction::BuildWindowsGame: return "BuildWindowsGame";
        case EditorAction::StartSunPreview: return "StartSunPreview";
        case EditorAction::PauseSunPreview: return "PauseSunPreview";
        case EditorAction::SetOceanEnabled: return "SetOceanEnabled";
        case EditorAction::SetOceanResolution: return "SetOceanResolution";
        case EditorAction::ApplyOceanPreset: return "ApplyOceanPreset";
        case EditorAction::CreateTerrain: return "CreateTerrain";
        case EditorAction::ExpandTerrain: return "ExpandTerrain";
        case EditorAction::ApplyTerrainMaterialPreset: return "ApplyTerrainMaterialPreset";
        case EditorAction::ApplyDefaultGrass: return "ApplyDefaultGrass";
        case EditorAction::ReloadTerrainMaterial: return "ReloadTerrainMaterial";
        case EditorAction::ValidateModelImport: return "ValidateModelImport";
        case EditorAction::ImportModel: return "ImportModel";
        case EditorAction::ApplyImportScale: return "ApplyImportScale";
        case EditorAction::DismissImportScale: return "DismissImportScale";
        }
        return "unknown";
    }

    void StudioRenderPath::InitializeLiveDiagnostics()
    {
        diagnosticService_.Identify("studio");
        diagnosticService_.StartLocalEndpoint(38741);
    }

    void StudioRenderPath::RequestDiagnosticAction(EditorAction action)
    {
        pendingAction_ = action;
        ++diagnosticActionSequence_;
        TraceDiagnosticAction(action, "requested");
    }

    void StudioRenderPath::TraceDiagnosticAction(EditorAction action, const char* stage)
    {
        if (action == EditorAction::None) return;
        diagnosticService_.SetState("last_action", {{"name", std::string(DiagnosticActionName(action))},
            {"stage", std::string(stage)}, {"sequence", diagnosticActionSequence_},
            {"elapsed_ms", diagnosticService_.ElapsedMs()},
            {"source", std::string("Studio/src/StudioApplication.cpp:ProcessPendingAction")}});
        diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
            "Studio/src/StudioApplication.cpp:ProcessPendingAction", stage, DiagnosticActionName(action));
    }

    void StudioRenderPath::UpdateLiveDiagnostics(const char* outerWorkspace)
    {
        diagnosticService_.Heartbeat();
        const auto now = diagnosticService_.ElapsedMs();
        if (now - lastDiagnosticSampleMs_ < 250) return;
        lastDiagnosticSampleMs_ = now;
        const bool levelEditor = std::string(outerWorkspace) == "level_editor";
        const std::string workspace = !levelEditor ? outerWorkspace : projectHubVisible_ ? "project_hub" :
            studioChrome_.IsPhysicsLabActive() ? "physics" : studioChrome_.IsAudioWorkspaceActive() ? "audio" :
            terrainWorkspaceActive_ ? "terrain" : environmentWorkspaceActive_ ? "environment" :
            renderWorkspaceActive_ ? "render" : "scene";
        const bool projectOpen = session_ && session_->Projects().HasProject();
        const auto selected = session_ ? session_->Selection().SelectedEntity() : wi::ecs::INVALID_ENTITY;
        const bool selectedValid = session_ && selected != wi::ecs::INVALID_ENTITY;
        // Direct component lookups only: ContainsEntity performs a full-scene scan.
        std::string selectedName;
        if (selectedValid)
            if (const auto* name = session_->Scenes().GetScene().names.GetComponent(selected)) selectedName = name->name;
        diagnosticService_.Observe("editor", {{"workspace", workspace}, {"project_open", projectOpen},
            {"project", projectOpen ? session_->Projects().CurrentProject().descriptorPath : std::string()},
            {"scene", session_ ? session_->Scenes().CurrentPath() : std::string()},
            {"scene_loading", sceneOpenInProgress_}, {"has_selection", selectedValid},
            {"selected_entity", selectedValid ? bridge::DiagnosticValue(static_cast<std::uint64_t>(selected)) : bridge::DiagnosticValue(nullptr)},
            {"selected_name", selectedName}, {"selection_scope", std::string("process-local ECS id")},
            {"inspector_owner", levelEditor ? workspace : std::string(outerWorkspace)},
            {"selection_applicable", levelEditor},
            {"tool", !levelEditor ? std::string("not_applicable") : lightPlacementActive_ ? std::string("light_placement") :
                creatorAssetPlacementActive_ ? std::string("asset_placement") : gizmo_.isTranslator ? std::string("translate") :
                gizmo_.isRotator ? std::string("rotate") : gizmo_.isScalator ? std::string("scale") : std::string("select")},
            {"placement_active", lightPlacementActive_ || creatorAssetPlacementActive_},
            {"gizmo_drag_active", gizmoDragActive_}, {"terrain_stroke_active", terrainStrokeActive_},
            {"asset_drop_pending", creatorAssetDropPending_}, {"import_active", DiagnosticImportActive()}},
            "Studio/src/StudioLiveDiagnostics.cpp");
        diagnosticService_.SetState("input", {{"applicable", levelEditor}, {"gui_focus", GetGUI().HasFocus()},
            {"pointer_over_viewport", levelEditor && IsPointerOverViewport(wi::input::GetPointer())},
            {"fly_camera_active", flyCameraActive_}, {"snapshot_elapsed_ms", now}});
        diagnosticService_.SetState("vegetation", CaptureVegetationDiagnostics());
        const auto& play = testLevelRuntime_.LastResult();
        diagnosticService_.Observe("test_level", {{"active", testLevelRuntime_.IsActive()}, {"ready", play.ready},
            {"state_code", static_cast<std::uint64_t>(play.state)}, {"message", play.message},
            {"warning", play.warning}, {"child_pid", static_cast<std::uint64_t>(testLevelRuntime_.ProcessId())}},
            "Studio/src/TestLevelRuntimeProcess.cpp");
        std::string text = diagnosticService_.SummaryText();
        const auto peer = diagnosticService_.ReadPeerSummary();
        text += peer.empty() ? "\nRUNTIME // endpoint unavailable" : "\nRUNTIME // " + peer;
        studioChrome_.SetDiagnosticText(std::move(text));
    }
}
