#pragma once
#include "renegade/bridge/CreatorModelImportRecipe.h"

namespace renegade::bridge
{
    inline bool StartImportedObjectClip(wi::scene::Scene& scene, size_t index)
    {
        const auto* metadata = scene.metadatas.GetComponent(scene.animations.GetEntity(index));
        if (!metadata || !metadata->int_values.has(ModelClipStartKey)) return false;
        auto& clip = scene.animations[index];
        const auto mode = static_cast<ModelClipStart>(metadata->int_values.get(ModelClipStartKey));
        clip.Pause();
        clip.timer = clip.start;
        clip.last_update_time = clip.timer;
        if (mode == ModelClipStart::Loop) clip.SetLooped(true);
        else if (mode == ModelClipStart::Once) clip.SetPlayOnce();
        else return true;
        clip.last_update_time = -1;
        clip.Play();
        return true;
    }

    inline void StartImportedObjectClips(wi::scene::Scene& scene)
    {
        for (size_t i = 0; i < scene.animations.GetCount(); ++i)
            StartImportedObjectClip(scene, i);
    }
}
