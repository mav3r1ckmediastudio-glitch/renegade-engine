from pathlib import Path


def one(s, old, new, label):
    n=s.count(old)
    if n != 1:
        raise SystemExit(f'{label}: expected 1 match, got {n}')
    return s.replace(old,new,1)


def between(s, a, b, repl, label):
    i=s.find(a); j=s.find(b,i+1)
    if i<0 or j<0 or s.find(a,i+1)>=0:
        raise SystemExit(f'{label}: anchors invalid')
    return s[:i]+repl+s[j:]

p=Path('Studio/src/StudioApplication.h'); s=p.read_text()
s=one(s,'#include "RenegadeStudioChrome.h"\n','#include "RenegadeStudioChrome.h"\n#include "RenegadeProjectHub.h"\n','include')
s=one(s,'        void BindSession(bridge::StudioSession& session) noexcept;\n','        void BindSession(bridge::StudioSession& session) noexcept;\n        void SetExitRequestHandler(std::function<void()> handler);\n','renderer setter')
s=one(s,'        wi::gui::Window projectHubPanel_;\n','        wi::gui::Window projectHubPanel_;\n        RenegadeProjectHub projectHubChrome_;\n','chrome field')
s=one(s,'        wi::gui::Label hubMessageLabel_;\n','        wi::gui::Label hubMessageLabel_;\n        RenegadeTextInputField hubNewProjectNameInput_;\n        RenegadeButton hubNewProjectConfirmButton_;\n        RenegadeButton hubNewProjectCancelButton_;\n','modal fields')
s=one(s,'        bool projectHubVisible_ = true;\n','        bool projectHubVisible_ = true;\n        bool hubNewProjectMode_ = false;\n        std::function<void()> exitRequestHandler_;\n','hub state')
s=one(s,'        void SetStartupScene(std::string filePath);\n        void Initialize() override;\n','        void SetStartupScene(std::string filePath);\n        void SetExitRequestHandler(std::function<void()> handler);\n        void Initialize() override;\n','app setter')
p.write_text(s)

p=Path('Studio/src/StudioApplication.cpp'); s=p.read_text()
s=one(s,'#include "StudioApplication.h"\n','#include "StudioApplication.h"\n#include "StudioUserPreferences.h"\n','prefs include')
anchor='''    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }
'''
s=one(s,anchor,anchor+'''\n    void StudioRenderPath::SetExitRequestHandler(std::function<void()> handler)\n    {\n        exitRequestHandler_ = std::move(handler);\n    }\n''','renderer setter impl')

tail='''        hubMessageLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubMessageLabel_);
    }
'''
extra='''        hubMessageLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubMessageLabel_);
        projectHubPanel_.SetVisible(false);

        projectHubChrome_.Create();
        const auto savedIdentity = StudioUserPreferences::LoadDeveloperIdentity();
        projectHubChrome_.SetDeveloperIdentity(savedIdentity.has_value()
            ? fs::path(*savedIdentity).u8string() : std::string("DEVELOPER"));
        projectHubChrome_.SetStatusProvider([this]() { return hubMessageLabel_.GetText(); });
        projectHubChrome_.OnRecentProjectSelected([this](std::size_t index) { SelectRecentProject(index); });
        projectHubChrome_.OnAction([this](RenegadeProjectHub::Action action)
        {
            switch (action)
            {
            case RenegadeProjectHub::Action::NewProject:
                hubNewProjectMode_ = true;
                projectHubChrome_.SetNewProjectMode(true);
                hubNewProjectNameInput_.SetText("New Renegade Project");
                hubNewProjectNameInput_.SetVisible(true);
                hubNewProjectConfirmButton_.SetVisible(true);
                hubNewProjectCancelButton_.SetVisible(true);
                break;
            case RenegadeProjectHub::Action::OpenProject: OpenProject(); break;
            case RenegadeProjectHub::Action::OpenSelectedProject: OpenSelectedRecentProject(); break;
            case RenegadeProjectHub::Action::BackToEditor:
                if (session_ && session_->Projects().HasProject()) SetProjectHubVisible(false);
                break;
            case RenegadeProjectHub::Action::ExitRenegade:
                if (exitRequestHandler_) exitRequestHandler_();
                break;
            case RenegadeProjectHub::Action::CancelNewProject:
                hubNewProjectMode_ = false;
                projectHubChrome_.SetNewProjectMode(false);
                hubNewProjectNameInput_.SetVisible(false);
                hubNewProjectConfirmButton_.SetVisible(false);
                hubNewProjectCancelButton_.SetVisible(false);
                break;
            }
        });
        projectHubChrome_.SetVisible(projectHubVisible_);
        GetGUI().AddWidget(&projectHubChrome_);

        hubNewProjectNameInput_.Create("Hub New Project Name");
        hubNewProjectNameInput_.SetPlaceholder("PROJECT NAME");
        hubNewProjectNameInput_.SetText("New Renegade Project");
        hubNewProjectNameInput_.SetCancelInputEnabled(false);
        hubNewProjectNameInput_.OnInputAccepted([this](const wi::gui::EventArgs&) { CreateProject(); });
        hubNewProjectNameInput_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectNameInput_);
        hubNewProjectConfirmButton_.Create("Hub Create Project Confirm");
        hubNewProjectConfirmButton_.SetText("CREATE PROJECT");
        hubNewProjectConfirmButton_.OnClick([this](const wi::gui::EventArgs&) { CreateProject(); });
        hubNewProjectConfirmButton_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectConfirmButton_);
        hubNewProjectCancelButton_.Create("Hub Create Project Cancel");
        hubNewProjectCancelButton_.SetText("CANCEL");
        hubNewProjectCancelButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            hubNewProjectMode_ = false;
            projectHubChrome_.SetNewProjectMode(false);
            hubNewProjectNameInput_.SetVisible(false);
            hubNewProjectConfirmButton_.SetVisible(false);
            hubNewProjectCancelButton_.SetVisible(false);
        });
        hubNewProjectCancelButton_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectCancelButton_);
    }
'''
s=one(s,tail,extra,'CreateProjectHub tail')

layout='''        studioChrome_.SetLayout(width, height);
        projectHubChrome_.SetLayout(width, height);
        const XMFLOAT4 n = projectHubChrome_.NewProjectInputBounds();
        hubNewProjectNameInput_.SetPos(XMFLOAT2(n.x,n.y));
        hubNewProjectNameInput_.SetSize(XMFLOAT2(n.z-n.x,n.w-n.y));
        const XMFLOAT4 c = projectHubChrome_.NewProjectConfirmBounds();
        hubNewProjectConfirmButton_.SetPos(XMFLOAT2(c.x,c.y));
        hubNewProjectConfirmButton_.SetSize(XMFLOAT2(c.z-c.x,c.w-c.y));
        const XMFLOAT4 x = projectHubChrome_.NewProjectCancelBounds();
        hubNewProjectCancelButton_.SetPos(XMFLOAT2(x.x,x.y));
        hubNewProjectCancelButton_.SetSize(XMFLOAT2(x.z-x.x,x.w-x.y));
'''
s=one(s,'        studioChrome_.SetLayout(width, height);\n',layout,'layout')
s=one(s,'        const std::string projectName = projectNameInput_.GetText();\n','        const std::string projectName = hubNewProjectNameInput_.GetText();\n','name input')

refresh='''    void StudioRenderPath::RefreshProjectHub()
    {
        if (!session_) return;
        const auto& projects = session_->Projects().RecentProjects();
        if (projects.empty()) selectedRecentProject_ = -1;
        else if (selectedRecentProject_ < 0 || static_cast<std::size_t>(selectedRecentProject_) >= projects.size())
            selectedRecentProject_ = 0;

        for (auto& button : recentProjectButtons_) button.SetVisible(false);
        launchProjectButton_.SetEnabled(selectedRecentProject_ >= 0);
        continueProjectButton_.SetVisible(false);

        std::vector<RenegadeProjectHub::ProjectEntry> entries;
        entries.reserve(projects.size());
        for (const auto& recent : projects)
        {
            RenegadeProjectHub::ProjectEntry entry;
            entry.name = recent.name;
            entry.descriptorPath = recent.descriptorPath;
            bridge::ProjectMetadata meta;
            std::string error;
            entry.descriptorValid = session_->Projects().InspectProject(recent.descriptorPath, meta, error);
            if (entry.descriptorValid)
            {
                if (!meta.name.empty()) entry.name = meta.name;
                entry.rootPath = meta.rootPath;
                entry.startupScene = meta.startupScene;
                entry.formatVersion = meta.formatVersion;
            }
            entries.push_back(std::move(entry));
        }
        projectHubChrome_.SetProjects(std::move(entries), selectedRecentProject_);
        if (session_->Projects().HasProject())
            projectHubChrome_.SetCurrentProject(session_->Projects().CurrentProject().name, true);
        else
            projectHubChrome_.SetCurrentProject({}, false);
    }

'''
s=between(s,'    void StudioRenderPath::RefreshProjectHub()\n','    void StudioRenderPath::ApplySelectedTransformValue(\n',refresh,'refresh')

s=one(s,'        projectHubPanel_.SetVisible(visible);\n','''        projectHubPanel_.SetVisible(false);
        projectHubChrome_.SetVisible(visible);
        if (!visible)
        {
            hubNewProjectMode_ = false;
            projectHubChrome_.SetNewProjectMode(false);
        }
        hubNewProjectNameInput_.SetVisible(visible && hubNewProjectMode_);
        hubNewProjectConfirmButton_.SetVisible(visible && hubNewProjectMode_);
        hubNewProjectCancelButton_.SetVisible(visible && hubNewProjectMode_);
''','visibility')

init='    void StudioApplication::Initialize()\n'
s=one(s,init,'''    void StudioApplication::SetExitRequestHandler(std::function<void()> handler)
    {
        renderer_.SetExitRequestHandler(std::move(handler));
    }

'''+init,'app setter impl')
p.write_text(s)

p=Path('Studio/src/main_Windows.cpp'); s=p.read_text()
s=one(s,'    application->SetWindow(window);\n    SetWindowTextW(window, GraphicsBackendTitle());\n','''    application->SetWindow(window);
    application->SetExitRequestHandler([window]() { PostMessageW(window, WM_CLOSE, 0, 0); });
    SetWindowTextW(window, GraphicsBackendTitle());
''','platform close')
p.write_text(s)

p=Path('Studio/CMakeLists.txt'); s=p.read_text()
s=one(s,'    src/RenegadeStudioChrome.h\n','    src/RenegadeStudioChrome.h\n    src/RenegadeProjectHub.cpp\n    src/RenegadeProjectHub.h\n','cmake sources')
anchor="# Gate 2A plays the owner's original MP4 directly through Windows Media\n"
block='''# Gate 3 concept plate is owner media. CI can omit it; owner packaging injects it.
set(RENEGADE_PROJECT_HUB_CONCEPT
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/renegade-project-hub-concept.jpg")
if(EXISTS "${RENEGADE_PROJECT_HUB_CONCEPT}")
    add_custom_command(TARGET RenegadeStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENEGADE_PROJECT_HUB_CONCEPT}"
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui/renegade-project-hub-concept.jpg"
        VERBATIM)
endif()

'''
s=one(s,anchor,block+anchor,'cmake plate')
p.write_text(s)

for q in [Path('.github/workflows/pr58-gate3-concept-rework.yml'), Path('Tools/pr58_gate3_concept_rework.py')]:
    if q.exists(): q.unlink()
