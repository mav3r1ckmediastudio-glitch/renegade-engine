from pathlib import Path
p=Path('Studio/src/StudioApplication.cpp')
s=p.read_text(encoding='utf-8')
old='''        importScalePanel_.Create(\n            "Model Import Workspace",\n            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);\n        importScalePanel_.SetVisible(false);\n\n        importScaleTitleLabel_.Create("MODEL IMPORTER // PREVIEW BEFORE COMMIT");'''
new='''        importScalePanel_.Create(\n            "Model Import Workspace",\n            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);\n        // Register the importer as a real Studio GUI window before attaching\n        // children. Window::AddWidget inherits the parent's enabled/visible\n        // state, so hiding it first makes the controls unreachable.\n        GetGUI().AddWidget(&importScalePanel_);\n\n        importScaleTitleLabel_.Create("MODEL IMPORTER // PREVIEW BEFORE COMMIT");'''
if old not in s: raise SystemExit('create-panel marker missing')
s=s.replace(old,new,1)
old='''        {\n            widget->SetShadowRadius(0.0f);\n            importScalePanel_.AddWidget(widget);\n        }\n    }\n\n    void StudioRenderPath::ApplyRenegadeTheme()'''
new='''        {\n            widget->SetShadowRadius(0.0f);\n            importScalePanel_.AddWidget(widget);\n        }\n        // Hide only after all child controls have inherited an enabled parent.\n        importScalePanel_.SetVisible(false);\n    }\n\n    void StudioRenderPath::ApplyRenegadeTheme()'''
if old not in s: raise SystemExit('panel-end marker missing')
s=s.replace(old,new,1)
old='''        importScalePanel_.SetPos(XMFLOAT2(\n            viewportBounds_.x +\n                (viewportBounds_.z - viewportBounds_.x) * 0.5f -\n                importScalePanelWidth * 0.5f,\n            viewportBounds_.y + 24.0f));'''
new='''        // GGMAX-style task workspace: keep the preview unobstructed and dock\n        // the importer controls down the right side of the preview viewport.\n        importScalePanel_.SetPos(XMFLOAT2(\n            std::max(\n                viewportBounds_.x + 12.0f,\n                viewportBounds_.z - importScalePanelWidth - 12.0f),\n            viewportBounds_.y + 12.0f));'''
if old not in s: raise SystemExit('panel-position marker missing')
s=s.replace(old,new,1)
p.write_text(s,encoding='utf-8')
