from pathlib import Path


def replace_once(text, before, after, label):
    count = text.count(before)
    if count != 1:
        raise SystemExit(f'{label}: expected 1 match, found {count}')
    return text.replace(before, after, 1)


cpp_path = Path('Studio/src/StudioApplication.cpp')
cpp = cpp_path.read_text(encoding='utf-8')

before = '''                        hubMessageLabel_.font.params.color = HologramMuted;
                        hubMessageLabel_.SetText(
                            "PROJECT OPENING // " +
                            wi::helper::GetFileNameFromPath(descriptorPath));
                        OpenProjectDescriptor(descriptorPath);'''
after = '''                        RequestSceneReplacement(
                            [this, descriptorPath]()
                            {
                                hubMessageLabel_.font.params.color = HologramMuted;
                                hubMessageLabel_.SetText(
                                    "PROJECT OPENING // " +
                                    wi::helper::GetFileNameFromPath(descriptorPath));
                                OpenProjectDescriptor(descriptorPath);
                            });'''
cpp = replace_once(cpp, before, after, 'Open Project guard')

before = '''        OpenProjectDescriptor(recent[index].descriptorPath);'''
after = '''        const std::string descriptorPath = recent[index].descriptorPath;
        RequestSceneReplacement(
            [this, descriptorPath]()
            {
                OpenProjectDescriptor(descriptorPath);
            });'''
cpp = replace_once(cpp, before, after, 'Recent Project guard')

before = '''            [this, parentDirectory, projectName](uint64_t)
            {
                if (!session_->Projects().CreateProject('''
after = '''            [this, parentDirectory, projectName](uint64_t)
            {
                RequestSceneReplacement(
                    [this, parentDirectory, projectName]()
                    {
                        if (!session_->Projects().CreateProject('''
cpp = replace_once(cpp, before, after, 'Create Project guard open')

before = '''                hubMessageLabel_.SetText(
                    "PROJECT CREATED // " +
                    session_->Projects().CurrentProject().descriptorPath);
                selectedRecentProject_ = -1;
                RefreshProjectHub();
                SetProjectHubVisible(false);
            });'''
after = '''                        hubMessageLabel_.SetText(
                            "PROJECT CREATED // " +
                            session_->Projects().CurrentProject().descriptorPath);
                        selectedRecentProject_ = -1;
                        RefreshProjectHub();
                        SetProjectHubVisible(false);
                    });
            });'''
cpp = replace_once(cpp, before, after, 'Create Project guard close')

before = '''        ClearSelectionOutline();
        const bool saved = session_->SaveScene(currentPath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
        if (saved)
        {
            continuation();
        }'''
after = '''        SaveSceneAfterTransientCleanup(
            currentPath,
            [continuation = std::move(continuation)](const bool saved)
            {
                if (saved)
                {
                    continuation();
                }
            });'''
cpp = replace_once(cpp, before, after, 'Guard safe-save path')

before = '''            case RenegadeProjectHub::Action::ExitRenegade:
                if (exitRequestHandler_) exitRequestHandler_();
                break;'''
after = '''            case RenegadeProjectHub::Action::ExitRenegade:
                RequestExit();
                break;'''
cpp = replace_once(cpp, before, after, 'Hub exit guard')

before = '''    void StudioRenderPath::SetExitRequestHandler(std::function<void()> handler)
    {
        exitRequestHandler_ = std::move(handler);
    }
'''
after = '''    void StudioRenderPath::SetExitRequestHandler(std::function<void()> handler)
    {
        exitRequestHandler_ = std::move(handler);
    }

    void StudioRenderPath::RequestExit()
    {
        if (session_ == nullptr)
        {
            if (exitRequestHandler_)
                exitRequestHandler_();
            return;
        }

        RequestSceneReplacement(
            [this]()
            {
                if (exitRequestHandler_)
                    exitRequestHandler_();
            });
    }
'''
cpp = replace_once(cpp, before, after, 'Renderer RequestExit')

before = '''    void StudioApplication::SetExitRequestHandler(std::function<void()> handler)
    {
        renderer_.SetExitRequestHandler(std::move(handler));
    }
'''
after = '''    void StudioApplication::SetExitRequestHandler(std::function<void()> handler)
    {
        renderer_.SetExitRequestHandler(std::move(handler));
    }

    void StudioApplication::RequestExit()
    {
        renderer_.RequestExit();
    }
'''
cpp = replace_once(cpp, before, after, 'Application RequestExit')
cpp_path.write_text(cpp, encoding='utf-8')

header_path = Path('Studio/src/StudioApplication.h')
header = header_path.read_text(encoding='utf-8')
header = replace_once(
    header,
    '''        void BindSession(bridge::StudioSession& session) noexcept;
        void SetExitRequestHandler(std::function<void()> handler);''',
    '''        void BindSession(bridge::StudioSession& session) noexcept;
        void SetExitRequestHandler(std::function<void()> handler);
        void RequestExit();''',
    'Renderer RequestExit declaration')
header = replace_once(
    header,
    '''        void SetStartupScene(std::string filePath);
        void SetExitRequestHandler(std::function<void()> handler);
        void Initialize() override;''',
    '''        void SetStartupScene(std::string filePath);
        void SetExitRequestHandler(std::function<void()> handler);
        void RequestExit();
        void Initialize() override;''',
    'Application RequestExit declaration')
header_path.write_text(header, encoding='utf-8')

win_path = Path('Studio/src/main_Windows.cpp')
win = win_path.read_text(encoding='utf-8')
before = '''        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;'''
after = '''        case WM_CLOSE:
            if (application != nullptr && windowReadyForWicked)
            {
                application->RequestExit();
                return 0;
            }
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;'''
win = replace_once(win, before, after, 'WM_CLOSE guard')
before = '''    application->SetExitRequestHandler([window]() { PostMessageW(window, WM_CLOSE, 0, 0); });'''
after = '''    application->SetExitRequestHandler([window]() { DestroyWindow(window); });'''
win = replace_once(win, before, after, 'Exit continuation')
win_path.write_text(win, encoding='utf-8')
