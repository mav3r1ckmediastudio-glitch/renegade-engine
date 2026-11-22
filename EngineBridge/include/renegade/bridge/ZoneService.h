#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include <array>
#include <cmath>

namespace renegade::bridge::zones
{
    using Entity = wi::ecs::Entity;
    using Scene = wi::scene::Scene;
    enum class Behaviour { Trigger, Win };

    // Bounds are in authored local metres. The placement origin is on the floor.
    struct Definition
    {
        std::array<float, 3> low{-1, 0, -1};
        std::array<float, 3> high{1, 2, 1};
        Behaviour behaviour = Behaviour::Trigger;
        std::string enter = "enter", exit = "exit", target;
    };

    inline constexpr const char* Version = "renegade.zone.version";
    inline constexpr const char* Low[] = {"zone.min.x", "zone.min.y", "zone.min.z"};
    inline constexpr const char* High[] = {"zone.max.x", "zone.max.y", "zone.max.z"};

    inline bool IsZone(const Scene& scene, Entity id)
    {
        const auto* m = scene.metadatas.GetComponent(id);
        return m && m->int_values.has(Version) && m->int_values.get(Version) == 1;
    }

    inline bool Valid(const Definition& zone)
    {
        for (size_t i = 0; i < 3; ++i)
            if (!std::isfinite(zone.low[i]) || !std::isfinite(zone.high[i]) ||
                zone.high[i] <= zone.low[i]) return false;
        return true;
    }

    inline bool Read(const Scene& scene, Entity id, Definition& zone)
    {
        if (!IsZone(scene, id)) return false;
        const auto& m = *scene.metadatas.GetComponent(id);
        for (size_t i = 0; i < 3; ++i)
        {
            zone.low[i] = m.float_values.get(Low[i]);
            zone.high[i] = m.float_values.get(High[i]);
        }
        zone.behaviour = m.int_values.get("zone.behaviour") == 1 ? Behaviour::Win : Behaviour::Trigger;
        zone.enter = m.string_values.get("zone.enter");
        zone.exit = m.string_values.get("zone.exit");
        zone.target = m.string_values.get("zone.target");
        return Valid(zone);
    }

    inline void Write(Scene& scene, Entity id, const Definition& zone)
    {
        auto& m = *scene.metadatas.GetComponent(id);
        m.int_values.set(Version, 1);
        m.int_values.set("zone.behaviour", int(zone.behaviour));
        for (size_t i = 0; i < 3; ++i)
        {
            m.float_values.set(Low[i], zone.low[i]);
            m.float_values.set(High[i], zone.high[i]);
        }
        m.string_values.set("zone.enter", zone.enter);
        m.string_values.set("zone.exit", zone.exit);
        m.string_values.set("zone.target", zone.target);
    }

    inline bool Contains(const Definition& zone, DirectX::FXMMATRIX world, const XMFLOAT3& point)
    {
        using namespace DirectX;
        XMVECTOR determinant;
        const auto inverse = XMMatrixInverse(&determinant, world);
        if (XMVectorGetX(determinant) == 0) return false;
        XMFLOAT3 p;
        XMStoreFloat3(&p, XMVector3TransformCoord(XMLoadFloat3(&point), inverse));
        return p.x >= zone.low[0] && p.x <= zone.high[0] && p.y >= zone.low[1] &&
               p.y <= zone.high[1] && p.z >= zone.low[2] && p.z <= zone.high[2];
    }

    class Create final : public ICommand
    {
    public:
        Create(Scene& scene, XMFLOAT3 position) : scene_(scene), position_(position) {}
        Entity Id() const { return id_; }
        bool Execute() override
        {
            if (scene_.transforms.Contains(id_) || scene_.names.Contains(id_)) return false;
            scene_.names.Create(id_).name = "Gameplay Zone";
            auto& transform = scene_.transforms.Create(id_);
            transform.Translate(position_);
            transform.UpdateTransform();
            std::string error;
            if (!AssignPersistentEntityId(scene_, id_, stableId_, error))
            {
                scene_.Entity_Remove(id_);
                return false;
            }
            Write(scene_, id_, {});
            return true;
        }
        void Undo() override { scene_.Entity_Remove(id_); }
    private:
        Scene& scene_;
        XMFLOAT3 position_;
        Entity id_ = wi::ecs::CreateEntity();
        StableId stableId_ = GenerateStableId();
    };

    class Edit final : public ICommand
    {
    public:
        Edit(Scene& scene, Entity entity, Definition next)
            : scene_(scene), id_(entity), next_(std::move(next)) {}
        bool Execute() override
        {
            if (!Valid(next_) || !Read(scene_, id_, before_)) return false;
            Write(scene_, id_, next_);
            return true;
        }
        void Undo() override { if (IsZone(scene_, id_)) Write(scene_, id_, before_); }
    private:
        Scene& scene_;
        Entity id_;
        Definition next_, before_;
    };
}
