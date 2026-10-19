#pragma once

// UI editor-only preferences: never serialized into Screen/Flow documents.
// Malformed, stale or oversized config resets to safe defaults.
#include "ScreenEditorPanelLayout.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace renegade::studio
{
    struct ScreenEditorWorkspacePreferences
    {
        float left = 260.0f;
        float right = 360.0f;
        int spacing = 16;
        int tab = 2;
        bool grid = true;
        bool snap = true;
        bool leftCollapsed=false,rightCollapsed=false;

        [[nodiscard]] bool Valid() const noexcept
        {
            return std::isfinite(left) && std::isfinite(right) &&
                left >= ScreenEditorPanelLayout::MinLeft &&
                left <= 1200.0f &&
                right >= ScreenEditorPanelLayout::MinRight &&
                right <= 1200.0f &&
                spacing >= 4 && spacing <= 128 &&
                tab >= 0 && tab <= 3;
        }

        [[nodiscard]] static bool Load(
            const std::filesystem::path& path,
            ScreenEditorWorkspacePreferences& result)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec) ||
                std::filesystem::file_size(path, ec) > 1024)
                return false;
            std::ifstream file(path);
            if (!file) return false;
            std::string version;
            ScreenEditorWorkspacePreferences candidate;
            int gridFlag = -1, snapFlag = -1;
            if (!(file >> version >> candidate.left >> candidate.right >>
                  candidate.spacing >> candidate.tab >> gridFlag >> snapFlag) ||
                (version != "renegade-ui-workspace-v1" && version != "renegade-ui-workspace-v2") ||
                (gridFlag != 0 && gridFlag != 1) ||
                (snapFlag != 0 && snapFlag != 1))
                return false;
            if(version=="renegade-ui-workspace-v2"){
                int leftFlag,rightFlag;
                if(!(file>>leftFlag>>rightFlag)||(leftFlag!=0&&leftFlag!=1)||(rightFlag!=0&&rightFlag!=1))return false;
                candidate.leftCollapsed=leftFlag!=0;candidate.rightCollapsed=rightFlag!=0;
            }
            std::string extra;
            if (file >> extra) return false;
            candidate.grid = gridFlag != 0;
            candidate.snap = snapFlag != 0;
            if (!candidate.Valid()) return false;
            result = candidate;
            return true;
        }

        [[nodiscard]] bool Save(const std::filesystem::path& path) const
        {
            if (!Valid()) return false;
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec) return false;
            std::ostringstream output;
            output << "renegade-ui-workspace-v2\n"
                   << left << " " << right << " " << spacing << " " << tab
                   << " " << int(grid) << " " << int(snap)
                   << " " << int(leftCollapsed) << " " << int(rightCollapsed) << "\n";
            const auto bytes=output.str();
            bridge::ProjectDocumentWrite write;write.destinationPath=std::filesystem::absolute(path).generic_u8string();
            write.content.assign(bytes.begin(),bytes.end());
            write.validator=[](const std::string& candidate,std::string& error){
                ScreenEditorWorkspacePreferences checked;
                if(!Load(std::filesystem::u8path(candidate),checked)){error="Invalid staged workspace preferences.";return false;}return true;
            };
            bridge::ProjectDocumentTransactionOptions options;
            options.allowedRoot=std::filesystem::absolute(path.parent_path()).generic_u8string();
            options.journalDirectory=(std::filesystem::absolute(path.parent_path())/"WorkspaceTransactions").generic_u8string();
            return bridge::ProjectDocumentTransaction{}.Execute({std::move(write)},std::move(options)).success;
        }
    };
}
