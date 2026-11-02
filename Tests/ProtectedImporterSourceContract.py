"""Regression guard: prevent silent source-level loss of the approved importer.

Source-level preflight only. It is NOT a substitute for FBX/OBJ Studio GUI,
model preview, asset commit, cold reopen, and packaged-binary testing.
"""
import argparse
from pathlib import Path

REQUIRED = {
    "Studio/src/StudioApplication.cpp": [
        'params.extensions = {"glb", "fbx", "obj"};',
        'modelImportAssignMap_.SetText("CHOOSE / REPLACE MAP");',
        'modelImportMaterial_.Create("Model material");',
        'modelImportWarnings_.Create("Source texture warnings");',
        'void StudioRenderPath::AssignModelImportTexture()',
    ],
    "Studio/src/StudioApplication.h": [
        "void AssignModelImportTexture();",
        "wi::gui::ComboBox modelImportMaterial_, modelImportMapSlot_;",
    ],
    "EngineBridge/src/ImportServiceModel.cpp": [
        "format == ModelSourceFormat::Glb || format == ModelSourceFormat::Obj;",
    ],
    "EngineBridge/src/ModelImportCandidateService.cpp": [
        "candidate.sourceFormat_ != ModelSourceFormat::Obj",
        "ImportModel_OBJ(candidate.sourcePath_, *candidate.scene_);",
    ],
    "EngineBridge/src/ModelImportCommitService.cpp": [
        "const bool isObj = candidate.SourceFormat() == ModelSourceFormat::Obj;",
    ],
    "EngineBridge/CMakeLists.txt": [
        "src/ModelImporterObj.cpp",
    ],
    "Tests/ModelImporterRebuild.cmake": [
        "RenegadeModelImportReliability",
        "RenegadeModelImportReliabilityColdReopen",
    ]
}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--repo",type=Path,required=True)
    args=parser.parse_args()
    errors=[]
    for path,needles in REQUIRED.items():
        target=args.repo/path
        if not target.is_file():
            errors.append(f"MISSING: {path}")
            continue
        content=target.read_text(encoding="utf-8-sig")
        for needle in needles:
            if needle not in content:
                errors.append(f"REMOVED: {path}: {needle}")
    if errors:
        raise SystemExit("OWNER-ACCEPTED IMPORTER SOURCE REGRESSION:\n"+"\n".join(errors))
    print("PASS: protected OBJ picker, bridge converter, asset transaction, material UI, and reliability suite. Runtime GUI proof remains mandatory.")

if __name__=="__main__":
    main()
