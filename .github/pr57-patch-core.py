from pathlib import Path


def load(path):
    return Path(path).read_text(encoding="utf-8")


def save(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path, old, new):
    text = load(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one target, found {count}")
    save(path, text.replace(old, new, 1))


replace_once(
    "Studio/src/RenegadeStudioChrome.h",
    """        void ClearCreatorAssetDragPreview();
""",
    """        void ClearCreatorAssetDragPreview();
        bool ConsumeCreatorAssetDragPreview(
            const bridge::StableId& assetId,
            bridge::PreparedReusableModelPlacement& prepared,
            XMFLOAT3& position,
            float& scale);
        [[nodiscard]] bool CreatorAssetDragPreviewBlocksSave() noexcept;
""",
)

replace_once(
    "EngineBridge/src/AssetBrowserService.cpp",
    """                if (!directory &&
                    lowerFilename.size() >= std::char_traits<char>::length(ThumbnailSuffix) &&
                    lowerFilename.compare(
                        lowerFilename.size() - std::char_traits<char>::length(ThumbnailSuffix),
                        std::char_traits<char>::length(ThumbnailSuffix),
                        ThumbnailSuffix) == 0)
                {
                    continue;
                }

                AssetEntry entry;
""",
    """                if (!directory &&
                    lowerFilename.size() >= std::char_traits<char>::length(ThumbnailSuffix) &&
                    lowerFilename.compare(
                        lowerFilename.size() - std::char_traits<char>::length(ThumbnailSuffix),
                        std::char_traits<char>::length(ThumbnailSuffix),
                        ThumbnailSuffix) == 0)
                {
                    continue;
                }
                constexpr const char* ManagedProjectionSuffix = ".rasset.json";
                if (!directory &&
                    lowerFilename.size() >=
                        std::char_traits<char>::length(ManagedProjectionSuffix) &&
                    lowerFilename.compare(
                        lowerFilename.size() -
                            std::char_traits<char>::length(ManagedProjectionSuffix),
                        std::char_traits<char>::length(ManagedProjectionSuffix),
                        ManagedProjectionSuffix) == 0)
                {
                    continue;
                }

                AssetEntry entry;
""",
)

replace_once(
    "EngineBridge/src/AssetCatalogueService.cpp",
    """        bool IsInternalMetadataPath(const std::string& projectRelativePath)
        {
            const std::string extension = LowerAscii(
                fs::u8path(projectRelativePath).extension().u8string());
            return extension == ".rmeta";
        }
""",
    """        bool IsInternalMetadataPath(const std::string& projectRelativePath)
        {
            const std::string lowerPath = LowerAscii(projectRelativePath);
            const std::string extension = LowerAscii(
                fs::u8path(projectRelativePath).extension().u8string());
            constexpr const char* ManagedProjectionSuffix = ".rasset.json";
            return extension == ".rmeta" ||
                (lowerPath.size() >=
                    std::char_traits<char>::length(ManagedProjectionSuffix) &&
                 lowerPath.compare(
                    lowerPath.size() -
                        std::char_traits<char>::length(ManagedProjectionSuffix),
                    std::char_traits<char>::length(ManagedProjectionSuffix),
                    ManagedProjectionSuffix) == 0);
        }
""",
)

replace_once(
    "EngineBridge/src/CreatorAssetWorkflowService.cpp",
    """        bool ResolveCreatorDestination(
""",
    """        bool IsCreatorManagedSidecar(const std::string& relativePath)
        {
            const std::string name = LowerAscii(
                fs::u8path(relativePath).filename().u8string());
            const auto hasSuffix = [&name](const char* suffix)
            {
                const std::size_t length = std::char_traits<char>::length(suffix);
                return name.size() >= length &&
                    name.compare(name.size() - length, length, suffix) == 0;
            };
            return hasSuffix(".rasset.json") ||
                hasSuffix(".thumbnail.png") ||
                hasSuffix(".rmeta") ||
                hasSuffix(".meta");
        }

        bool ResolveCreatorDestination(
""",
)

replace_once(
    "EngineBridge/src/CreatorAssetWorkflowService.cpp",
    """                    if (relative.empty() || activePaths.find(relative) != activePaths.end())
                        continue;

                    RefreshCandidate candidate;
""",
    """                    if (relative.empty() ||
                        activePaths.find(relative) != activePaths.end() ||
                        IsCreatorManagedSidecar(relative))
                    {
                        continue;
                    }

                    RefreshCandidate candidate;
""",
)

replace_once(
    "Tests/AssetBrowserTests.cpp",
    """    Touch(root / "Content/Models/Props/Hero.rasset");
    Touch(root / "Content/Models/Props/Hero.thumbnail.png");
""",
    """    Touch(root / "Content/Models/Props/Hero.rasset");
    Touch(root / "Content/Models/Props/Hero.rasset.json");
    Touch(root / "Content/Models/Props/Hero.thumbnail.png");
    Touch(root / "Content/Models/Props/Notes.json");
""",
)

replace_once(
    "Tests/AssetBrowserTests.cpp",
    """    if (FindAsset(crateFolder, "Hero.thumbnail.png") != nullptr)
    {
        return Fail(root, "governed asset thumbnail sidecar leaked into asset cards");
    }

    const auto unsafe = browser.Scan(
""",
    """    if (FindAsset(crateFolder, "Hero.thumbnail.png") != nullptr)
    {
        return Fail(root, "governed asset thumbnail sidecar leaked into asset cards");
    }
    if (FindAsset(crateFolder, "Hero.rasset.json") != nullptr)
    {
        return Fail(root, "managed rasset projection leaked into asset cards");
    }
    if (FindAsset(crateFolder, "Notes.json") == nullptr)
    {
        return Fail(root, "asset browser broadly hid arbitrary JSON");
    }

    const auto unsafe = browser.Scan(
""",
)

replace_once(
    "Studio/src/StudioApplication.h",
    """        void SaveScene();
        void SaveSceneAs(
""",
    """        void SaveSceneAfterTransientCleanup(
            const std::string& scenePath,
            std::function<void(bool)> completion = {});
        void SaveScene();
        void SaveSceneAs(
""",
)

print("PR57_CORE_PATCH_APPLIED")
