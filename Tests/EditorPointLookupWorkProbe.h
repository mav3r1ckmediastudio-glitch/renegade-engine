#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/SceneService.h"
#include <memory>
#include <stdexcept>
#include <vector>

namespace renegade::tests
{
    struct LookupWorkCounters { size_t enumerationReads = 0, mutations = 0; };

    // Decorate every native manager, retaining the original manager objects so
    // Scene's typed references remain valid. Count work, not machine timing.
    class LookupWorkProbe final : public wi::ecs::ComponentManager_Interface
    {
        using Base = wi::ecs::ComponentManager_Interface;
        std::unique_ptr<Base> inner_;
        LookupWorkCounters& work_;
    public:
        LookupWorkProbe(std::unique_ptr<Base> inner, LookupWorkCounters& work)
            : inner_(std::move(inner)), work_(work) {}
        void Copy(const Base&) override { throw std::logic_error("Point lookup copied component storage"); }
        void Merge(Base&) override { throw std::logic_error("Point lookup merged component storage"); }
        void Clear() override { ++work_.mutations; inner_->Clear(); }
        void Serialize(wi::Archive& a, wi::ecs::EntitySerializer& s) override
            { ++work_.enumerationReads; inner_->Serialize(a,s); }
        void Component_Serialize(wi::ecs::Entity e, wi::Archive& a, wi::ecs::EntitySerializer& s) override
            { ++work_.mutations; inner_->Component_Serialize(e,a,s); }
        void Remove(wi::ecs::Entity e) override { ++work_.mutations; inner_->Remove(e); }
        void Remove_KeepSorted(wi::ecs::Entity e) override { ++work_.mutations; inner_->Remove_KeepSorted(e); }
        void MoveItem(size_t a, size_t b) override { ++work_.mutations; inner_->MoveItem(a,b); }
        bool Contains(wi::ecs::Entity e) const override { return inner_->Contains(e); }
        size_t GetIndex(wi::ecs::Entity e) const override { return inner_->GetIndex(e); }
        size_t GetCount() const override { return inner_->GetCount(); }
        wi::ecs::Entity GetEntity(size_t i) const override
            { ++work_.enumerationReads; return inner_->GetEntity(i); }
        const wi::vector<wi::ecs::Entity>& GetEntityArray() const override
            { ++work_.enumerationReads; return inner_->GetEntityArray(); }
    };

    inline bool EditorPointLookupAvoidsSceneEnumeration()
    {
        LookupWorkCounters work;
        bridge::SceneService probe;
        probe.NewScene();
        auto& scene = probe.GetScene();
        std::vector<wi::ecs::Entity> markers;
        for (size_t i=0; i<5500; ++i)
        {
            const auto e=wi::ecs::CreateEntity();
            scene.names.Create(e) = "Lookup regression entity";
            if (i<64) { scene.lights.Create(e); markers.push_back(e); }
            else if (i<80) { scene.metadatas.Create(e); markers.push_back(e); }
            else if (i<88) { scene.cameras.Create(e); markers.push_back(e); }
        }
        const auto absent=wi::ecs::CreateEntity();
        for (auto& entry : scene.componentLibrary.entries)
            entry.second.component_manager=std::make_unique<LookupWorkProbe>(
                std::move(entry.second.component_manager),work);

        // Positive control: the old lookup must trip this instrumentation.
        wi::unordered_set<wi::ecs::Entity> reference;
        scene.FindAllEntities(reference);
        if (!work.enumerationReads || reference.count(markers.front())!=1)
            return false;
        work = {};
        for (int frame=0; frame<20; ++frame)
        {
            for (auto e : markers)
                if (!probe.ContainsEntity(e) || !probe.IsHierarchyVisible(e))
                    return false;
            if (probe.ContainsEntity(absent) || probe.IsHierarchyVisible(absent) ||
                probe.ContainsEntity(wi::ecs::INVALID_ENTITY))
                return false;
        }
        return work.enumerationReads==0 && work.mutations==0;
    }
}
