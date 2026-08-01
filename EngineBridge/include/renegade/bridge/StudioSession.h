#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"

namespace renegade::bridge
{
    class StudioSession
    {
    public:
        [[nodiscard]] SceneService& Scenes() noexcept
        {
            return scenes_;
        }

        [[nodiscard]] const SceneService& Scenes() const noexcept
        {
            return scenes_;
        }

        [[nodiscard]] SelectionService& Selection() noexcept
        {
            return selection_;
        }

        [[nodiscard]] CommandService& Commands() noexcept
        {
            return commands_;
        }

        [[nodiscard]] ProjectService& Projects() noexcept
        {
            return projects_;
        }

        [[nodiscard]] const ProjectService& Projects() const noexcept
        {
            return projects_;
        }

        void NewScene()
        {
            commands_.Clear();
            selection_.Clear();
            scenes_.NewScene();
        }

        bool LoadScene(const std::string& filePath)
        {
            if (!scenes_.LoadScene(filePath))
            {
                return false;
            }
            commands_.Clear();
            selection_.Clear();
            return true;
        }

        bool SaveScene(const std::string& filePath)
        {
            if (!scenes_.SaveScene(filePath))
            {
                return false;
            }
            commands_.MarkSaved();
            return true;
        }

        bool ReloadScene()
        {
            if (!scenes_.ReloadScene())
            {
                return false;
            }
            commands_.Clear();
            selection_.Clear();
            return true;
        }

    private:
        ProjectService projects_;
        SceneService scenes_;
        SelectionService selection_;
        CommandService commands_;
    };
}
