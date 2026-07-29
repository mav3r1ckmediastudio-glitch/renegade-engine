#pragma once

#include "renegade/bridge/CommandService.h"
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

        void NewScene()
        {
            commands_.Clear();
            selection_.Clear();
            scenes_.NewScene();
        }

        bool LoadScene(const std::string& filePath)
        {
            commands_.Clear();
            selection_.Clear();
            return scenes_.LoadScene(filePath);
        }

    private:
        SceneService scenes_;
        SelectionService selection_;
        CommandService commands_;
    };
}
