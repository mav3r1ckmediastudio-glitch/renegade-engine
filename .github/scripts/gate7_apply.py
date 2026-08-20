from pathlib import Path
import re


def replace_once(path, before, after):
    text = path.read_text(encoding='utf-8')
    count = text.count(before)
    if count != 1:
        raise SystemExit(f'{path}: expected one seam, found {count}: {before[:100]!r}')
    path.write_text(text.replace(before, after, 1), encoding='utf-8')

# Prepared scene is private candidate state, so allow preparation services to
# enrich it before the one authoritative commit boundary.
path = Path('EngineBridge/include/renegade/bridge/SceneDocumentService.h')
before = '''        [[nodiscard]] const wi::scene::Scene* ReadOnlyScene() const noexcept
        {
            return scene_.get();
        }
'''
after = before + '''
        // Mutable preparation seam for background restoration work. This scene
        // is still detached from Studio's active document; callers must finish
        // all mutation before CommitPreparedOpen() adopts it.
        [[nodiscard]] wi::scene::Scene* MutablePreparedScene() noexcept
        {
            return scene_.get();
        }
'''
replace_once(path, before, after)

# Progress callback for the existing governed texture restore service.
path = Path('EngineBridge/include/renegade/bridge/MaterialTextureAssetService.h')
text = path.read_text(encoding='utf-8')
needle = '''    using MaterialTextureResourceLoader = std::function<wi::Resource(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error)>;
'''
if text.count(needle) != 1:
    raise SystemExit('MaterialTextureAssetService.h: loader seam missing')
text = text.replace(needle, needle + '''
    // Reports completed unique governed resource preparations. It is designed
    // for UI telemetry and may be invoked from a background job.
    using MaterialTextureRestoreProgress =
        std::function<void(std::size_t completed, std::size_t total)>;
''', 1)
old_sig = '''    [[nodiscard]] MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader = {});
'''
new_sig = '''    [[nodiscard]] MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader = {},
        MaterialTextureRestoreProgress progress = {});
'''
if text.count(old_sig) != 1:
    raise SystemExit('MaterialTextureAssetService.h: restore signature missing')
path.write_text(text.replace(old_sig, new_sig, 1), encoding='utf-8')

path = Path('EngineBridge/src/MaterialTextureAssetService.cpp')
text = path.read_text(encoding='utf-8')
old_sig = '''    MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader)
'''
new_sig = '''    MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader,
        MaterialTextureRestoreProgress progress)
'''
if text.count(old_sig) != 1:
    raise SystemExit('MaterialTextureAssetService.cpp: restore signature missing')
text = text.replace(old_sig, new_sig, 1)
needle = '''        result.uniqueAssetIds = uniqueTextureAssetIds.size();
'''
if text.count(needle) != 1:
    raise SystemExit('MaterialTextureAssetService.cpp: unique seam missing')
text = text.replace(needle, needle + '''        std::size_t progressedUnique = 0;
        if (progress)
            progress(0, result.uniqueAssetIds);
''', 1)
needle = '''                if (cached.preparedSuccessfully)
                    ++result.preparedUnique;
'''
if text.count(needle) != 1:
    raise SystemExit('MaterialTextureAssetService.cpp: preparation progress seam missing')
text = text.replace(needle, needle + '''                ++progressedUnique;
                if (progress)
                    progress(progressedUnique, result.uniqueAssetIds);
''', 1)
path.write_text(text, encoding='utf-8')

# Reuse Gate 5's project/scene transaction when a scene has already been
# prepared and enriched on the worker.
path = Path('EngineBridge/include/renegade/bridge/StudioSession.h')
text = path.read_text(encoding='utf-8')
start = text.index('        bool LoadScene(const std::string& filePath)\n')
end = text.index('        bool SaveScene(const std::string& filePath)\n', start)
old = text[start:end]
new = '''        bool CommitPendingProjectScene(PreparedSceneOpen prepared)
        {
            if (!projects_.HasPendingProject())
            {
                return documents_.CommitPreparedOpen(std::move(prepared));
            }

            if (!prepared.IsReady())
            {
                const bool ignored =
                    documents_.CommitPreparedOpen(std::move(prepared));
                (void)ignored;
                projects_.DiscardPendingProject();
                return false;
            }

            if (!projects_.CommitPendingProject())
            {
                scenes_.SetLastError(
                    "Project adoption failed after startup-scene validation: " +
                    projects_.LastError());
                projects_.DiscardPendingProject();
                return false;
            }

            return documents_.CommitPreparedOpen(std::move(prepared));
        }

        bool LoadScene(const std::string& filePath)
        {
            return CommitPendingProjectScene(documents_.PrepareOpen(filePath));
        }

'''
path.write_text(text[:start] + new + text[end:], encoding='utf-8')

# Studio target owns the overlay.
path = Path('Studio/CMakeLists.txt')
replace_once(path,
'''    src/RenegadeProjectHub.cpp
    src/RenegadeProjectHub.h
''',
'''    src/RenegadeProjectHub.cpp
    src/RenegadeProjectHub.h
    src/RenegadeProjectLoadingOverlay.cpp
    src/RenegadeProjectLoadingOverlay.h
''')

# Render-path contract and state.
path = Path('Studio/src/StudioApplication.h')
text = path.read_text(encoding='utf-8')
text = text.replace('#include <functional>\n', '#include <functional>\n#include <memory>\n', 1)
text = text.replace('#include "RenegadeProjectHub.h"\n', '#include "RenegadeProjectHub.h"\n#include "RenegadeProjectLoadingOverlay.h"\n', 1)
needle = '''        void OpenProjectDescriptor(const std::string& descriptorPath);
        void OpenSelectedRecentProject();
'''
replacement = '''        struct ProjectLoadOperation;
        void BeginProjectLoad(const std::string& descriptorPath);
        void CompleteProjectLoad(std::shared_ptr<ProjectLoadOperation> operation);
        void OpenProjectDescriptor(const std::string& descriptorPath);
        void OpenSelectedRecentProject();
'''
if text.count(needle) != 1:
    raise SystemExit('StudioApplication.h: project method seam missing')
text = text.replace(needle, replacement, 1)
needle = '''        RenegadeProjectHub projectHubChrome_;
'''
if text.count(needle) != 1:
    raise SystemExit('StudioApplication.h: hub member seam missing')
text = text.replace(needle, needle + '        RenegadeProjectLoadingOverlay projectLoadingOverlay_;\n', 1)
needle = '''        wi::jobsystem::context sceneOpenWorkload_;
        wi::jobsystem::context modelImportWorkload_;
'''
if text.count(needle) != 1:
    raise SystemExit('StudioApplication.h: workload seam missing')
text = text.replace(needle, '''        wi::jobsystem::context sceneOpenWorkload_;
        wi::jobsystem::context projectLoadWorkload_;
        wi::jobsystem::context modelImportWorkload_;
''', 1)
path.write_text(text, encoding='utf-8')

path = Path('Studio/src/StudioApplication.cpp')
text = path.read_text(encoding='utf-8')

# Detached operation state can safely cross the jobsystem boundary.
needle = '''namespace renegade::studio
{
    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
'''
operation = '''namespace renegade::studio
{
    struct StudioRenderPath::ProjectLoadOperation
    {
        std::string descriptorPath;
        std::string startupScenePath;
        bridge::ProjectMetadata project;
        bridge::PreparedSceneOpen preparedScene;
        bridge::MaterialTextureRestoreResult textureRestore;
        std::string error;
    };

    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
'''
if text.count(needle) != 1:
    raise SystemExit('StudioApplication.cpp: namespace operation seam missing')
text = text.replace(needle, operation, 1)

# Do not allow the process to disappear under a worker that is still preparing
# a detached project scene.
needle = '''    void StudioRenderPath::RequestExit()
    {
        if (session_ == nullptr)
'''
replacement = '''    void StudioRenderPath::RequestExit()
    {
        if (projectLoadingOverlay_.IsBlocking() &&
            projectLoadingOverlay_.CurrentPhase() != RenegadeProjectLoadingOverlay::Phase::Failed)
        {
            return;
        }
        if (session_ == nullptr)
'''
if text.count(needle) != 1:
    raise SystemExit('StudioApplication.cpp: exit seam missing')
text = text.replace(needle, replacement, 1)

# Register loader before Hub chrome: Wicked top-level GUI is rendered
# back-to-front, so the earliest registration is the front-most overlay.
needle = '''        projectHubPanel_.SetVisible(false);

        projectHubChrome_.Create();
'''
replacement = '''        projectHubPanel_.SetVisible(false);

        projectLoadingOverlay_.Create();
        projectLoadingOverlay_.OnReturnToHub([this]()
        {
            projectLoadingOverlay_.SetVisible(false);
            RefreshProjectHub();
            SetProjectHubVisible(true);
        });
        GetGUI().AddWidget(&projectLoadingOverlay_);

        projectHubChrome_.Create();
'''
if text.count(needle) != 1:
    raise SystemExit('StudioApplication.cpp: loader registration seam missing')
text = text.replace(needle, replacement, 1)

# Keep overlay geometry locked to the same canvas as the Hub.
pattern = re.compile(r'(\s+projectHubChrome_\.SetLayout\([^;]+;\n)', re.MULTILINE)
matches = list(pattern.finditer(text))
if len(matches) != 1:
    raise SystemExit(f'StudioApplication.cpp: expected one Hub SetLayout, found {len(matches)}')
segment = matches[0].group(1)
indent = re.match(r'(\s+)', segment).group(1)
# Reuse the exact argument expression by replacing the object name.
loader_segment = segment.replace('projectHubChrome_.SetLayout', 'projectLoadingOverlay_.SetLayout')
text = text[:matches[0].end()] + loader_segment + text[matches[0].end():]

# Replace the old synchronous project-open body with a real background prepare
# pipeline. Existing Create Project remains instant and intentionally bypasses
# this presentation.
start = text.index('    void StudioRenderPath::OpenProjectDescriptor(\n')
end = text.index('    void StudioRenderPath::OpenSelectedRecentProject()\n', start)
new_block = '''    void StudioRenderPath::BeginProjectLoad(
        const std::string& descriptorPath)
    {
        if (session_ == nullptr || descriptorPath.empty() ||
            projectLoadingOverlay_.IsBlocking() ||
            wi::jobsystem::IsBusy(projectLoadWorkload_))
        {
            return;
        }

        projectLoadingOverlay_.Begin(
            wi::helper::GetFileNameFromPath(descriptorPath));
        projectHubChrome_.SetVisible(false);
        hubNewProjectNameInput_.SetVisible(false);
        hubNewProjectConfirmButton_.SetVisible(false);
        hubNewProjectCancelButton_.SetVisible(false);

        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::ValidatingProject);
        if (!session_->Projects().OpenProject(descriptorPath))
        {
            projectLoadingOverlay_.Fail(
                "Project validation failed: " + session_->Projects().LastError());
            return;
        }

        auto operation = std::make_shared<ProjectLoadOperation>();
        operation->descriptorPath = descriptorPath;
        operation->startupScenePath = session_->Projects().StartupScenePath();
        operation->project = session_->Projects().PendingProject();
        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::PreparingScene);

        projectLoadWorkload_.priority = wi::jobsystem::Priority::Low;
        wi::jobsystem::Execute(
            projectLoadWorkload_,
            [this, operation](wi::jobsystem::JobArgs)
            {
                operation->preparedScene =
                    session_->Documents().PrepareOpen(operation->startupScenePath);
                if (!operation->preparedScene.IsReady())
                {
                    operation->error = operation->preparedScene.Error().empty()
                        ? "The startup scene could not be prepared."
                        : operation->preparedScene.Error();
                }
                else
                {
                    auto* candidate = operation->preparedScene.MutablePreparedScene();
                    if (candidate != nullptr)
                    {
                        projectLoadingOverlay_.SetPhase(
                            RenegadeProjectLoadingOverlay::Phase::RestoringAssets,
                            0, 0);
                        operation->textureRestore =
                            bridge::RestoreMaterialTextureBindings(
                                *candidate,
                                operation->project.rootPath,
                                operation->project.projectId,
                                {},
                                [this](const std::size_t completed,
                                    const std::size_t total)
                                {
                                    projectLoadingOverlay_.SetPhase(
                                        RenegadeProjectLoadingOverlay::Phase::RestoringAssets,
                                        completed, total);
                                });
                    }
                }

                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, operation](std::uint64_t)
                    {
                        CompleteProjectLoad(operation);
                    });
            });
    }

    void StudioRenderPath::CompleteProjectLoad(
        std::shared_ptr<ProjectLoadOperation> operation)
    {
        if (session_ == nullptr || !operation)
            return;

        if (!operation->error.empty())
        {
            session_->Projects().DiscardPendingProject();
            projectLoadingOverlay_.Fail(operation->error);
            return;
        }

        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::Finalising);
        ClearSelectionOutline();
        if (!session_->CommitPendingProjectScene(
                std::move(operation->preparedScene)))
        {
            SyncSelectionOutline();
            projectLoadingOverlay_.Fail(
                session_->Scenes().LastError().empty()
                    ? "The prepared project could not be adopted."
                    : session_->Scenes().LastError());
            return;
        }

        AdoptOpenedSceneCamera();
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        workspaceTitle_.SetText(
            "RENEGADE STUDIO // " +
            session_->Projects().CurrentProject().name);
        hubMessageLabel_.font.params.color = operation->textureRestore.succeeded
            ? HologramMuted : WarningAmber;
        hubMessageLabel_.SetText(operation->textureRestore.succeeded
            ? "PROJECT ONLINE // " + session_->Projects().CurrentProject().descriptorPath
            : "PROJECT ONLINE // GOVERNED RESOURCE WARNING // " +
                operation->textureRestore.error);
        selectedRecentProject_ = -1;
        RefreshProjectHub();
        RefreshAssetBrowser();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SetProjectHubVisible(false);
        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::Ready);
    }

    void StudioRenderPath::OpenProjectDescriptor(
        const std::string& descriptorPath)
    {
        BeginProjectLoad(descriptorPath);
    }

'''
text = text[:start] + new_block + text[end:]
path.write_text(text, encoding='utf-8')

print('Gate 7 source patch applied successfully')
