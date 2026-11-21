#pragma once

#include "renegade/bridge/ProjectileEffectRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace renegade::runtime
{
    struct VtolThrusterEffects
    {
        void Bind(wi::scene::Scene& scene, wi::ecs::Entity chassis)
        {
            nozzles[0].marker = scene.Entity_FindByName("REF_Nacelle_Exhaust_L", chassis);
            nozzles[1].marker = scene.Entity_FindByName("REF_Nacelle_Exhaust_R", chassis);
        }

        void Stop(wi::scene::Scene& scene)
        {
            strength = 0.f;
            for (auto& nozzle : nozzles)
            {
                nozzle.marker = wi::ecs::INVALID_ENTITY;
                StopEmitter(scene, nozzle.core);
                StopEmitter(scene, nozzle.halo);
            }
        }

        void Update(wi::scene::Scene& scene, XMFLOAT3 velocity, float target, float dt)
        {
            strength += (target - strength) * (1.f - std::exp(-10.f * std::clamp(dt, 0.f, .1f)));
            if (strength < .02f)
                strength = 0.f;
            for (auto& nozzle : nozzles)
            {
                const auto* marker = scene.transforms.GetComponent(nozzle.marker);
                if (!marker || strength == 0.f)
                {
                    StopEmitter(scene, nozzle.core);
                    StopEmitter(scene, nozzle.halo);
                    continue;
                }

                // Wicked reflects glTF Z. Marker-local -Z points out of the nozzle.
                const auto world = XMLoadFloat4x4(&marker->world);
                const auto exhaust = XMVector3Normalize(-world.r[2]);
                XMFLOAT3 position;
                XMStoreFloat3(&position, world.r[3] + exhaust * .10f);
                const auto carrier = XMLoadFloat3(&velocity);
                UpdateEmitter(scene, nozzle.core, position, carrier + exhaust * (5.f + 6.f * strength), true);
                UpdateEmitter(scene, nozzle.halo, position, carrier + exhaust * (4.f + 6.f * strength), false);
            }
        }

    private:
        struct Nozzle
        {
            wi::ecs::Entity marker = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity core = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity halo = wi::ecs::INVALID_ENTITY;
        };
        std::array<Nozzle, 2> nozzles{};
        float strength = 0.f;

        static void StopEmitter(wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            if (auto* emitter = scene.emitters.GetComponent(entity))
                emitter->count = 0.f;
        }

        void UpdateEmitter(wi::scene::Scene& scene, wi::ecs::Entity& entity,
            XMFLOAT3 position, XMVECTOR velocity, bool core)
        {
            if (!scene.emitters.Contains(entity))
            {
                bridge::ProjectileEffectLayer layer;
                layer.kind = bridge::ProjectileEffectKind::Flame;
                layer.sizeMetres = core ? .42f : .82f;
                layer.particleLifeSeconds = core ? .13f : .23f;
                entity = bridge::CreateProjectileEffectEmitter(scene, layer, position);
                auto& emitter = *scene.emitters.GetComponent(entity);
                emitter.SetMaxParticleCount(64);
                emitter.random_factor = .13f;
                emitter.random_life = .055f;
                emitter.random_color = 0.f;
                emitter.rotation = 0.f;
                emitter.scaleX = core ? .55f : 1.25f;
                emitter.scaleY = emitter.scaleX;
                emitter.SetOpacityCurveControl(.02f, core ? .43f : .23f);
                auto& material = *scene.materials.GetComponent(entity);
                material.SetBaseColor(core ? XMFLOAT4(.34f, .88f, 1.f, .84f)
                                           : XMFLOAT4(.075f, .38f, 1.f, .35f));
                material.SetEmissiveColor(core ? XMFLOAT4(.30f, .84f, 1.f, 2.f)
                                               : XMFLOAT4(.07f, .28f, 1.f, .75f));
                scene.names.GetComponent(entity)->name = core ? "VTOL exhaust core" : "VTOL exhaust halo";
            }
            auto& transform = *scene.transforms.GetComponent(entity);
            transform.translation_local = position;
            transform.SetDirty();
            auto& emitter = *scene.emitters.GetComponent(entity);
            emitter.count = (core ? 118.f : 92.f) * strength;
            XMStoreFloat3(&emitter.velocity, velocity);
        }
    };
}
