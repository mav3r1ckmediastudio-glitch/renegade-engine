from __future__ import annotations

import base64
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "Studio" / "src" / "StudioApplication.cpp"
CMAKE = ROOT / "Studio" / "CMakeLists.txt"
PLATE_JPG = ROOT / "Studio" / "assets" / "renegade-project-hub-concept.jpg"
PLATE_PNG = ROOT / "Studio" / "assets" / "renegade-project-hub-concept.png"


def fail(message: str) -> None:
    raise RuntimeError(message)


def match_brace(text: str, opening: int) -> int:
    if opening < 0 or text[opening] != "{":
        fail("invalid brace start")
    depth = 0
    i = opening
    in_string = False
    in_char = False
    escape = False
    line_comment = False
    block_comment = False
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if ch == "\n":
                line_comment = False
            i += 1
            continue
        if block_comment:
            if ch == "*" and nxt == "/":
                block_comment = False
                i += 2
                continue
            i += 1
            continue
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            i += 1
            continue
        if in_char:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "'":
                in_char = False
            i += 1
            continue
        if ch == "/" and nxt == "/":
            line_comment = True
            i += 2
            continue
        if ch == "/" and nxt == "*":
            block_comment = True
            i += 2
            continue
        if ch == '"':
            in_string = True
            i += 1
            continue
        if ch == "'":
            in_char = True
            i += 1
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    fail("unmatched brace")


def normalize_plate() -> str:
    if not PLATE_JPG.exists():
        fail(f"Gate 3 Hub plate is missing: {PLATE_JPG}")
    raw = PLATE_JPG.read_bytes()
    if not raw:
        fail("Gate 3 Hub plate is empty")

    # A previous connector write could leave the textual base64 representation
    # in the .jpg path. Accept that only when it decodes into a known image.
    if not (raw.startswith(b"\xff\xd8\xff") or raw.startswith(b"\x89PNG\r\n\x1a\n")):
        compact = b"".join(raw.split())
        try:
            decoded = base64.b64decode(compact, validate=True)
        except Exception as exc:
            fail(f"Hub plate is neither an image nor valid base64: {exc}")
        if not (decoded.startswith(b"\xff\xd8\xff") or decoded.startswith(b"\x89PNG\r\n\x1a\n")):
            fail("Decoded Hub plate does not contain JPEG or PNG bytes")
        raw = decoded

    if raw.startswith(b"\xff\xd8\xff"):
        if not raw.endswith(b"\xff\xd9"):
            fail("Hub JPEG is truncated (missing EOI marker)")
        PLATE_JPG.write_bytes(raw)
        if PLATE_PNG.exists():
            PLATE_PNG.unlink()
        return "jpg"

    # If the committed data is PNG, keep the bytes honest and update the two
    # runtime/build references rather than lying with a .jpg extension.
    PLATE_PNG.write_bytes(raw)
    PLATE_JPG.unlink()
    return "png"


def extract_old_open_project_body(source: str) -> str:
    marker = "openProjectButton_.OnClick"
    pos = source.find(marker)
    if pos < 0:
        fail("Could not find the pre-existing Open Project button callback")
    brace = source.find("{", pos)
    if brace < 0:
        fail("Could not find Open Project callback body")
    end = match_brace(source, brace)
    body = source[brace + 1 : end].strip("\n")
    if not body.strip():
        fail("Existing Open Project callback body is unexpectedly empty")
    # The event argument has historically not been needed. If that changes,
    # stop rather than copying code into an invalid context.
    callback_prefix = source[pos:brace]
    param_match = re.search(r"\((?:const\s+)?wi::gui::EventArgs(?:&)?\s+([A-Za-z_]\w*)\)", callback_prefix)
    if param_match and re.search(rf"\b{re.escape(param_match.group(1))}\b", body):
        fail("Existing Open Project callback now depends on EventArgs; manual refactor required")
    return body


def indent_body(body: str, prefix: str) -> str:
    lines = body.splitlines()
    nonempty = [line for line in lines if line.strip()]
    if not nonempty:
        return ""
    minimum = min(len(line) - len(line.lstrip()) for line in nonempty)
    return "\n".join(prefix + line[minimum:] if line.strip() else "" for line in lines)


def patch_open_project(source: str) -> str:
    body = extract_old_open_project_body(source)
    case = "case RenegadeProjectHub::Action::OpenProject:"
    start = source.find(case)
    if start < 0:
        fail("Custom Hub Open Project case not found")
    next_case = source.find("case RenegadeProjectHub::Action::", start + len(case))
    if next_case < 0:
        fail("Could not bound custom Hub Open Project case")
    old = source[start:next_case]

    # If this repair already landed, leave it idempotent.
    if "PR58 Gate 3: invoke the proven project-open flow directly" in old:
        return source

    # Keep the switch indentation from the existing case.
    line_start = source.rfind("\n", 0, start) + 1
    case_indent = source[line_start:start]
    body_indent = case_indent + "    "
    statement_indent = body_indent + "    "
    copied = indent_body(body, statement_indent)
    replacement = (
        case
        + "\n"
        + body_indent
        + "{\n"
        + statement_indent
        + "// PR58 Gate 3: invoke the proven project-open flow directly.\n"
        + statement_indent
        + "// Do not proxy through the now-hidden stock Hub button.\n"
        + copied
        + "\n"
        + statement_indent
        + "break;\n"
        + body_indent
        + "}\n"
        + case_indent
    )
    return source[:start] + replacement + source[next_case:]


def harden_plate_packaging(cmake: str, extension: str) -> str:
    expected = f"renegade-project-hub-concept.{extension}"
    other = "renegade-project-hub-concept.png" if extension == "jpg" else "renegade-project-hub-concept.jpg"
    cmake = cmake.replace(other, expected)

    old_comment = "# Gate 3 concept plate is owner media. CI can omit it; owner packaging injects it."
    if old_comment in cmake:
        cmake = cmake.replace(
            old_comment,
            "# Gate 3 Hub plate is a required Renegade Studio runtime asset.\n"
            "# Missing it is a build error: a black fallback Hub is not acceptable."
        )

    # Convert the previous silent conditional copy to a mandatory asset.
    pattern = re.compile(
        r"if\(EXISTS \"\$\{RENEGADE_PROJECT_HUB_CONCEPT\}\"\)\s*\n"
        r"(?P<body>\s*add_custom_command\(TARGET RenegadeStudio POST_BUILD.*?\n\s*endif\(\)\n)",
        re.DOTALL,
    )
    m = pattern.search(cmake)
    if m:
        block = m.group(0)
        command_start = block.find("add_custom_command")
        command = block[command_start:]
        command = re.sub(r"\n\s*endif\(\)\s*$", "", command)
        mandatory = (
            "if(NOT EXISTS \"${RENEGADE_PROJECT_HUB_CONCEPT}\")\n"
            "    message(FATAL_ERROR \"Required Gate 3 Hub plate is missing: ${RENEGADE_PROJECT_HUB_CONCEPT}\")\n"
            "endif()\n"
            + command
            + "\n"
        )
        cmake = cmake[:m.start()] + mandatory + cmake[m.end():]
    elif "Required Gate 3 Hub plate is missing" not in cmake:
        fail("Could not find the Gate 3 Hub plate CMake copy block")

    # Make the post-build copy self-checking so a green CI artifact cannot
    # silently omit or alter the plate.
    copy_line = (
        '            "${RENEGADE_PROJECT_HUB_CONCEPT}"\n'
        f'            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui/{expected}"'
    )
    if copy_line not in cmake:
        fail("Hub plate copy destination was not found after normalization")
    compare = (
        copy_line
        + "\n"
        + "        COMMAND ${CMAKE_COMMAND} -E compare_files\n"
        + '            "${RENEGADE_PROJECT_HUB_CONCEPT}"\n'
        + f'            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui/{expected}"'
    )
    if "compare_files" not in cmake[cmake.find("RENEGADE_PROJECT_HUB_CONCEPT"):cmake.find("# Gate 2A", cmake.find("RENEGADE_PROJECT_HUB_CONCEPT"))]:
        cmake = cmake.replace(copy_line, compare, 1)
    return cmake


def patch_runtime_extension(source: str, extension: str) -> str:
    expected = f"Content/ui/renegade-project-hub-concept.{extension}"
    other = "Content/ui/renegade-project-hub-concept.png" if extension == "jpg" else "Content/ui/renegade-project-hub-concept.jpg"
    return source.replace(other, expected)


def main() -> None:
    extension = normalize_plate()

    app = APP.read_text(encoding="utf-8")
    patched_app = patch_open_project(app)
    if patched_app != app:
        APP.write_text(patched_app, encoding="utf-8", newline="\n")

    hub_cpp = ROOT / "Studio" / "src" / "RenegadeProjectHub.cpp"
    hub = hub_cpp.read_text(encoding="utf-8")
    patched_hub = patch_runtime_extension(hub, extension)
    if patched_hub != hub:
        hub_cpp.write_text(patched_hub, encoding="utf-8", newline="\n")

    cmake = CMAKE.read_text(encoding="utf-8")
    patched_cmake = harden_plate_packaging(cmake, extension)
    if patched_cmake != cmake:
        CMAKE.write_text(patched_cmake, encoding="utf-8", newline="\n")

    # Assert final invariants before a commit can be produced.
    final_app = APP.read_text(encoding="utf-8")
    open_case = final_app[final_app.find("case RenegadeProjectHub::Action::OpenProject:"):]
    open_case = open_case[:open_case.find("case RenegadeProjectHub::Action::", 1)]
    if "PR58 Gate 3: invoke the proven project-open flow directly" not in open_case:
        fail("Open Project direct-flow assertion failed")

    final_cmake = CMAKE.read_text(encoding="utf-8")
    if "Required Gate 3 Hub plate is missing" not in final_cmake or "compare_files" not in final_cmake:
        fail("Hub plate packaging assertions failed")

    final_plate = PLATE_JPG if extension == "jpg" else PLATE_PNG
    raw = final_plate.read_bytes()
    if extension == "jpg" and not (raw.startswith(b"\xff\xd8\xff") and raw.endswith(b"\xff\xd9")):
        fail("Final JPEG validation failed")
    if extension == "png" and not raw.startswith(b"\x89PNG\r\n\x1a\n"):
        fail("Final PNG validation failed")

    print(f"Gate 3 owner repair prepared: plate={extension}, bytes={len(raw)}")


if __name__ == "__main__":
    main()
