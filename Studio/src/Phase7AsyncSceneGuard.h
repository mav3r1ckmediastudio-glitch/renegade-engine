#pragma once

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/StudioSession.h"

#include <cstdint>

namespace renegade::studio
{
    struct Phase7AsyncSceneGuard
    {
        bridge::StudioSession* session = nullptr;
        bool hasProject = false;
        bridge::StableId projectId;
        std::uint64_t sceneRevision = 0;
    };

    [[nodiscard]] inline Phase7AsyncSceneGuard CapturePhase7AsyncSceneGuard()
    {
        Phase7AsyncSceneGuard guard;
        guard.session = bridge::StudioSession::Current();
        if (guard.session == nullptr)
            return guard;

        guard.hasProject = guard.session->Projects().HasProject();
        if (guard.hasProject)
            guard.projectId = guard.session->Projects().CurrentProject().projectId;
        guard.sceneRevision = guard.session->Scenes().Revision();
        return guard;
    }

    [[nodiscard]] inline bool MatchesPhase7AsyncSceneGuard(
        const Phase7AsyncSceneGuard& guard)
    {
        auto* current = bridge::StudioSession::Current();
        if (guard.session == nullptr || current != guard.session)
            return false;
        if (current->Projects().HasProject() != guard.hasProject)
            return false;
        if (guard.hasProject &&
            current->Projects().CurrentProject().projectId != guard.projectId)
        {
            return false;
        }
        return current->Scenes().Revision() == guard.sceneRevision;
    }
}
