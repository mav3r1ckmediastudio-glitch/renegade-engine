#pragma once

// UI-09: import a creator-selected TrueType font to a governed LP08 product
// AND a Screen/Runtime-readable Content path. OTF remains unsupported by
// ScreenService schema v2 and is not advertised as importable.
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ResourceAssetService.h"
#include "renegade/bridge/ScreenService.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct CreatorScreenFontImportResult
    {
        bool succeeded = false;
        bool governedCommitted = false;
        StableId assetId;
        std::string runtimePath;
        std::string error;
    };

    class CreatorScreenFontImportService final
    {
    public:
        static constexpr std::uintmax_t MaximumFontBytes = 32u * 1024u * 1024u;

        [[nodiscard]] static CreatorScreenFontImportResult Import(
            const std::string& projectRoot,
            const StableId& projectId,
            const std::string& selectedPath)
        {
            namespace fs = std::filesystem;
            CreatorScreenFontImportResult result;
            if (projectRoot.empty() || !IsValidStableId(projectId) ||
                selectedPath.empty())
            {
                result.error = "An active project and TrueType font are required.";
                return result;
            }
            std::error_code ec;
            const fs::path root = fs::weakly_canonical(
                fs::absolute(fs::u8path(projectRoot), ec), ec);
            if (ec || !fs::is_directory(root, ec) || ec)
            {
                result.error = "The active Renegade project is unavailable.";
                return result;
            }
            ec.clear();
            const fs::path selected = fs::weakly_canonical(
                fs::absolute(fs::u8path(selectedPath), ec), ec);
            if (ec || !fs::is_regular_file(selected, ec) || ec)
            {
                result.error = "The selected font is unavailable.";
                return result;
            }
            if (DetectResourceSourceFormat(
                    selected.filename().generic_u8string()) !=
                ResourceSourceFormat::Ttf)
            {
                result.error =
                    "Only TrueType (.ttf) fonts are supported by this Screen Runtime.";
                return result;
            }
            const auto byteCount = fs::file_size(selected, ec);
            if (ec || byteCount == 0 || byteCount > MaximumFontBytes)
            {
                result.error = "The font is empty, inaccessible or exceeds 32 MiB.";
                return result;
            }

            // Stage under a unique stable ID; never overwrite another font,
            // whether its external file name matches or not.
            const StableId name = GenerateStableId();
            const fs::path retained = root / "SourceAssets" / "Fonts" /
                fs::u8path(name + ".ttf");
            const fs::path product = root / "Content" / "Fonts" /
                fs::u8path(name + ResourceAssetExtension);
            fs::create_directories(retained.parent_path(), ec);
            if (ec)
            {
                result.error = "Cannot create the project's SourceAssets/Fonts.";
                return result;
            }
            fs::create_directories(product.parent_path(), ec);
            if (ec)
            {
                result.error = "Cannot create the project's Content/Fonts.";
                return result;
            }

            // Bootstrap LC01 on older projects before the first governed
            // resource, matching the creator texture workflow.
            const fs::path registryPath = root / AssetRegistryDocumentName;
            if (!fs::exists(registryPath, ec))
            {
                if (ec)
                {
                    result.error = "Cannot inspect the project asset registry.";
                    return result;
                }
                AssetRegistry registry;
                registry.projectId = projectId;
                registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
                const auto written = WriteAssetRegistry(
                    root.generic_u8string(), registry);
                if (!written.success || !written.committed)
                {
                    result.error = "Cannot initialise asset registry: " +
                        written.message;
                    return result;
                }
            }

            fs::copy_file(selected, retained, fs::copy_options::none, ec);
            if (ec)
            {
                result.error = "Cannot retain font inside project: " + ec.message();
                return result;
            }
            ResourceAssetImportRequest request;
            request.projectRoot = root.generic_u8string();
            request.projectId = projectId;
            request.sourceProjectRelativePath =
                retained.lexically_relative(root).generic_u8string();
            request.assetProjectRelativePath =
                product.lexically_relative(root).generic_u8string();
            request.expectedFormat = ResourceSourceFormat::Ttf;
            const auto imported = ResourceAssetService().ImportResourceAsset(request);
            result.governedCommitted = imported.transaction.committed;
            result.assetId = imported.assetId;
            if (!imported.succeeded)
            {
                if (!imported.transaction.committed)
                {
                    std::error_code ignored;
                    fs::remove(retained, ignored);
                }
                result.error = "Governed font import failed: " + imported.error;
                return result;
            }

            // ScreenRenderer uses AddFontStyle(raw .ttf), not the LP08
            // packed .rasset. Copy the owned retained font through the same
            // journaled transaction API as other UI media.
            const fs::path mirror = fs::path("Content") / "UI" / "Fonts" /
                fs::u8path(result.assetId + ".ttf");
            const fs::path destination = root / mirror;
            fs::create_directories(destination.parent_path(), ec);
            if (ec || fs::exists(destination))
            {
                result.error = "Font imported, but the UI font destination is unavailable.";
                return result;
            }
            std::ifstream stream(retained, std::ios::binary);
            if (!stream)
            {
                result.error = "Font imported, but its retained source cannot be read.";
                return result;
            }
            const std::vector<std::uint8_t> bytes(
                std::istreambuf_iterator<char>(stream), {});
            if (stream.bad() || bytes.empty())
            {
                result.error = "Font imported, but its retained source is invalid.";
                return result;
            }
            ProjectDocumentWrite write;
            write.destinationPath = destination.generic_u8string();
            write.content = bytes;
            write.validator = [bytes](const std::string& staged,
                std::string& error)
            {
                std::ifstream input(fs::u8path(staged), std::ios::binary);
                if (!input)
                {
                    error = "Staged font is unreadable.";
                    return false;
                }
                const std::vector<std::uint8_t> stagedBytes(
                    std::istreambuf_iterator<char>(input), {});
                if (input.bad() || stagedBytes != bytes)
                {
                    error = "Staged font bytes do not match imported source.";
                    return false;
                }
                return true;
            };
            ProjectDocumentTransactionOptions opts;
            opts.allowedRoot = root.generic_u8string();
            opts.journalDirectory = (
                root / "Intermediate" / "Transactions").generic_u8string();
            const auto saved = ProjectDocumentTransaction().Execute(
                {std::move(write)}, std::move(opts));
            if (!saved.success)
            {
                result.error = "Font imported but UI resource commit failed: " +
                    saved.message;
                return result;
            }
            result.runtimePath = mirror.generic_u8string();
            std::string resolved;
            if (!ResolveScreenResourcePath(
                    root.generic_u8string(), result.runtimePath,
                    resolved, result.error))
                return result;
            result.succeeded = true;
            result.error.clear();
            return result;
        }
    };
}
