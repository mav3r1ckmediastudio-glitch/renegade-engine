#pragma once
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ScreenService.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace renegade::studio
{
    class ScreenProjectPalette
    {
        std::string root_;
        std::filesystem::path path_;
    public:
        std::vector<bridge::ScreenColor> colours;
        std::string status;

        bool Load(const std::string& root)
        {
            root_ = root;
            path_ = std::filesystem::u8path(root) / "Content/UI/Palette.rcolours";
            colours.clear();
            status.clear();
            if (root.empty()) { status = "Open a project to save colours"; return false; }
            std::error_code ec;
            if (!std::filesystem::exists(path_, ec) && !ec) return true;
            std::ifstream input(path_);
            std::string header;
            if (std::getline(input, header) && header == "renegade-ui-colours 1")
            {
                int r, g, b, a;
                while (input >> std::ws && !input.eof())
                {
                    if (!(input >> r >> g >> b >> a) || std::min({r,g,b,a}) < 0 ||
                        std::max({r,g,b,a}) > 255) break;
                    colours.push_back({uint8_t(r),uint8_t(g),uint8_t(b),uint8_t(a)});
                }
                if (input.eof() && !input.fail()) return true;
            }
            colours.clear();
            status = "Could not read project palette";
            return false;
        }

        void Save(bridge::ScreenColor colour)
        {
            if (!Load(root_)) return;
            for (const auto& c : colours)
                if (c.red == colour.red && c.green == colour.green &&
                    c.blue == colour.blue && c.alpha == colour.alpha)
                { status = "Already in palette"; return; }
            colours.push_back(colour);
            std::ostringstream out;
            out << "renegade-ui-colours 1\n";
            for (const auto& c : colours)
                out << int(c.red) << ' ' << int(c.green) << ' ' << int(c.blue) << ' ' << int(c.alpha) << '\n';
            auto data = out.str();
            bridge::ProjectDocumentWrite write;
            write.destinationPath = path_.generic_u8string();
            write.content.assign(data.begin(),data.end());
            bridge::ProjectDocumentTransactionOptions options;
            options.allowedRoot = root_;
            options.journalDirectory = (std::filesystem::u8path(root_) / "Saved/Transactions").generic_u8string();
            auto result = bridge::ProjectDocumentTransaction().Execute({std::move(write)},options);
            if (!result.success) colours.pop_back();
            status = result.success ? "Colour saved" : "Could not save palette";
        }
    };
}
