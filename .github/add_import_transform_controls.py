from pathlib import Path
p=Path('Studio/src/StudioApplication.cpp')
s=p.read_text(encoding='utf-8')

old='''        std::size_t selectedMaterial = 0;\n        std::size_t selectedAnimation = 0;\n    };'''
new='''        std::size_t selectedMaterial = 0;\n        std::size_t selectedAnimation = 0;\n        XMFLOAT3 positionOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);\n        XMFLOAT3 rotationDegrees = XMFLOAT3(0.0f, 0.0f, 0.0f);\n    };'''
if old not in s: raise SystemExit('state marker missing')
s=s.replace(old,new,1)

old='''    wi::gui::Label creatorImportTransformLabel;\n    wi::gui::Label creatorImportHelpLabel;'''
new='''    wi::gui::Label creatorImportTransformLabel;\n    renegade::studio::RenegadeTextInputField creatorImportPositionX;\n    renegade::studio::RenegadeTextInputField creatorImportPositionY;\n    renegade::studio::RenegadeTextInputField creatorImportPositionZ;\n    renegade::studio::RenegadeTextInputField creatorImportRotationX;\n    renegade::studio::RenegadeTextInputField creatorImportRotationY;\n    renegade::studio::RenegadeTextInputField creatorImportRotationZ;\n    wi::gui::Label creatorImportHelpLabel;'''
if old not in s: raise SystemExit('widget marker missing')
s=s.replace(old,new,1)

marker='''    void RefreshCreatorImportMaterialReadout()\n    {'''
helper=r'''    void ApplyCreatorImportPreviewTransform()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.active ||
            creatorModelImporter.previewRoot == wi::ecs::INVALID_ENTITY)
            return;
        auto& scene = session->Scenes().GetScene();
        auto* transform = scene.transforms.GetComponent(creatorModelImporter.previewRoot);
        if (transform == nullptr)
            return;
        auto next = renegade::bridge::CaptureTransform(*transform);
        next.translation = XMFLOAT3(
            creatorModelImporter.positionOffset.x,
            CreatorImportStageHeight + creatorModelImporter.positionOffset.y,
            creatorModelImporter.positionOffset.z);
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(creatorModelImporter.rotationDegrees.x),
            XMConvertToRadians(creatorModelImporter.rotationDegrees.y),
            XMConvertToRadians(creatorModelImporter.rotationDegrees.z));
        XMStoreFloat4(&next.rotation, q);
        session->Commands().Execute(
            std::make_unique<renegade::bridge::SetTransformCommand>(
                scene, creatorModelImporter.previewRoot, next));
    }

'''
if marker not in s: raise SystemExit('helper marker missing')
s=s.replace(marker,helper+marker,1)

old='''        creatorImportTransformLabel.Create("TRANSFORM // PREVIEW");\n        importScaleModeCombo_.Create("Scale Mode");'''
new=r'''        creatorImportTransformLabel.Create("TRANSFORM // PREVIEW");
        const auto createImportTransformField = [](
            RenegadeTextInputField& field,
            const char* name,
            const char* description,
            const bool rotation,
            const int axis)
        {
            field.Create(name);
            field.SetDescription(description);
            field.SetValue(0.0f);
            field.OnInputAccepted([rotation, axis](const wi::gui::EventArgs& args)
            {
                XMFLOAT3& target = rotation
                    ? creatorModelImporter.rotationDegrees
                    : creatorModelImporter.positionOffset;
                if (axis == 0) target.x = args.fValue;
                if (axis == 1) target.y = args.fValue;
                if (axis == 2) target.z = args.fValue;
                ApplyCreatorImportPreviewTransform();
            });
        };
        createImportTransformField(
            creatorImportPositionX, "Import Position X", "POS X: ", false, 0);
        createImportTransformField(
            creatorImportPositionY, "Import Position Y", "POS Y: ", false, 1);
        createImportTransformField(
            creatorImportPositionZ, "Import Position Z", "POS Z: ", false, 2);
        createImportTransformField(
            creatorImportRotationX, "Import Rotation X", "ROT X: ", true, 0);
        createImportTransformField(
            creatorImportRotationY, "Import Rotation Y", "ROT Y: ", true, 1);
        createImportTransformField(
            creatorImportRotationZ, "Import Rotation Z", "ROT Z: ", true, 2);

        importScaleModeCombo_.Create("Scale Mode");'''
if old not in s: raise SystemExit('create controls marker missing')
s=s.replace(old,new,1)

old='''            static_cast<wi::gui::Widget*>(&creatorImportTransformLabel),\n            static_cast<wi::gui::Widget*>(&importScaleModeCombo_),'''
new='''            static_cast<wi::gui::Widget*>(&creatorImportTransformLabel),\n            static_cast<wi::gui::Widget*>(&creatorImportPositionX),\n            static_cast<wi::gui::Widget*>(&creatorImportPositionY),\n            static_cast<wi::gui::Widget*>(&creatorImportPositionZ),\n            static_cast<wi::gui::Widget*>(&creatorImportRotationX),\n            static_cast<wi::gui::Widget*>(&creatorImportRotationY),\n            static_cast<wi::gui::Widget*>(&creatorImportRotationZ),\n            static_cast<wi::gui::Widget*>(&importScaleModeCombo_),'''
if old not in s: raise SystemExit('add widget marker missing')
s=s.replace(old,new,1)

old='''        creatorImportTransformLabel.SetPos(XMFLOAT2(12.0f, 160.0f));\n        creatorImportTransformLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));\n        importScaleModeCombo_.SetPos(XMFLOAT2(12.0f, 186.0f));\n        importScaleModeCombo_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));\n        creatorImportMaterialLabel.SetPos(XMFLOAT2(12.0f, 232.0f));\n        creatorImportMaterialLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));\n        creatorImportMaterialCombo.SetPos(XMFLOAT2(12.0f, 258.0f));\n        creatorImportMaterialCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));\n        creatorImportMaterialReadout.SetPos(XMFLOAT2(12.0f, 292.0f));\n        creatorImportMaterialReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 112.0f));\n        creatorImportAnimationLabel.SetPos(XMFLOAT2(12.0f, 420.0f));\n        creatorImportAnimationLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));\n        creatorImportAnimationCombo.SetPos(XMFLOAT2(12.0f, 446.0f));\n        creatorImportAnimationCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));\n        creatorImportAnimationReadout.SetPos(XMFLOAT2(12.0f, 480.0f));\n        creatorImportAnimationReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 76.0f));\n        importScaleApplyButton_.SetPos(XMFLOAT2(12.0f, 624.0f));'''
new=r'''        creatorImportTransformLabel.SetPos(XMFLOAT2(12.0f, 160.0f));
        creatorImportTransformLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        const float transformGap = 4.0f;
        const float transformWidth =
            (importScalePanelWidth - 24.0f - transformGap * 2.0f) / 3.0f;
        const auto layoutTransformRow = [transformWidth, transformGap](
            wi::gui::Widget& x, wi::gui::Widget& y, wi::gui::Widget& z,
            const float rowY)
        {
            x.SetPos(XMFLOAT2(12.0f, rowY));
            y.SetPos(XMFLOAT2(12.0f + transformWidth + transformGap, rowY));
            z.SetPos(XMFLOAT2(
                12.0f + (transformWidth + transformGap) * 2.0f, rowY));
            x.SetSize(XMFLOAT2(transformWidth, 28.0f));
            y.SetSize(XMFLOAT2(transformWidth, 28.0f));
            z.SetSize(XMFLOAT2(transformWidth, 28.0f));
        };
        layoutTransformRow(
            creatorImportPositionX, creatorImportPositionY,
            creatorImportPositionZ, 186.0f);
        layoutTransformRow(
            creatorImportRotationX, creatorImportRotationY,
            creatorImportRotationZ, 220.0f);
        importScaleModeCombo_.SetPos(XMFLOAT2(12.0f, 254.0f));
        importScaleModeCombo_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportMaterialLabel.SetPos(XMFLOAT2(12.0f, 300.0f));
        creatorImportMaterialLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        creatorImportMaterialCombo.SetPos(XMFLOAT2(12.0f, 326.0f));
        creatorImportMaterialCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportMaterialReadout.SetPos(XMFLOAT2(12.0f, 360.0f));
        creatorImportMaterialReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 104.0f));
        creatorImportAnimationLabel.SetPos(XMFLOAT2(12.0f, 478.0f));
        creatorImportAnimationLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        creatorImportAnimationCombo.SetPos(XMFLOAT2(12.0f, 504.0f));
        creatorImportAnimationCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportAnimationReadout.SetPos(XMFLOAT2(12.0f, 538.0f));
        creatorImportAnimationReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 70.0f));
        importScaleApplyButton_.SetPos(XMFLOAT2(12.0f, 624.0f));'''
if old not in s: raise SystemExit('layout marker missing')
s=s.replace(old,new,1)

old='''        importScaleModeCombo_.SetSelectedWithoutCallback(0);\n\n        creatorImportMaterialCombo.ClearItems();'''
new='''        importScaleModeCombo_.SetSelectedWithoutCallback(0);\n        creatorModelImporter.positionOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);\n        creatorModelImporter.rotationDegrees = XMFLOAT3(0.0f, 0.0f, 0.0f);\n        creatorImportPositionX.SetValue(0.0f);\n        creatorImportPositionY.SetValue(0.0f);\n        creatorImportPositionZ.SetValue(0.0f);\n        creatorImportRotationX.SetValue(0.0f);\n        creatorImportRotationY.SetValue(0.0f);\n        creatorImportRotationZ.SetValue(0.0f);\n\n        creatorImportMaterialCombo.ClearItems();'''
if old not in s: raise SystemExit('show reset marker missing')
s=s.replace(old,new,1)

p.write_text(s,encoding='utf-8')
