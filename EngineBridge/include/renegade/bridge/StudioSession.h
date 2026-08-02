#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SceneDocumentService.h"
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

        [[nodiscard]] SceneDocumentService& Documents() noexcept
        {
            return documents_;
        }

        [[nodiscard]] const SceneDocumentService& Documents() const noexcept
        {
            return documents_;
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
            documents_.NewScene();
        }

        bool LoadScene(const std::string& filePath)
        {
            return documents_.Open(filePath);
        }

        bool SaveScene(const std::string& filePath)
        {
            return documents_.Save(filePath);
        }

        bool ReloadScene()
        {
            return documents_.Reload();
        }

    private:
        ProjectService projects_;
        SceneService scenes_;
        SelectionService selection_;
        CommandService commands_;
        SceneDocumentService documents_{
            scenes_, selection_, commands_, projects_};
    };
}
