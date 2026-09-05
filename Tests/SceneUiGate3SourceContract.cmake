if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(STUDIO_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(CHROME_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.h")
set(CHROME_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")

foreach(path IN ITEMS
    "${STUDIO_HEADER}"
    "${STUDIO_SOURCE}"
    "${CHROME_HEADER}"
    "${CHROME_SOURCE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Scene UI Gate 3 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${STUDIO_HEADER}" studio_header_source)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${CHROME_HEADER}" chrome_header_source)
file(READ "${CHROME_SOURCE}" chrome_source)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 3 contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 3 contract regressed ${description}: ${needle}")
    endif()
endfunction()

# Scene Inspector readability is opt-in rather than a global mutation of the
# controls already accepted by Story Flow, Screen Editor and creator workflows.
require_text(chrome_header_source
    "int renderTextSize_ = 10;"
    "backwards-compatible shared 10 px control default")
require_text(chrome_header_source
    "int labelTextSize_ = 9;"
    "backwards-compatible shared slider label default")
require_text(chrome_header_source
    "int valueTextSize_ = 10;"
    "backwards-compatible shared slider value default")
require_text(chrome_header_source
    "SceneInspectorTextInputField() { SetRenderTextSize(12); }"
    "12 px Scene Inspector text inputs")
require_text(chrome_header_source
    "SceneInspectorComboBox() { SetRenderTextSize(12); }"
    "12 px Scene Inspector combo boxes")
require_text(chrome_header_source
    "SceneInspectorButton() { SetRenderTextSize(11); }"
    "11 px Scene Inspector buttons")
require_text(chrome_header_source
    "SceneInspectorCheckBox() { SetRenderTextSize(11); }"
    "11 px Scene Inspector check boxes")
require_text(chrome_header_source
    "SceneInspectorSlider() { SetRenderTextSizes(11, 11); }"
    "11 px Scene Inspector slider labels and values")
require_text(chrome_source
    "void RenegadeCheckBox::SetRenderTextSize(const int size) noexcept"
    "configurable check-box typography")
require_text(chrome_source
    "void RenegadeSlider::SetRenderTextSizes("
    "configurable slider typography")
require_text(chrome_source
    "labelTextSize_,\n            TextStrong"
    "slider label uses configured size")
require_text(chrome_source
    "valueTextSize_,\n            TextStrong"
    "slider numeric value uses configured size")
require_text(chrome_source
    "(scale.y - static_cast<float>(renderTextSize_)) * 0.5f - 1.0f"
    "shared check-box default baseline preservation")
require_text(chrome_source
    "(inputSize.y - static_cast<float>(valueTextSize_)) * 0.5f - 1.0f"
    "shared slider-value default baseline preservation")

# Representative controls from every Scene Inspector workspace must use the
# Scene-only readable types. Non-Scene surfaces deliberately keep base types.
require_text(studio_header_source
    "SceneInspectorTextInputField translationX_;"
    "Transform readable input")
require_text(studio_header_source
    "SceneInspectorComboBox lightType_;"
    "Light readable combo")
require_text(studio_header_source
    "SceneInspectorSlider lightIntensity_;"
    "Light readable slider")
require_text(studio_header_source
    "SceneInspectorCheckBox aerialPerspective_;"
    "Environment readable check box")
require_text(studio_header_source
    "SceneInspectorComboBox precipitationMode_;"
    "Environment readable combo")
require_text(studio_header_source
    "SceneInspectorSlider oceanWaterHeight_;"
    "Environment readable slider")
require_text(studio_header_source
    "SceneInspectorComboBox terrainMaterialPreset_;"
    "Terrain readable combo")
require_text(studio_header_source
    "SceneInspectorSlider terrainBrushStrength_;"
    "Terrain readable sculpt slider")
require_text(studio_header_source
    "SceneInspectorButton terrainReloadMaterialButton_;"
    "Terrain readable action")
require_text(studio_header_source
    "SceneInspectorButton saveButton_;"
    "Scene Inspector readable transaction action")
require_text(studio_header_source
    "RenegadeTextInputField hubNewProjectNameInput_;"
    "Project Hub retains independent typography")
require_text(studio_header_source
    "RenegadeComboBox importScaleModeCombo_;"
    "creator import scale UI retains independent typography")
require_text(studio_header_source
    "RenegadeButton importScaleApplyButton_;"
    "creator import action retains independent typography")

# Existing headings were already readable and must stay that way while the
# functional Inspector remains the scrollable opaque window host.
require_text(studio_source
    "inspectorLabel_.font.params.size = 16;"
    "16 px Inspector heading")
require_text(studio_source
    "label.font.params.size = 13;"
    "13 px Inspector section headings")
require_text(studio_source
    "inspectorPanel_.SetSize(XMFLOAT2("
    "Inspector window layout authority")
require_text(studio_source
    "inspectorPanel_.SetColor(\n            wi::Color(8, 11, 13, 255),\n            wi::gui::WIDGET_ID_WINDOW_BASE);"
    "opaque Inspector host")
require_text(studio_source
    "positionEnvironmentWidget(environmentSkyLabel_, 44.0f, 20.0f);"
    "Environment content remains owned by Inspector layout")
require_text(studio_source
    "positionEnvironmentWidget(terrainLabel_, 44.0f, 20.0f);"
    "Terrain content remains owned by Inspector layout")

# Scene-owned chrome has a named readability floor. Asset Browser card/detail
# typography is intentionally outside Gate 3 and remains for Gate 4.
require_text(chrome_source
    "constexpr int SceneChromeSmallTextSize = 10;"
    "10 px minimum Scene chrome secondary text")
require_text(chrome_source
    "constexpr int SceneChromeStandardTextSize = 11;"
    "11 px standard Scene chrome text")
require_text(chrome_source
    "constexpr int SceneHierarchyEntityTextSize = 12;"
    "12 px Hierarchy entity names")
require_text(chrome_source
    "row.name,"
    "Hierarchy entity-name renderer")
require_text(chrome_source
    "SceneHierarchyEntityTextSize,"
    "Hierarchy rows use readable entity size")
require_text(chrome_source
    "row.selected ? TextStrong : TextSecondary,"
    "Hierarchy selection readability")
require_text(chrome_source
    "SEARCH SCENE..."
    "Hierarchy search presentation")

# Long Scene status/error strings may never run beneath the FPS/ownership group.
# The footer is a bounded summary while Output presents the same raw status in
# wrapped lines rather than truncating or allowing it to run off the drawer.
require_text(chrome_source
    "std::size_t StatusTextCapacity(const float width) noexcept"
    "width-aware status capacity")
require_text(chrome_source
    "constexpr float RightGroupOffset = 402.0f;"
    "status right-group collision boundary")
require_text(chrome_source
    "Ellipsize(statusText_, StatusTextCapacity(width_))"
    "bounded status footer summary")
forbid_text(chrome_source
    "DrawText(\n            statusText_,\n            105.0f"
    "unbounded status footer rendering")
require_text(chrome_source
    "std::vector<std::string> WrapTextLines("
    "full-status wrapping helper")
require_text(chrome_source
    "const std::string outputStatus = statusText_.empty()"
    "Output drawer raw status source")
require_text(chrome_source
    "const auto lines = WrapTextLines("
    "Output drawer wrapped status presentation")
require_text(chrome_source
    "outputStatus,"
    "raw status passed to Output wrapping")

message(STATUS "Scene UI Gate 3 Inspector, Hierarchy and typography contract passed")
