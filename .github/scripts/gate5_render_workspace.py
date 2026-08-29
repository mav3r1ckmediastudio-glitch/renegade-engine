from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


def replace_at_least(text, old, new, minimum, label):
    count = text.count(old)
    if count < minimum:
        raise SystemExit(f"{label}: expected at least {minimum} anchors, found {count}")
    return text.replace(old, new)


# ---- RenegadeStudioChrome.h -------------------------------------------------
path = "Studio/src/RenegadeStudioChrome.h"
text = read(path)
text = replace_once(
    text,
    "            EnvironmentWorkspace,\n            TerrainWorkspace,\n            SceneWorkspace,",
    "            EnvironmentWorkspace,\n            TerrainWorkspace,\n            RenderWorkspace,\n            SceneWorkspace,",
    "chrome action enum")
text = replace_once(
    text,
    "        void SetEnvironmentWorkspaceActive(bool active) noexcept;\n        void SetTerrainWorkspaceActive(bool active) noexcept;",
    "        void SetEnvironmentWorkspaceActive(bool active) noexcept;\n        void SetTerrainWorkspaceActive(bool active) noexcept;\n        void SetRenderWorkspaceActive(bool active) noexcept;\n        [[nodiscard]] bool IsRenderWorkspaceActive() const noexcept\n        {\n            return renderWorkspaceActive_;\n        }",
    "chrome render workspace API")
text = replace_once(
    text,
    "            action_(environmentWorkspaceActive_\n                ? Action::EnvironmentWorkspace\n                : terrainWorkspaceActive_\n                    ? Action::TerrainWorkspace\n                    : Action::SceneWorkspace);",
    "            action_(environmentWorkspaceActive_\n                ? Action::EnvironmentWorkspace\n                : terrainWorkspaceActive_\n                    ? Action::TerrainWorkspace\n                    : renderWorkspaceActive_\n                        ? Action::RenderWorkspace\n                        : Action::SceneWorkspace);",
    "chrome workspace reconcile")
text = replace_once(
    text,
    "        bool environmentWorkspaceActive_ = false;\n        bool terrainWorkspaceActive_ = false;",
    "        bool environmentWorkspaceActive_ = false;\n        bool terrainWorkspaceActive_ = false;\n        bool renderWorkspaceActive_ = false;",
    "chrome workspace state")
write(path, text)


# ---- RenegadeStudioChrome.cpp -----------------------------------------------
path = "Studio/src/RenegadeStudioChrome.cpp"
text = read(path)
text = replace_once(
    text,
    "    void RenegadeStudioChrome::SetEnvironmentWorkspaceActive(\n        const bool active) noexcept\n    {\n        environmentWorkspaceActive_ = active;\n    }\n\n    void RenegadeStudioChrome::SetTerrainWorkspaceActive(\n        const bool active) noexcept\n    {\n        terrainWorkspaceActive_ = active;\n    }",
    "    void RenegadeStudioChrome::SetEnvironmentWorkspaceActive(\n        const bool active) noexcept\n    {\n        environmentWorkspaceActive_ = active;\n        if (active)\n        {\n            terrainWorkspaceActive_ = false;\n            renderWorkspaceActive_ = false;\n        }\n    }\n\n    void RenegadeStudioChrome::SetTerrainWorkspaceActive(\n        const bool active) noexcept\n    {\n        terrainWorkspaceActive_ = active;\n        if (active)\n        {\n            environmentWorkspaceActive_ = false;\n            renderWorkspaceActive_ = false;\n        }\n    }\n\n    void RenegadeStudioChrome::SetRenderWorkspaceActive(\n        const bool active) noexcept\n    {\n        renderWorkspaceActive_ = active;\n        if (active)\n        {\n            environmentWorkspaceActive_ = false;\n            terrainWorkspaceActive_ = false;\n        }\n    }",
    "chrome workspace setters")
text = replace_once(
    text,
    "            if (!consumed && y >= 34.0f && y < 58.0f)\n            {\n                if (x >= sceneMetaX + 16.0f && x < sceneMetaX + 76.0f)\n                {\n                    if (action_)\n                    {\n                        action_(Action::SceneWorkspace);\n                    }\n                    consumed = true;\n                }\n                else if (x >= sceneMetaX + 82.0f &&\n                    x < sceneMetaX + 202.0f)\n                {\n                    if (action_)\n                    {\n                        action_(Action::EnvironmentWorkspace);\n                    }\n                    consumed = true;\n                }\n                else if (x >= sceneMetaX + 208.0f &&\n                    x < sceneMetaX + 286.0f)\n                {\n                    if (action_)\n                    {\n                        action_(Action::TerrainWorkspace);\n                    }\n                    consumed = true;\n                }\n            }",
    "            if (!consumed && y >= 34.0f && y < 58.0f)\n            {\n                if (x >= sceneMetaX + 12.0f && x < sceneMetaX + 62.0f)\n                {\n                    if (action_)\n                        action_(Action::SceneWorkspace);\n                    consumed = true;\n                }\n                else if (x >= sceneMetaX + 64.0f && x < sceneMetaX + 162.0f)\n                {\n                    if (action_)\n                        action_(Action::EnvironmentWorkspace);\n                    consumed = true;\n                }\n                else if (x >= sceneMetaX + 164.0f && x < sceneMetaX + 224.0f)\n                {\n                    if (action_)\n                        action_(Action::TerrainWorkspace);\n                    consumed = true;\n                }\n                else if (x >= sceneMetaX + 226.0f && x < sceneMetaX + 286.0f)\n                {\n                    if (action_)\n                        action_(Action::RenderWorkspace);\n                    consumed = true;\n                }\n            }",
    "chrome workspace hit zones")
text = replace_once(
    text,
    "        const wi::Color sceneWorkspaceColor =\n            environmentWorkspaceActive_ || terrainWorkspaceActive_\n                ? TextSecondary : TextStrong;\n        const wi::Color environmentWorkspaceColor =\n            environmentWorkspaceActive_ ? TextStrong : TextSecondary;\n        const wi::Color terrainWorkspaceColor =\n            terrainWorkspaceActive_ ? TextStrong : TextSecondary;",
    "        const bool sceneWorkspaceActive =\n            !environmentWorkspaceActive_ && !terrainWorkspaceActive_ &&\n            !renderWorkspaceActive_;\n        const wi::Color sceneWorkspaceColor =\n            sceneWorkspaceActive ? TextStrong : TextSecondary;\n        const wi::Color environmentWorkspaceColor =\n            environmentWorkspaceActive_ ? TextStrong : TextSecondary;\n        const wi::Color terrainWorkspaceColor =\n            terrainWorkspaceActive_ ? TextStrong : TextSecondary;\n        const wi::Color renderWorkspaceColor =\n            renderWorkspaceActive_ ? TextStrong : TextSecondary;",
    "chrome workspace colours")
text = replace_once(text, "            sceneMetaX + 20.0f,\n            38.0f,\n            SceneChromeSmallTextSize,\n            sceneWorkspaceColor,", "            sceneMetaX + 14.0f,\n            38.0f,\n            SceneChromeSmallTextSize,\n            sceneWorkspaceColor,", "scene label position")
text = replace_once(
    text,
    "            1.3f,\n            environmentWorkspaceActive_ || terrainWorkspaceActive_\n                ? 0.0f : 0.12f);",
    "            1.1f,\n            sceneWorkspaceActive ? 0.12f : 0.0f);",
    "scene label active style")
text = replace_once(text, "            sceneMetaX + 86.0f,", "            sceneMetaX + 66.0f,", "environment label position")
text = replace_once(text, "            sceneMetaX + 212.0f,", "            sceneMetaX + 166.0f,", "terrain label position")
text = replace_once(
    text,
    "        DrawText(\n            \"TERRAIN\",\n            sceneMetaX + 166.0f,\n            38.0f,\n            SceneChromeSmallTextSize,\n            terrainWorkspaceColor,\n            cmd,\n            1.15f,\n            terrainWorkspaceActive_ ? 0.12f : 0.0f);\n        DrawRect(\n            terrainWorkspaceActive_\n                ? sceneMetaX + 208.0f\n                : environmentWorkspaceActive_\n                    ? sceneMetaX + 82.0f\n                    : sceneMetaX + 16.0f,\n            57.0f,\n            terrainWorkspaceActive_\n                ? 78.0f\n                : environmentWorkspaceActive_ ? 120.0f : 60.0f,\n            2.0f,\n            Forge,\n            cmd);",
    "        DrawText(\n            \"TERRAIN\",\n            sceneMetaX + 166.0f,\n            38.0f,\n            SceneChromeSmallTextSize,\n            terrainWorkspaceColor,\n            cmd,\n            1.05f,\n            terrainWorkspaceActive_ ? 0.12f : 0.0f);\n        DrawText(\n            \"RENDER\",\n            sceneMetaX + 228.0f,\n            38.0f,\n            SceneChromeSmallTextSize,\n            renderWorkspaceColor,\n            cmd,\n            1.05f,\n            renderWorkspaceActive_ ? 0.12f : 0.0f);\n        DrawRect(\n            renderWorkspaceActive_\n                ? sceneMetaX + 226.0f\n                : terrainWorkspaceActive_\n                    ? sceneMetaX + 164.0f\n                    : environmentWorkspaceActive_\n                        ? sceneMetaX + 64.0f\n                        : sceneMetaX + 12.0f,\n            57.0f,\n            renderWorkspaceActive_\n                ? 60.0f\n                : terrainWorkspaceActive_\n                    ? 60.0f\n                    : environmentWorkspaceActive_ ? 98.0f : 50.0f,\n            2.0f,\n            Forge,\n            cmd);",
    "chrome render tab drawing")
write(path, text)


# ---- Physics chrome: RENDER is another base workspace -----------------------
path = "Studio/src/RenegadePhysicsLabStudioChrome.cpp"
text = read(path)
text = replace_once(
    text,
    "                if (action == Action::SceneWorkspace ||\n                    action == Action::EnvironmentWorkspace ||\n                    action == Action::TerrainWorkspace)",
    "                if (action == Action::SceneWorkspace ||\n                    action == Action::EnvironmentWorkspace ||\n                    action == Action::TerrainWorkspace ||\n                    action == Action::RenderWorkspace)",
    "physics base workspace deactivation")
write(path, text)


# ---- StudioApplication.h ----------------------------------------------------
path = "Studio/src/StudioApplication.h"
text = read(path)
text = replace_once(
    text,
    "#include \"renegade/bridge/SceneComponentService.h\"\n",
    "#include \"renegade/bridge/SceneComponentService.h\"\n#include \"renegade/bridge/RenderSettingsService.h\"\n",
    "render settings include")
text = replace_once(
    text,
    "            OpenEnvironmentWorkspace,\n            OpenTerrainWorkspace,\n            OpenSceneWorkspace,",
    "            OpenEnvironmentWorkspace,\n            OpenTerrainWorkspace,\n            OpenRenderWorkspace,\n            OpenSceneWorkspace,",
    "render editor action")
text = replace_once(
    text,
    "        enum class MaterialToggle\n        {\n            ReceiveShadow,\n            CastShadow,\n            UseVertexColors,\n            DoubleSided,\n        };",
    "        enum class MaterialToggle\n        {\n            ReceiveShadow,\n            CastShadow,\n            UseVertexColors,\n            DoubleSided,\n        };\n\n        enum class RenderField\n        {\n            Exposure,\n            Brightness,\n            Contrast,\n            Saturation,\n            HdrCalibration,\n            BloomThreshold,\n            EyeAdaptationKey,\n            EyeAdaptationRate,\n            DepthOfFieldStrength,\n            MotionBlurStrength,\n            SharpenAmount,\n            ChromaticAberrationAmount,\n        };\n\n        enum class RenderToggle\n        {\n            Bloom,\n            EyeAdaptation,\n            DepthOfField,\n            MotionBlur,\n            Sharpen,\n            ChromaticAberration,\n            Dither,\n        };",
    "render authoring enums")
text = replace_once(
    text,
    "        void SetEnvironmentWorkspaceActive(bool active);\n        void SetTerrainWorkspaceActive(bool active);",
    "        void SetEnvironmentWorkspaceActive(bool active);\n        void SetTerrainWorkspaceActive(bool active);\n        void SetRenderWorkspaceActive(bool active);\n        void CreateRenderWorkspace();\n        void LayoutRenderWorkspace();\n        void RefreshRenderWorkspace();\n        void SyncRenderSettingsFromScene(bool resizeBuffersForMSAA);\n        void ApplyRenderSettingsState(\n            const bridge::RenderSettingsState& state,\n            bool resizeBuffersForMSAA);\n        bool CommitRenderSettings(const bridge::RenderSettingsState& state);\n        void ApplyRenderToggle(RenderToggle toggle, bool value);\n        void BeginRenderSlider(RenderField field);\n        void PreviewRenderSlider(RenderField field, float value);\n        void CommitRenderSlider(RenderField field, float value);\n        static void SetRenderFieldValue(\n            bridge::RenderSettingsState& state,\n            RenderField field,\n            float value) noexcept;",
    "render workspace methods")
text = replace_once(
    text,
    "        wi::gui::Label terrainStrokeDiagnostic_;\n        SceneInspectorButton focusButton_;",
    "        wi::gui::Label terrainStrokeDiagnostic_;\n        wi::gui::Window renderWorkspacePanel_;\n        wi::gui::Label renderWorkspaceTitle_;\n        wi::gui::Label renderImageLabel_;\n        SceneInspectorComboBox renderTonemap_;\n        SceneInspectorSlider renderExposure_;\n        SceneInspectorSlider renderBrightness_;\n        SceneInspectorSlider renderContrast_;\n        SceneInspectorSlider renderSaturation_;\n        SceneInspectorSlider renderHdrCalibration_;\n        wi::gui::Label renderBloomLabel_;\n        SceneInspectorCheckBox renderBloomEnabled_;\n        SceneInspectorSlider renderBloomThreshold_;\n        SceneInspectorCheckBox renderEyeAdaptationEnabled_;\n        SceneInspectorSlider renderEyeAdaptationKey_;\n        SceneInspectorSlider renderEyeAdaptationRate_;\n        wi::gui::Label renderAntiAliasingLabel_;\n        SceneInspectorComboBox renderAntiAliasing_;\n        wi::gui::Label renderPostFxLabel_;\n        SceneInspectorCheckBox renderDepthOfFieldEnabled_;\n        SceneInspectorSlider renderDepthOfFieldStrength_;\n        SceneInspectorCheckBox renderMotionBlurEnabled_;\n        SceneInspectorSlider renderMotionBlurStrength_;\n        SceneInspectorCheckBox renderSharpenEnabled_;\n        SceneInspectorSlider renderSharpenAmount_;\n        SceneInspectorCheckBox renderChromaticAberrationEnabled_;\n        SceneInspectorSlider renderChromaticAberrationAmount_;\n        SceneInspectorCheckBox renderDitherEnabled_;\n        SceneInspectorButton focusButton_;",
    "render workspace controls")
text = replace_once(
    text,
    "        bool environmentWorkspaceActive_ = false;\n        bool terrainWorkspaceActive_ = false;",
    "        bool environmentWorkspaceActive_ = false;\n        bool terrainWorkspaceActive_ = false;\n        bool renderWorkspaceActive_ = false;\n        bool renderSliderActive_ = false;\n        bool renderSliderHadCarrierBefore_ = false;\n        RenderField renderSliderField_ = RenderField::Exposure;\n        bridge::RenderSettingsState renderSliderBefore_;\n        bridge::RenderSettingsState renderSliderAfter_;\n        bridge::RenderSettingsState appliedRenderSettings_;\n        bool appliedRenderSettingsInitialized_ = false;",
    "render workspace state")
write(path, text)


# ---- StudioApplication.cpp --------------------------------------------------
path = "Studio/src/StudioApplication.cpp"
text = read(path)
text = replace_once(
    text,
    "        setSSREnabled(false);\n        setReflectionsEnabled(true);\n        setFXAAEnabled(false);\n        setBloomEnabled(true);\n        setBloomThreshold(1.35f);",
    "        setSSREnabled(false);\n        setReflectionsEnabled(true);",
    "remove hidden Gate 5 Studio defaults")
text = replace_once(
    text,
    "        // Fixed exposure. Eye adaption would make the viewport brightness\n        // depend on where the creator points the camera, which is not\n        // acceptable for a colour-accurate authoring viewport.\n        setEyeAdaptionEnabled(false);\n        setExposure(1.0f);",
    "        // Gate 5 owns image-quality state per Level. Apply the persisted\n        // state before RenderPath3D::Load() so the first rendered frame cannot\n        // inherit arbitrary settings from the previous editor scene.\n        SyncRenderSettingsFromScene(false);",
    "replace fixed Studio exposure")
text = replace_once(
    text,
    "        CreateMaterialInspector();\n",
    "        CreateMaterialInspector();\n        CreateRenderWorkspace();\n",
    "create render workspace")
text = replace_at_least(
    text,
    "            SetEnvironmentWorkspaceActive(false);\n            SetTerrainWorkspaceActive(false);\n            RefreshInspector();",
    "            SetEnvironmentWorkspaceActive(false);\n            SetTerrainWorkspaceActive(false);\n            SetRenderWorkspaceActive(false);\n            RefreshInspector();",
    2,
    "hierarchy/entity selection returns to Scene")
text = replace_once(
    text,
    "            case RenegadeStudioChrome::Action::TerrainWorkspace:\n                pendingAction_ = EditorAction::OpenTerrainWorkspace;\n                break;\n            case RenegadeStudioChrome::Action::SceneWorkspace:",
    "            case RenegadeStudioChrome::Action::TerrainWorkspace:\n                pendingAction_ = EditorAction::OpenTerrainWorkspace;\n                break;\n            case RenegadeStudioChrome::Action::RenderWorkspace:\n                pendingAction_ = EditorAction::OpenRenderWorkspace;\n                break;\n            case RenegadeStudioChrome::Action::SceneWorkspace:",
    "chrome render action mapping")
text = replace_at_least(
    text,
    "            SetTerrainWorkspaceActive(false);\n            SetTransformTool(",
    "            SetTerrainWorkspaceActive(false);\n            SetRenderWorkspaceActive(false);\n            SetTransformTool(",
    4,
    "transform tools return to Scene")
text = replace_once(
    text,
    "        case EditorAction::OpenTerrainWorkspace:\n            SetTerrainWorkspaceActive(true);\n            break;\n        case EditorAction::OpenSceneWorkspace:\n            SetEnvironmentWorkspaceActive(false);\n            SetTerrainWorkspaceActive(false);\n            break;",
    "        case EditorAction::OpenTerrainWorkspace:\n            SetTerrainWorkspaceActive(true);\n            break;\n        case EditorAction::OpenRenderWorkspace:\n            SetRenderWorkspaceActive(true);\n            break;\n        case EditorAction::OpenSceneWorkspace:\n            SetEnvironmentWorkspaceActive(false);\n            SetTerrainWorkspaceActive(false);\n            SetRenderWorkspaceActive(false);\n            break;",
    "pending render workspace action")
text = replace_once(
    text,
    "    void StudioRenderPath::SetEnvironmentWorkspaceActive(const bool active)\n    {\n        if (!active && sunPreviewPlaying_)",
    "    void StudioRenderPath::SetEnvironmentWorkspaceActive(const bool active)\n    {\n        if (active && renderWorkspaceActive_)\n            SetRenderWorkspaceActive(false);\n        if (!active && sunPreviewPlaying_)",
    "environment exits render")
text = replace_once(
    text,
    "    void StudioRenderPath::SetTerrainWorkspaceActive(const bool active)\n    {\n        if (sunPreviewPlaying_)",
    "    void StudioRenderPath::SetTerrainWorkspaceActive(const bool active)\n    {\n        if (active && renderWorkspaceActive_)\n            SetRenderWorkspaceActive(false);\n        if (sunPreviewPlaying_)",
    "terrain exits render")

update_match = re.search(r"(    void StudioRenderPath::Update\([^\n]+\)\n    \{\n)", text)
if not update_match:
    raise SystemExit("Studio Update function anchor not found")
insert = (
    update_match.group(1) +
    "        SyncRenderSettingsFromScene(true);\n"
    "        if (renderWorkspaceActive_)\n"
    "        {\n"
    "            LayoutRenderWorkspace();\n"
    "            renderWorkspacePanel_.SetVisible(!projectHubVisible_);\n"
    "            inspectorPanel_.SetVisible(false);\n"
    "        }\n"
)
text = text[:update_match.start()] + insert + text[update_match.end():]
write(path, text)


# ---- Studio CMake ------------------------------------------------------------
path = "Studio/CMakeLists.txt"
text = read(path)
text = replace_once(
    text,
    "    src/Phase5Gate4MaterialInspector.cpp\n    src/StudioApplication.cpp",
    "    src/Phase5Gate4MaterialInspector.cpp\n    src/Phase5Gate5RenderWorkspace.cpp\n    src/StudioApplication.cpp",
    "Studio Gate 5 source registration")
write(path, text)


# ---- Gate 5 source-contract registration ------------------------------------
path = "Tests/Phase5Gate5.cmake"
text = read(path)
text += "\nadd_test(\n    NAME RenegadePhase5Gate5SourceContract\n    COMMAND ${CMAKE_COMMAND}\n        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}\n        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate5SourceContract.cmake\n)\n"
write(path, text)

print("Gate 5 RENDER workspace patch applied successfully")
