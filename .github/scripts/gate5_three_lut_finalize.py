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


# Gate 5 proof set: use three representative built-ins while retaining the
# complete custom-LUT workflow. The remaining built-ins can be expanded after
# the end-to-end authoring/package path is proven.
path = "EngineBridge/include/renegade/bridge/RenderLutService.h"
text = read(path)
text = replace_once(
    text,
    "inline constexpr std::size_t BuiltInColorGradingLutCount = 50;",
    "inline constexpr std::size_t BuiltInColorGradingLutCount = 3;",
    "three-LUT built-in count")
write(path, text)

path = "Studio/CMakeLists.txt"
text = read(path)
old = '''set(RENEGADE_BUILTIN_LUT_ARCHIVE
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
new = '''set(RENEGADE_BUILTIN_LUT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/luts")
foreach(RENEGADE_LUT_INDEX IN ITEMS 1 2 3)
    if(NOT EXISTS "${RENEGADE_BUILTIN_LUT_DIR}/${RENEGADE_LUT_INDEX}.png")
        message(FATAL_ERROR "Gate 5 built-in LUT ${RENEGADE_LUT_INDEX}.png is missing")
    endif()
endforeach()
add_custom_command(TARGET RenegadeStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/luts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RENEGADE_BUILTIN_LUT_DIR}/1.png"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/luts/1.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RENEGADE_BUILTIN_LUT_DIR}/2.png"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/luts/2.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RENEGADE_BUILTIN_LUT_DIR}/3.png"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/luts/3.png"
    VERBATIM
)

'''
text = replace_once(text, old, new, "direct built-in LUT copy")
write(path, text)

path = "docs/PHASE5_GATE5_POST_PROCESSING.md"
text = read(path)
text = replace_once(
    text,
    "- 50 Renegade built-in 256x16 RGBA PNG LUTs,",
    "- 3 representative Renegade built-in 256x16 RGBA PNG LUTs for Gate 5 proof (library expansion follows after end-to-end validation),",
    "three-LUT gate contract")
write(path, text)

path = "Tests/Phase5Gate5SourceContract.cmake"
text = read(path)
text = replace_once(
    text,
    'set(lut_archive "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/Renegade_Builtin_LUTs_1-50_RGBA.zip")',
    'set(lut_1 "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/1.png")\nset(lut_2 "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/2.png")\nset(lut_3 "${RENEGADE_SOURCE_DIR}/Studio/assets/luts/3.png")',
    "three-LUT source paths")
text = replace_once(
    text,
    '    "${settings_test}" "${lut_test}" "${gate_cmake}" "${gate_doc}" "${lut_archive}")',
    '    "${settings_test}" "${lut_test}" "${gate_cmake}" "${gate_doc}"\n    "${lut_1}" "${lut_2}" "${lut_3}")',
    "three-LUT required sources")
text = replace_once(
    text,
    '    "src/Phase5Gate5RenderWorkspace.cpp"\n    "Renegade_Builtin_LUTs_1-50_RGBA.zip"\n    "Content/luts")',
    '    "src/Phase5Gate5RenderWorkspace.cpp"\n    "assets/luts/1.png"\n    "assets/luts/2.png"\n    "assets/luts/3.png"\n    "copy_if_different"\n    "Content/luts")',
    "three-LUT Studio build contract")
text = replace_once(
    text,
    '    "50 Renegade built-in"',
    '    "3 representative Renegade built-in"',
    "three-LUT documentation contract")
write(path, text)

print("Gate 5 three-LUT finalization applied successfully")
