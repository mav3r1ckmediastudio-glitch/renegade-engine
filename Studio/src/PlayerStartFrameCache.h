#pragma once
#include "renegade/bridge/PlayerService.h"
#include <cstdint>

namespace renegade::studio
{
    // A UI-frame snapshot, never persisted and never shared between scenes.
    class PlayerStartFrameCache
    {
    public:
        bridge::PlayerStartResult Resolve(const wi::scene::Scene& scene,
            std::uint64_t frame,std::uint64_t sceneRevision,std::uint64_t commandRevision)
        {
            if(scene_!=&scene || frame_!=frame || sceneRevision_!=sceneRevision ||
                commandRevision_!=commandRevision || metadataCount_!=scene.metadatas.GetCount())
            {
                scene_=&scene;frame_=frame;sceneRevision_=sceneRevision;
                commandRevision_=commandRevision;metadataCount_=scene.metadatas.GetCount();
                value_=bridge::ResolvePlayerStart(scene);
            }
            auto value=value_;
            if(value.resolution==bridge::PlayerStartResolution::Success)
            {
                if(!bridge::IsPlayerStart(scene,value.start.entity))
                    return bridge::ResolvePlayerStart(scene);
                if(const auto* transform=scene.transforms.GetComponent(value.start.entity))
                    value.start.transform=bridge::CaptureTransform(*transform);
                else return bridge::ResolvePlayerStart(scene);
            }
            return value;
        }
    private:
        const wi::scene::Scene* scene_=nullptr;
        std::uint64_t frame_=~std::uint64_t(0),sceneRevision_=0,commandRevision_=0;
        std::size_t metadataCount_=0;
        bridge::PlayerStartResult value_;
    };
}
