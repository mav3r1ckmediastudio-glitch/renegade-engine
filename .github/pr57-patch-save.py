from pathlib import Path
import re


def load(path):
    return Path(path).read_text(encoding="utf-8")


def save(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def regex_once(path, pattern, replacement):
    text = load(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{path}: expected one regex target, found {count}")
    save(path, updated)


app = "Studio/src/StudioApplication.cpp"
save_block = r'''    void StudioRenderPath::SaveSceneAfterTransientCleanup(
        const std::string& scenePath,
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
            if (completion)
                completion(false);
            return;
        }

        if (detail::CreatorAssetDragPreviewBlocksSave())
        {
            detail::ClearCreatorAssetDragPreview();
            studioChrome_.SetStatusText(
                "SAVE // WAITING FOR TRANSIENT ASSET PREVIEW CLEANUP");
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, scenePath, completion](std::uint64_t)
                {
                    SaveSceneAfterTransientCleanup(scenePath, completion);
                });
            return;
        }

        ClearSelectionOutline();
        const bool saved = session_->SaveScene(scenePath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
        if (completion)
            completion(saved);
    }

    void StudioRenderPath::SaveScene()
    {
        if (session_ == nullptr)
            return;

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        StopSunPreview(true);
        SaveSceneAfterTransientCleanup(scenePath);
    }

    void StudioRenderPath::SaveSceneAs(
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
            if (completion)
                completion(false);
            return;
        }

        StopSunPreview(true);

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::SAVE;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(
            params,
            [this, completion](const std::string& selectedPath)
            {
                const std::string scenePath =
                    wi::helper::ForceExtension(selectedPath, "wiscene");
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, scenePath, completion](std::uint64_t)
                    {
                        SaveSceneAfterTransientCleanup(
                            scenePath,
                            completion);
                    });
            },
            [completion]()
            {
                if (completion)
                    completion(false);
            });
    }

'''
regex_once(
    app,
    r"    void StudioRenderPath::SaveScene\(\)\n    \{.*?\n    \}\n\n    void StudioRenderPath::SaveSceneAs\(\n        std::function<void\(bool\)> completion\)\n    \{.*?\n    \}\n\n    void StudioRenderPath::ReopenScene",
    save_block + "    void StudioRenderPath::ReopenScene",
)

text = load(app)
for required in (
    "CreatorAssetDragPreviewBlocksSave",
    "SaveSceneAfterTransientCleanup",
    "WAITING FOR TRANSIENT ASSET PREVIEW CLEANUP",
):
    if required not in text:
        raise RuntimeError(f"save-safety recovery missing: {required}")

print("PR57_SAVE_PATCH_APPLIED")
