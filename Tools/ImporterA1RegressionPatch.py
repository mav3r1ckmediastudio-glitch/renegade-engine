#!/usr/bin/env python3
"""Fail-closed amendment of existing LP07 import tests in GitHub Actions only."""
from pathlib import Path
import subprocess

path = Path('Tests/ReusableAssetTests.cpp')
expected = 'c4a920a38e1fcd2e1a984447992090a6461ddd40'
actual = subprocess.check_output(['git', 'rev-parse', f'HEAD:{path.as_posix()}'], text=True).strip()
if actual != expected:
    raise RuntimeError(f'Refusing to patch unexpected test blob: {actual}')
src = path.read_text(encoding='utf-8')
changes = [
    (
        '    passed &= Check(!outsideSource.succeeded &&\n            outsideSource.error.find("SourceAssets") != std::string::npos,\n        "source outside SourceAssets did not fail closed before conversion");',
        '    passed &= Check(!outsideSource.succeeded &&\n            outsideSource.error.find("SourceAssets") != std::string::npos,\n        "source outside SourceAssets did not fail closed before conversion");\n    passed &= Check(outsideSource.diagnostics.attemptId != 0 &&\n            outsideSource.diagnostics.stage == CreatorImportStage::InputValidation &&\n            !outsideSource.diagnostics.sourceRetained &&\n            !outsideSource.diagnostics.transactionCommitted,\n        "source rejection reported inaccurate stage or side effects");'
    ),
    (
        '    passed &= Check(!existingProduct.succeeded &&\n            existingProduct.error.find("already exists") != std::string::npos,\n        "existing RAsset destination was not rejected before conversion");',
        '    passed &= Check(!existingProduct.succeeded &&\n            existingProduct.error.find("already exists") != std::string::npos,\n        "existing RAsset destination was not rejected before conversion");\n    passed &= Check(existingProduct.diagnostics.attemptId != 0 &&\n            existingProduct.diagnostics.stage == CreatorImportStage::InputValidation &&\n            !existingProduct.diagnostics.packageSerialized &&\n            !existingProduct.diagnostics.transactionCommitted,\n        "existing product rejection reported inaccurate stage or side effects");'
    ),
    (
        '    passed &= Check(!nonCanonicalSettings.succeeded &&\n            nonCanonicalSettings.error.find("canonical") != std::string::npos,\n        "non-canonical import settings were accepted");',
        '    passed &= Check(!nonCanonicalSettings.succeeded &&\n            nonCanonicalSettings.error.find("canonical") != std::string::npos,\n        "non-canonical import settings were accepted");\n    passed &= Check(nonCanonicalSettings.diagnostics.attemptId != 0 &&\n            nonCanonicalSettings.diagnostics.stage == CreatorImportStage::RecipeValidation &&\n            !nonCanonicalSettings.diagnostics.preparedSceneReady &&\n            !nonCanonicalSettings.diagnostics.transactionCommitted,\n        "invalid recipe reported inaccurate stage or side effects");'
    ),
    (
        '    passed &= Check(!crossProject.succeeded &&\n            crossProject.error.find("another project") != std::string::npos,\n        "cross-project registry did not fail closed before conversion");',
        '    passed &= Check(!crossProject.succeeded &&\n            crossProject.error.find("another project") != std::string::npos,\n        "cross-project registry did not fail closed before conversion");\n    passed &= Check(crossProject.diagnostics.attemptId != 0 &&\n            crossProject.diagnostics.stage == CreatorImportStage::RegistryValidation &&\n            !crossProject.diagnostics.transactionCommitted,\n        "cross-project registry rejection reported inaccurate stage or side effects");\n    passed &= Check(!fs::exists(root / "Content" / "Models" / "new.rasset"),\n        "rejected imports created an authoritative product");'
    ),
]
for old, new in changes:
    count = src.count(old)
    if count != 1:
        raise RuntimeError(f'Test anchor matched {count} times: {old[:90]!r}')
    src = src.replace(old, new, 1)
path.write_text(src, encoding='utf-8', newline='\n')
print('LP07 headless import regressions: 4 failure-stage assertions and no-product guard added')
