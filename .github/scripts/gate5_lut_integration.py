from pathlib import Path

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


# ---- LUT service cleanup ----------------------------------------------------
path = "EngineBridge/include/renegade/bridge/RenderLutService.h"
text = read(path)
text = replace_once(
    text,
    "        bool builtIn = false;\n    };",
    "        bool builtIn = false;\n        std::size_t builtInIndex = 0;\n    };",
    "LUT entry built-in identity")
write(path, text)

path = "EngineBridge/src/RenderLutService.cpp"
text = read(path)
text = replace_once(
    text,
    "#include <array>\n#include <cstdint>",
    "#include <array>\n#include <cctype>\n#include <cstdint>",
    "LUT cctype include")
write(path, text)


# ---- Shared RenderSettings state -------------------------------------------
path = "EngineBridge/include/renegade/bridge/RenderSettingsService.h"
text = read(path)
text = replace_once(
    text,
    "        float hdrCalibration = 1.0f;\n\n        bool bloomEnabled = true;",
    "        float hdrCalibration = 1.0f;\n        bool colorGradingEnabled = true;\n\n        bool bloomEnabled = true;",
    "render color grading state")
write(path, text)

path = "EngineBridge/src/RenderSettingsService.cpp"
text = read(path)
text = replace_once(
    text,
    '    constexpr const char* KeyHDRCalibration = "renegade.render.hdr_calibration";\n    constexpr const char* KeyBloomEnabled',
    '    constexpr const char* KeyHDRCalibration = "renegade.render.hdr_calibration";\n    constexpr const char* KeyColorGradingEnabled = "renegade.render.color_grading.enabled";\n    constexpr const char* KeyBloomEnabled',
    "render color grading metadata key")
text = replace_once(
    text,
    "            !NearlyEqual(left.hdrCalibration, right.hdrCalibration) ||\n            left.bloomEnabled",
    "            !NearlyEqual(left.hdrCalibration, right.hdrCalibration) ||\n            left.colorGradingEnabled != right.colorGradingEnabled ||\n            left.bloomEnabled",
    "render color grading change detection")
text = replace_once(
    text,
    "        state.hdrCalibration = ReadValue(metadata->float_values, KeyHDRCalibration, defaults.hdrCalibration);\n        state.bloomEnabled",
    "        state.hdrCalibration = ReadValue(metadata->float_values, KeyHDRCalibration, defaults.hdrCalibration);\n        state.colorGradingEnabled = ReadValue(metadata->bool_values, KeyColorGradingEnabled, defaults.colorGradingEnabled);\n        state.bloomEnabled",
    "render color grading capture")
text = replace_once(
    text,
    "        metadata->float_values.set(KeyHDRCalibration, safe.hdrCalibration);\n        metadata->bool_values.set(KeyBloomEnabled",
    "        metadata->float_values.set(KeyHDRCalibration, safe.hdrCalibration);\n        metadata->bool_values.set(KeyColorGradingEnabled, safe.colorGradingEnabled);\n        metadata->bool_values.set(KeyBloomEnabled",
    "render color grading write")
text = replace_once(
    text,
    "        path.setHDRCalibration(safe.hdrCalibration);\n        path.setBloomEnabled",
    "        path.setHDRCalibration(safe.hdrCalibration);\n        path.setColorGradingEnabled(safe.colorGradingEnabled);\n        path.setBloomEnabled",
    "native color grading setter")
write(path, text)


# ---- EngineBridge build -----------------------------------------------------
path = "EngineBridge/Phase5Gate5.cmake"
text = read(path)
text = replace_once(
    text,
    "    ${CMAKE_CURRENT_LIST_DIR}/include/renegade/bridge/RenderSettingsService.h\n    ${CMAKE_CURRENT_LIST_DIR}/src/RenderSettingsService.cpp",
    "    ${CMAKE_CURRENT_LIST_DIR}/include/renegade/bridge/RenderSettingsService.h\n    ${CMAKE_CURRENT_LIST_DIR}/src/RenderSettingsService.cpp\n    ${CMAKE_CURRENT_LIST_DIR}/include/renegade/bridge/RenderLutService.h\n    ${CMAKE_CURRENT_LIST_DIR}/src/RenderLutService.cpp",
    "Gate 5 LUT service build")
write(path, text)


# ---- Studio declarations (after gate5_render_workspace.py) -----------------
path = "Studio/src/StudioApplication.h"
text = read(path)
text = replace_once(
    text,
    '#include "renegade/bridge/RenderSettingsService.h"\n',
    '#include "renegade/bridge/RenderSettingsService.h"\n#include "renegade/bridge/RenderLutService.h"\n',
    "Studio LUT service include")
text = replace_once(
    text,
    "        enum class RenderToggle\n        {\n            Bloom,",
    "        enum class RenderToggle\n        {\n            ColorGrading,\n            Bloom,",
    "Studio color grading toggle")
text = replace_once(
    text,
    "        void ApplyRenderToggle(RenderToggle toggle, bool value);\n        void BeginRenderSlider",
    "        void ApplyRenderToggle(RenderToggle toggle, bool value);\n        void RefreshRenderLutLibrary();\n        void ApplyRenderLutChoice(std::size_t choiceIndex);\n        void ImportRenderLut();\n        void ClearRenderLut();\n        void BeginRenderSlider",
    "Studio LUT workspace methods")
text = replace_once(
    text,
    "        wi::gui::Label renderBloomLabel_;\n        SceneInspectorCheckBox renderBloomEnabled_;",
    "        wi::gui::Label renderColorGradingLabel_;\n        SceneInspectorCheckBox renderColorGradingEnabled_;\n        SceneInspectorComboBox renderLutLibrary_;\n        SceneInspectorButton renderLutImport_;\n        SceneInspectorButton renderLutClear_;\n        std::vector<bridge::ColorGradingLutEntry> renderLutChoices_;\n        wi::gui::Label renderBloomLabel_;\n        SceneInspectorCheckBox renderBloomEnabled_;",
    "Studio LUT workspace controls")
write(path, text)


# ---- Dedicated RENDER workspace --------------------------------------------
path = "Studio/src/Phase5Gate5RenderWorkspace.cpp"
text = read(path)
text = replace_once(
    text,
    "        createToggle(\n            renderBloomEnabled_, \"Bloom enabled: \",",
    "        createSection(\n            renderColorGradingLabel_,\n            \"Render Color Grading Section\",\n            \"COLOR GRADING // LUT\");\n        createToggle(\n            renderColorGradingEnabled_, \"Color grading: \",\n            \"Enable Wicked's native color-grading LUT stage.\",\n            RenderToggle::ColorGrading);\n        renderLutLibrary_.Create(\"Render LUT Library\");\n        renderLutLibrary_.SetTooltip(\n            \"Choose one Renegade built-in or project LUT. The selected LUT is stored inside project Content and previews live.\");\n        renderLutLibrary_.OnSelect([this](const wi::gui::EventArgs& args)\n        {\n            ApplyRenderLutChoice(static_cast<std::size_t>(args.userdata));\n        });\n        renderWorkspacePanel_.AddWidget(&renderLutLibrary_);\n\n        renderLutImport_.Create(\"Render LUT Import\");\n        renderLutImport_.SetText(\"+ IMPORT LUT\");\n        renderLutImport_.SetTooltip(\n            \"Add a custom 256x16 RGBA PNG LUT to this project's LUT library.\");\n        renderLutImport_.OnClick([this](const wi::gui::EventArgs&)\n        {\n            ImportRenderLut();\n        });\n        renderWorkspacePanel_.AddWidget(&renderLutImport_);\n\n        renderLutClear_.Create(\"Render LUT Clear\");\n        renderLutClear_.SetText(\"CLEAR\");\n        renderLutClear_.SetTooltip(\"Remove the current scene LUT selection.\");\n        renderLutClear_.OnClick([this](const wi::gui::EventArgs&)\n        {\n            ClearRenderLut();\n        });\n        renderWorkspacePanel_.AddWidget(&renderLutClear_);\n\n        createToggle(\n            renderBloomEnabled_, \"Bloom enabled: \",",
    "Studio LUT controls")
text = replace_once(
    text,
    "        const auto full = [fieldWidth, &y](wi::gui::Widget& widget)\n        {\n            widget.SetPos(XMFLOAT2(12.0f, y));\n            widget.SetSize(XMFLOAT2(fieldWidth, 28.0f));\n            y += 32.0f;\n        };",
    "        const auto full = [fieldWidth, &y](wi::gui::Widget& widget)\n        {\n            widget.SetPos(XMFLOAT2(12.0f, y));\n            widget.SetSize(XMFLOAT2(fieldWidth, 28.0f));\n            y += 32.0f;\n        };\n        const auto two = [fieldWidth, &y](wi::gui::Widget& left, wi::gui::Widget& right)\n        {\n            constexpr float gap = 6.0f;\n            const float width = (fieldWidth - gap) * 0.5f;\n            left.SetPos(XMFLOAT2(12.0f, y));\n            left.SetSize(XMFLOAT2(width, 28.0f));\n            right.SetPos(XMFLOAT2(12.0f + width + gap, y));\n            right.SetSize(XMFLOAT2(width, 28.0f));\n            y += 32.0f;\n        };",
    "Studio LUT two-button layout")
text = replace_once(
    text,
    "        full(renderHdrCalibration_);\n\n        section(renderBloomLabel_);",
    "        full(renderHdrCalibration_);\n\n        section(renderColorGradingLabel_);\n        full(renderColorGradingEnabled_);\n        full(renderLutLibrary_);\n        two(renderLutImport_, renderLutClear_);\n\n        section(renderBloomLabel_);",
    "Studio LUT layout")
text = replace_once(
    text,
    "        renderHdrCalibration_.SetValue(state.hdrCalibration);\n        renderBloomEnabled_.SetCheck",
    "        renderHdrCalibration_.SetValue(state.hdrCalibration);\n        renderColorGradingEnabled_.SetCheck(state.colorGradingEnabled);\n        RefreshRenderLutLibrary();\n        renderBloomEnabled_.SetCheck",
    "Studio LUT refresh")
text = replace_once(
    text,
    "    void StudioRenderPath::SetRenderWorkspaceActive(const bool active)\n    {",
    '''    void StudioRenderPath::RefreshRenderLutLibrary()
    {
        renderLutChoices_.clear();
        renderLutLibrary_.ClearItems();

        bridge::ColorGradingLutEntry none;
        none.displayName = "NONE";
        renderLutChoices_.push_back(none);
        for (std::size_t index = 1;
            index <= bridge::BuiltInColorGradingLutCount; ++index)
        {
            bridge::ColorGradingLutEntry entry;
            entry.displayName = "BUILT-IN // " + std::to_string(index);
            entry.projectRelativePath = "Content/LUTs/BuiltIn/" +
                std::to_string(index) + ".png";
            entry.builtIn = true;
            entry.builtInIndex = index;
            renderLutChoices_.push_back(std::move(entry));
        }

        std::string current;
        bool projectAvailable = false;
        if (session_ != nullptr && session_->Projects().HasProject())
        {
            projectAvailable = true;
            const auto& project = session_->Projects().CurrentProject();
            const auto custom = bridge::ListProjectColorGradingLuts(project.rootPath);
            renderLutChoices_.insert(
                renderLutChoices_.end(), custom.begin(), custom.end());
            const auto& scene = session_->Scenes().GetScene();
            if (scene.weathers.GetCount() > 0)
                current = scene.weathers[0].colorGradingMapName;
        }

        std::size_t selected = 0;
        for (std::size_t index = 0; index < renderLutChoices_.size(); ++index)
        {
            renderLutLibrary_.AddItem(
                renderLutChoices_[index].displayName,
                static_cast<std::uint64_t>(index));
            if (!current.empty() &&
                renderLutChoices_[index].projectRelativePath == current)
            {
                selected = index;
            }
        }
        renderLutLibrary_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(selected));
        renderLutLibrary_.SetEnabled(projectAvailable);
        renderLutImport_.SetEnabled(projectAvailable);
        renderLutClear_.SetEnabled(projectAvailable && !current.empty());
    }

    void StudioRenderPath::ApplyRenderLutChoice(const std::size_t choiceIndex)
    {
        if (session_ == nullptr || !session_->Projects().HasProject() ||
            choiceIndex >= renderLutChoices_.size())
        {
            return;
        }
        if (choiceIndex == 0)
        {
            ClearRenderLut();
            return;
        }

        auto entry = renderLutChoices_[choiceIndex];
        const auto& project = session_->Projects().CurrentProject();
        std::string path = entry.projectRelativePath;
        std::string error;
        if (entry.builtIn)
        {
            const std::string library = wi::helper::GetCurrentPath() + "/Content/luts";
            if (!bridge::InstallBuiltInColorGradingLut(
                    project.rootPath, library, entry.builtInIndex, path, error))
            {
                studioChrome_.SetStatusText("LUT // BUILT-IN INSTALL FAILED // " + error);
                wi::helper::messageBox(error, "Color Grading LUT");
                RefreshRenderLutLibrary();
                return;
            }
        }

        if (!session_->Commands().Execute(
                std::make_unique<bridge::SetColorGradingLutCommand>(
                    session_->Scenes().GetScene(), project.rootPath, path)))
        {
            RefreshRenderLutLibrary();
            return;
        }
        studioChrome_.SetStatusText("LUT // APPLIED // " + entry.displayName);
        RefreshRenderWorkspace();
        RefreshStatus();
    }

    void StudioRenderPath::ImportRenderLut()
    {
        if (session_ == nullptr || !session_->Projects().HasProject())
            return;
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Import Color Grading LUT";
        params.extensions = {"png"};
        wi::helper::FileDialog(params, [this](const std::string& sourcePath)
        {
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, sourcePath](std::uint64_t)
                {
                    if (sourcePath.empty() || session_ == nullptr ||
                        !session_->Projects().HasProject())
                    {
                        return;
                    }
                    const auto& project = session_->Projects().CurrentProject();
                    std::string imported;
                    std::string error;
                    if (!bridge::ImportCustomColorGradingLut(
                            project.rootPath, sourcePath, imported, error))
                    {
                        studioChrome_.SetStatusText("LUT // IMPORT FAILED // " + error);
                        wi::helper::messageBox(
                            error + "\n\nUse a lossless 256x16 8-bit RGBA PNG.",
                            "Import Color Grading LUT");
                        return;
                    }
                    if (!session_->Commands().Execute(
                            std::make_unique<bridge::SetColorGradingLutCommand>(
                                session_->Scenes().GetScene(), project.rootPath, imported)))
                    {
                        studioChrome_.SetStatusText("LUT // IMPORTED // ASSIGN FAILED");
                        RefreshRenderLutLibrary();
                        return;
                    }
                    studioChrome_.SetStatusText("LUT // IMPORTED + APPLIED");
                    RefreshRenderWorkspace();
                    RefreshStatus();
                });
        });
    }

    void StudioRenderPath::ClearRenderLut()
    {
        if (session_ == nullptr || !session_->Projects().HasProject())
            return;
        const auto& project = session_->Projects().CurrentProject();
        if (session_->Commands().Execute(
                std::make_unique<bridge::SetColorGradingLutCommand>(
                    session_->Scenes().GetScene(), project.rootPath, std::string{})))
        {
            studioChrome_.SetStatusText("LUT // CLEARED");
            RefreshRenderWorkspace();
            RefreshStatus();
        }
        else
        {
            RefreshRenderLutLibrary();
        }
    }

    void StudioRenderPath::SetRenderWorkspaceActive(const bool active)
    {''',
    "Studio LUT methods")
text = replace_once(
    text,
    "        if (session_ == nullptr || renderSliderActive_)\n            return;\n        const auto authored = bridge::CaptureRenderSettings(session_->Scenes().GetScene());",
    "        if (session_ == nullptr || renderSliderActive_)\n            return;\n        auto& scene = session_->Scenes().GetScene();\n        if (session_->Projects().HasProject())\n        {\n            std::string ignored;\n            (void)bridge::RefreshColorGradingLutResource(\n                scene, session_->Projects().CurrentProject().rootPath, ignored);\n        }\n        const auto authored = bridge::CaptureRenderSettings(scene);",
    "Studio LUT load refresh")
text = replace_once(
    text,
    "        switch (toggle)\n        {\n        case RenderToggle::Bloom:",
    "        switch (toggle)\n        {\n        case RenderToggle::ColorGrading:\n            state.colorGradingEnabled = value;\n            break;\n        case RenderToggle::Bloom:",
    "Studio color grading toggle application")
write(path, text)


# ---- Runtime parity ---------------------------------------------------------
path = "Runtime/src/RuntimeApplication.h"
text = read(path)
text = replace_once(
    text,
    '#include "renegade/bridge/RenderSettingsService.h"\n',
    '#include "renegade/bridge/RenderSettingsService.h"\n#include "renegade/bridge/RenderLutService.h"\n',
    "Runtime LUT include")
text = replace_once(
    text,
    "        void BindScene(bridge::SceneService& scenes) noexcept;",
    "        void BindScene(\n            bridge::SceneService& scenes,\n            std::string projectRoot) noexcept;",
    "Runtime project-root binding")
text = replace_once(
    text,
    "        bridge::SceneService* scenes_ = nullptr;\n        bridge::RenderSettingsState renderSettings_;",
    "        bridge::SceneService* scenes_ = nullptr;\n        std::string projectRoot_;\n        bridge::RenderSettingsState renderSettings_;",
    "Runtime LUT project root")
write(path, text)

path = "Runtime/src/RuntimeApplication.cpp"
text = read(path)
text = replace_once(
    text,
    "    void RuntimeRenderPath::BindScene(bridge::SceneService& scenes) noexcept\n    {\n        scenes_ = &scenes;",
    "    void RuntimeRenderPath::BindScene(\n        bridge::SceneService& scenes,\n        std::string projectRoot) noexcept\n    {\n        scenes_ = &scenes;\n        projectRoot_ = std::move(projectRoot);",
    "Runtime LUT BindScene")
text = replace_once(
    text,
    "        const auto authored =\n            bridge::CaptureRenderSettings(scenes_->GetScene());",
    "        auto& activeScene = scenes_->GetScene();\n        if (!projectRoot_.empty())\n        {\n            std::string ignored;\n            (void)bridge::RefreshColorGradingLutResource(\n                activeScene, projectRoot_, ignored);\n        }\n        const auto authored =\n            bridge::CaptureRenderSettings(activeScene);",
    "Runtime LUT refresh")
text = replace_once(
    text,
    "        renderer_.BindScene(scenes_);",
    "        renderer_.BindScene(scenes_, startupResult_.project.rootPath);",
    "Runtime project root supply")
write(path, text)


# ---- Studio built-in LUT payload extraction --------------------------------
path = "Studio/CMakeLists.txt"
text = read(path)
anchor = "# Gate 3 concept plate is owner media. CI can omit it; owner packaging injects it."
insert = '''set(RENEGADE_BUILTIN_LUT_ARCHIVE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/luts/Renegade_Builtin_LUTs_1-50_RGBA.zip")
if(NOT EXISTS "${RENEGADE_BUILTIN_LUT_ARCHIVE}")
    message(FATAL_ERROR "Gate 5 built-in LUT archive is missing")
endif()
add_custom_command(TARGET RenegadeStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/luts"
    COMMAND ${CMAKE_COMMAND} -E chdir
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/luts"
        ${CMAKE_COMMAND} -E tar xvf "${RENEGADE_BUILTIN_LUT_ARCHIVE}"
    VERBATIM
)

'''
text = replace_once(text, anchor, insert + anchor, "Studio built-in LUT extraction")
write(path, text)


# ---- Gate 5 regression wiring -----------------------------------------------
path = "Tests/Phase5Gate5.cmake"
text = read(path)
text = replace_once(
    text,
    "add_test(\n    NAME RenegadePhase5Gate5RenderSettingsTests\n    COMMAND RenegadePhase5Gate5RenderSettingsTests\n)",
    "add_test(\n    NAME RenegadePhase5Gate5RenderSettingsTests\n    COMMAND RenegadePhase5Gate5RenderSettingsTests\n)\n\nadd_executable(RenegadePhase5Gate5LutTests\n    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate5LutTests.cpp\n)\n\ntarget_link_libraries(RenegadePhase5Gate5LutTests\n    PRIVATE\n        Renegade::EngineBridge\n)\n\ntarget_compile_options(RenegadePhase5Gate5LutTests\n    PRIVATE\n        \"$<$<CXX_COMPILER_ID:MSVC>:/utf-8>\"\n)\n\nset_target_properties(RenegadePhase5Gate5LutTests PROPERTIES\n    FOLDER \"Renegade/Tests\"\n)\n\nadd_dependencies(\n    RenegadeBridgeTests\n    RenegadePhase5Gate5LutTests\n)\n\nadd_test(\n    NAME RenegadePhase5Gate5LutTests\n    COMMAND RenegadePhase5Gate5LutTests\n)",
    "Gate 5 LUT regression target")
write(path, text)

path = "Tests/Phase5Gate5RenderSettingsTests.cpp"
text = read(path)
text = replace_once(
    text,
    "        !NearlyEqual(defaults.exposure, 1.0f) ||\n        !defaults.bloomEnabled",
    "        !NearlyEqual(defaults.exposure, 1.0f) ||\n        !defaults.colorGradingEnabled || !defaults.bloomEnabled",
    "Gate 5 color grading default regression")
text = replace_once(
    text,
    "    authored.hdrCalibration = 1.5f;\n    authored.bloomEnabled = false;",
    "    authored.hdrCalibration = 1.5f;\n    authored.colorGradingEnabled = false;\n    authored.bloomEnabled = false;",
    "Gate 5 color grading roundtrip regression")
write(path, text)


# ---- Gate contract ----------------------------------------------------------
path = "docs/PHASE5_GATE5_POST_PROCESSING.md"
text = read(path)
text = replace_once(
    text,
    "### BLOOM // AUTO EXPOSURE\n\nExpose:",
    '''### COLOR GRADING // LUT

Expose Wicked's native color-grading stage as a first-class creator workflow:

- color grading enabled,
- 50 Renegade built-in 256x16 RGBA PNG LUTs,
- a project LUT library selector,
- a small `+ IMPORT LUT` action for creator-supplied 256x16 RGBA PNG LUTs,
- `CLEAR` to remove the scene LUT selection.

Wicked's native `WeatherComponent::colorGradingMapName` remains the serialized LUT identity and `IMPORT_COLORGRADINGLUT` remains the decoder. Renegade copies every selected built-in or imported custom LUT into project `Content/LUTs/...` and stores only that project-relative path in WISCENE. This makes Studio preview, Test Level, dependency extraction and packaged Runtime consume the same file without retaining an editor-installation or desktop source path.

The decoded `colorGradingMap` resource is non-serialized upstream state and is reconstructed from the project path after load. LUT selection/clear is command-backed and previews immediately in Studio.

### BLOOM // AUTO EXPOSURE

Expose:''',
    "Gate 5 LUT contract section")
text = replace_once(
    text,
    "- Bloom: enabled\n- Bloom threshold: 1.0",
    "- Color grading stage: enabled (no LUT selected by default)\n- Bloom: enabled\n- Bloom threshold: 1.0",
    "Gate 5 LUT default")
text = replace_once(
    text,
    "5. Bloom can be disabled/enabled and its threshold visibly changes bright-emissive bloom response.",
    "5. Built-in LUTs can be selected and visibly preview immediately in Studio; a custom 256x16 RGBA PNG can be added with `+ IMPORT LUT`, selected, cleared, saved/reopened and restored.\n6. Bloom can be disabled/enabled and its threshold visibly changes bright-emissive bloom response.",
    "Gate 5 LUT owner acceptance")
write(path, text)

print("Gate 5 LUT integration patch applied successfully")
