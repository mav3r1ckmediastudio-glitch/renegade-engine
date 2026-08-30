if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(render_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/RenderSettingsService.h")
set(render_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/RenderSettingsService.cpp")
set(lut_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/RenderLutService.h")
set(lut_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/RenderLutService.cpp")
set(bridge_cmake "${RENEGADE_SOURCE_DIR}/EngineBridge/Phase5Gate5.cmake")
set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(studio_render "${RENEGADE_SOURCE_DIR}/Studio/src/Phase5Gate5RenderWorkspace.cpp")
set(studio_chrome "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")
set(studio_cmake "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt")
set(runtime_header "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.h")
set(runtime_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")
set(settings_test "${RENEGADE_SOURCE_DIR}/Tests/Phase5Gate5RenderSettingsTests.cpp")
set(lut_test "${RENEGADE_SOURCE_DIR}/Tests/Phase5Gate5LutTests.cpp")
set(gate_cmake "${RENEGADE_SOURCE_DIR}/Tests/Phase5Gate5.cmake")
set(gate_doc "${RENEGADE_SOURCE_DIR}/docs/PHASE5_GATE5_POST_PROCESSING.md")
set(lut_1 "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/1.png")
set(lut_2 "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/2.png")
set(lut_3 "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/3.png")

foreach(path IN ITEMS
    "${render_header}" "${render_source}" "${lut_header}" "${lut_source}"
    "${bridge_cmake}" "${studio_header}" "${studio_source}" "${studio_render}"
    "${studio_chrome}" "${studio_cmake}" "${runtime_header}" "${runtime_source}"
    "${settings_test}" "${lut_test}" "${gate_cmake}" "${gate_doc}"
    "${lut_1}" "${lut_2}" "${lut_3}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Gate 5 missing required source: ${path}")
    endif()
endforeach()

file(READ "${render_header}" render_header_text)
file(READ "${render_source}" render_source_text)
file(READ "${lut_header}" lut_header_text)
file(READ "${lut_source}" lut_source_text)
file(READ "${bridge_cmake}" bridge_cmake_text)
file(READ "${studio_header}" studio_header_text)
file(READ "${studio_source}" studio_source_text)
file(READ "${studio_render}" studio_render_text)
file(READ "${studio_chrome}" studio_chrome_text)
file(READ "${studio_cmake}" studio_cmake_text)
file(READ "${runtime_header}" runtime_header_text)
file(READ "${runtime_source}" runtime_source_text)
file(READ "${settings_test}" settings_test_text)
file(READ "${lut_test}" lut_test_text)
file(READ "${gate_cmake}" gate_cmake_text)
file(READ "${gate_doc}" gate_doc_text)

foreach(token IN ITEMS
    "RenderSettingsSchemaVersion"
    "RenderTonemap"
    "AntiAliasingMode"
    "colorGradingEnabled"
    "SetRenderSettingsCommand"
    "RenderSettingsMatchPath")
    string(FIND "${render_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 render-state contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "setTonemap"
    "setExposure"
    "setBrightness"
    "setContrast"
    "setSaturation"
    "setHDRCalibration"
    "setColorGradingEnabled"
    "setBloomEnabled"
    "setBloomThreshold"
    "setEyeAdaptionEnabled"
    "SetTemporalAAEnabled"
    "setMSAASampleCount"
    "ResizeBuffers"
    "setDepthOfFieldEnabled"
    "setMotionBlurEnabled"
    "setSharpenFilterEnabled"
    "setChromaticAberrationEnabled"
    "setDitherEnabled")
    string(FIND "${render_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 native render mapping missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "BuiltInColorGradingLutCount"
    "ValidateColorGradingLutPng"
    "InstallBuiltInColorGradingLut"
    "ImportCustomColorGradingLut"
    "RefreshColorGradingLutResource"
    "SetColorGradingLutCommand")
    string(FIND "${lut_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 LUT service contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "IMPORT_COLORGRADINGLUT"
    "Content"
    "LUTs"
    "Custom"
    "BuiltIn"
    "colorGradingMapName"
    "colorGradingMap")
    string(FIND "${lut_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 LUT implementation missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "RenderLutService.h"
    "RenderLutService.cpp")
    string(FIND "${bridge_cmake_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 LUT service is not wired into EngineBridge: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "OpenRenderWorkspace"
    "SetRenderWorkspaceActive"
    "renderWorkspacePanel_"
    "renderColorGradingEnabled_"
    "renderLutLibrary_"
    "renderLutImport_"
    "renderLutClear_")
    string(FIND "${studio_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Studio declaration missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "CreateRenderWorkspace();"
    "SyncRenderSettingsFromScene(false)"
    "SyncRenderSettingsFromScene(true)"
    "OpenRenderWorkspace")
    string(FIND "${studio_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Studio integration hook missing ${token}")
    endif()
endforeach()

# The former hard-coded Gate 5 image defaults must not reassert themselves in
# Studio after scene state has been applied.
foreach(forbidden IN ITEMS
    "setBloomThreshold(1.35f)"
    "setFXAAEnabled(false);\n        setBloomEnabled(true)")
    string(FIND "${studio_source_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Studio still contains old hard-coded render state: ${forbidden}")
    endif()
endforeach()

foreach(token IN ITEMS
    "RENDER // NATIVE WICKED"
    "IMAGE // TONEMAPPING"
    "COLOR GRADING // LUT"
    "+ IMPORT LUT"
    "BUILT-IN // "
    "BLOOM // AUTO EXPOSURE"
    "ANTI-ALIASING // NATIVE WICKED"
    "POST FX // CAMERA IMAGE"
    "RefreshColorGradingLutResource"
    "InstallBuiltInColorGradingLut"
    "ImportCustomColorGradingLut"
    "SetColorGradingLutCommand")
    string(FIND "${studio_render_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 RENDER workspace missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "RENDER"
    "Action::RenderWorkspace"
    "SetRenderWorkspaceActive")
    string(FIND "${studio_chrome_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Studio chrome missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "src/Phase5Gate5RenderWorkspace.cpp"
    "RENEGADE_LUT_INDEX IN ITEMS 1 2 3"
    "Content/luts/1.png"
    "Content/luts/2.png"
    "Content/luts/3.png"
    "copy_if_different")
    string(FIND "${studio_cmake_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Studio build surface missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "RenderLutService.h"
    "projectRoot_"
    "SyncRenderSettings")
    string(FIND "${runtime_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Runtime declaration missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "RefreshColorGradingLutResource"
    "CaptureRenderSettings"
    "ApplyRenderSettingsToPath"
    "startupResult_.project.rootPath")
    string(FIND "${runtime_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Runtime parity missing ${token}")
    endif()
endforeach()

string(FIND "${runtime_source_text}" "setFXAAEnabled(false);" old_runtime_fxaa)
if(NOT old_runtime_fxaa EQUAL -1)
    message(FATAL_ERROR "Gate 5 Runtime still hard-codes FXAA off outside shared state")
endif()

foreach(token IN ITEMS
    "colorGradingEnabled"
    "AntiAliasingMode::MSAA4X"
    "SetRenderSettingsCommand"
    "RenderSettingsMatchPath(nativePath, authored)")
    string(FIND "${settings_test_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 render regression missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "ValidateColorGradingLutPng"
    "ImportCustomColorGradingLut"
    "SetColorGradingLutCommand"
    "colorGradingMapName"
    "Entity_Serialize")
    string(FIND "${lut_test_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 LUT regression missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "RenegadePhase5Gate5RenderSettingsTests"
    "RenegadePhase5Gate5LutTests"
    "Phase5Gate5SourceContract.cmake")
    string(FIND "${gate_cmake_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 CTest wiring missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "COLOR GRADING // LUT"
    "+ IMPORT LUT"
    "3 representative Renegade built-in"
    "IMPORT_COLORGRADINGLUT")
    string(FIND "${gate_doc_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 documentation missing ${token}")
    endif()
endforeach()


# These ranges are the pinned Wicked Editor authoring domains. Owner testing
# proved the previous oversized domains made most slider travel ineffective.
foreach(token IN ITEMS
    "0.0f, 3.0f, 10000.0f"
    "-1.0f, 1.0f, 10000.0f"
    "0.0f, 1.0f, 1000.0f"
    "0.01f, 0.5f, 10000.0f"
    "1.0f, 20.0f, 1000.0f"
    "0.1f, 400.0f, 10000.0f"
    "0.0f, 40.0f, 1000.0f")
    string(FIND "${studio_render_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Wicked-native creator range missing ${token}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "0.0f, 16.0f, 1600.0f"
    "0.0f, 64.0f, 1280.0f"
    "0.0f, 100.0f, 1000.0f"
    "0.0f, 500.0f, 1000.0f")
    string(FIND "${studio_render_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Gate 5 oversized dead creator range returned: ${forbidden}")
    endif()
endforeach()


# Live creator sliders must directly drive the exact Wicked RenderPath setter.
# This prevents regression to whole-state replay during mouse preview.
foreach(token IN ITEMS
    "setExposure(renderSliderAfter_.exposure)"
    "setBrightness(renderSliderAfter_.brightness)"
    "setContrast(renderSliderAfter_.contrast)"
    "setSaturation(renderSliderAfter_.saturation)"
    "setHDRCalibration(renderSliderAfter_.hdrCalibration)"
    "setBloomThreshold(renderSliderAfter_.bloomThreshold)"
    "setEyeAdaptionKey(renderSliderAfter_.eyeAdaptationKey)"
    "setEyeAdaptionRate(renderSliderAfter_.eyeAdaptationRate)"
    "setDepthOfFieldStrength(renderSliderAfter_.depthOfFieldStrength)"
    "setMotionBlurStrength(renderSliderAfter_.motionBlurStrength)"
    "setSharpenFilterAmount(renderSliderAfter_.sharpenAmount)"
    "setChromaticAberrationAmount(renderSliderAfter_.chromaticAberrationAmount)")
    string(FIND "${studio_render_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 direct native slider preview missing ${token}")
    endif()
endforeach()


# RENDER workspace interaction ownership: frame updates must not relayout widgets.
string(FIND "${studio_source_text}" "if (renderWorkspaceActive_)\n        {\n            LayoutRenderWorkspace();" render_frame_layout)
if(NOT render_frame_layout EQUAL -1)
    message(FATAL_ERROR "Gate 5 frame loop still relayouts Render widgets")
endif()
string(FIND "${studio_source_text}" "if (renderWorkspaceActive_)\n            LayoutRenderWorkspace();" render_resize_layout)
if(render_resize_layout EQUAL -1)
    message(FATAL_ERROR "Gate 5 ResizeLayout no longer owns Render workspace layout")
endif()
string(FIND "${studio_render_text}" "renderWorkspaceActive_ && authoredStateChanged" render_authored_refresh)
if(render_authored_refresh EQUAL -1)
    message(FATAL_ERROR "Gate 5 Render refresh is not gated by authored state changes")
endif()
string(FIND "${studio_render_text}" "RenderField::BloomThreshold, 0.0f, 1.0f, 1000.0f" bloom_useful_range)
if(bloom_useful_range EQUAL -1)
    message(FATAL_ERROR "Gate 5 Bloom mouse range regressed from useful 0-1 range")
endif()


# Wicked Window::AddWidget() copies parent IsEnabled(), which includes visibility.
# The RENDER window must therefore remain visible while all children are attached,
# then be enabled and hidden only after the final control has been created.
string(FIND "${studio_render_text}"
    "renderWorkspacePanel_.scrollbar_vertical.SetVisible(true);\n        renderWorkspacePanel_.SetVisible(false);\n        GetGUI().AddWidget(&renderWorkspacePanel_);"
    render_early_hide)
if(NOT render_early_hide EQUAL -1)
    message(FATAL_ERROR "Gate 5 Render controls are again attached to a hidden/disabled Window")
endif()
string(FIND "${studio_render_text}"
    "GetGUI().AddWidget(&renderWorkspacePanel_);" render_parent_add)
string(FIND "${studio_render_text}"
    "RenderToggle::Dither);" render_last_control)
string(FIND "${studio_render_text}"
    "renderWorkspacePanel_.SetEnabled(true);" render_enable_after_children)
string(FIND "${studio_render_text}"
    "renderWorkspacePanel_.SetVisible(false);" render_hide_after_children)
if(render_parent_add EQUAL -1 OR render_last_control EQUAL -1 OR
   render_enable_after_children EQUAL -1 OR render_hide_after_children EQUAL -1)
    message(FATAL_ERROR "Gate 5 Render enabled-state construction contract is incomplete")
endif()
if(render_enable_after_children LESS render_last_control OR
   render_hide_after_children LESS render_last_control OR
   render_hide_after_children LESS render_enable_after_children)
    message(FATAL_ERROR "Gate 5 Render Window is hidden/disabled before child attachment completes")
endif()

message(STATUS "Phase 5 Gate 5 source contract passed")
