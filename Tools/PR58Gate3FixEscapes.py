from pathlib import Path
import re

path = Path('Studio/src/StudioApplication.cpp')
source = path.read_text(encoding='utf-8')

refresh_hub = r'''    void StudioRenderPath::RefreshProjectHub()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto& projects = session_->Projects().RecentProjects();
        if (projects.empty())
        {
            selectedRecentProject_ = -1;
        }
        else if (selectedRecentProject_ < 0 ||
                 static_cast<std::size_t>(selectedRecentProject_) >= projects.size())
        {
            // Presentation-only default selection: no project is opened here.
            selectedRecentProject_ = 0;
        }

        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            const bool hasProject = index < projects.size();
            button.SetVisible(hasProject);
            if (!hasProject)
            {
                continue;
            }

            const std::string ordinal =
                (index + 1u < 10u ? "0" : "") + std::to_string(index + 1u);
            button.SetText(ordinal + "  //  " + projects[index].name);
            button.SetTooltip(projects[index].descriptorPath);
            button.SetColor(
                static_cast<int>(index) == selectedRecentProject_
                    ? HubSelected
                    : HubSurface,
                wi::gui::IDLE);
        }

        if (selectedRecentProject_ < 0)
        {
            selectedProjectLabel_.SetText(
                "PROJECT DETAILS\n\n"
                "NO RECENT PROJECTS\n\n"
                "Create a new project or open an existing .renegade project to begin.");
        }
        else
        {
            const auto& selected =
                projects[static_cast<std::size_t>(selectedRecentProject_)];
            selectedProjectLabel_.SetText(
                "PROJECT DETAILS\n\n" + selected.name +
                "\n\nPROJECT DESCRIPTOR\n" + selected.descriptorPath +
                "\n\nFORMAT // RENEGADE PROJECT V1");
        }

        launchProjectButton_.SetEnabled(selectedRecentProject_ >= 0);
        continueProjectButton_.SetVisible(session_->Projects().HasProject());
        if (session_->Projects().HasProject())
        {
            continueProjectButton_.SetText(
                "BACK TO EDITOR // " + session_->Projects().CurrentProject().name);
        }
    }'''

pattern = (
    r'    void StudioRenderPath::RefreshProjectHub\(\)\n    \{.*?\n    \}\n\n'
    r'    void StudioRenderPath::ApplySelectedTransformValue')
match = re.search(pattern, source, flags=re.S)
if match is None:
    raise SystemExit('RefreshProjectHub: exact function range not found')

source = source[:match.start()] + refresh_hub + '\n\n    void StudioRenderPath::ApplySelectedTransformValue' + source[match.end():]
path.write_text(source, encoding='utf-8')
