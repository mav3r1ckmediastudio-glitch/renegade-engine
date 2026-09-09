#include "MarkerIconOverlay.h"
#include "StudioApplication.h"

#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/PlayerService.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>

namespace renegade::studio
{
    namespace
    {
        namespace fs = std::filesystem;

        enum class MarkerIconKind : std::size_t
        {
            PlayerStart,
            PointLight,
            SpotLight,
            DirectionalLight,
            RectangleLight,
            Camera,
            DecalProjector,
            AudioSource,
            ParticleEmitter,
            TriggerZone,
            AudioZone,
            NpcSpawn,
            Waypoint,
            Checkpoint,
            PickupSpawn,
            ScriptEntity,
            PhysicsVolume,
            Count,
        };

        constexpr std::array<const char*, static_cast<std::size_t>(MarkerIconKind::Count)> MarkerFiles = {
            "player_start.png",
            "light_point.png",
            "light_spot.png",
            "light_directional.png",
            "light_rectangle.png",
            "camera.png",
            "decal_projector.png",
            "audio_source.png",
            "particle_emitter.png",
            "trigger_zone.png",
            "audio_zone.png",
            "npc_spawn.png",
            "waypoint.png",
            "checkpoint.png",
            "pickup_spawn.png",
            "script_entity.png",
            "physics_volume.png",
        };

        constexpr float MarkerSize = 96.0f;
        constexpr float HoverExtraSize = 10.0f;
        constexpr float SelectedExtraSize = 16.0f;
        constexpr float HoverRadius = 54.0f;

        bool PointInside(const XMFLOAT2& point, const XMFLOAT4& bounds) noexcept
        {
            return point.x >= bounds.x && point.x <= bounds.z &&
                point.y >= bounds.y && point.y <= bounds.w;
        }

        bool ProjectPoint(
            const XMFLOAT3& world,
            const wi::scene::CameraComponent& camera,
            const wi::Canvas& canvas,
            const XMFLOAT4& viewport,
            XMFLOAT2& screen,
            float* depth = nullptr) noexcept
        {
            const XMVECTOR clip = XMVector4Transform(
                XMVectorSet(world.x, world.y, world.z, 1.0f),
                camera.GetViewProjection());
            const float w = XMVectorGetW(clip);
            if (w <= 0.001f)
                return false;

            const XMVECTOR ndc = clip / w;
            const float z = XMVectorGetZ(ndc);
            if (z < 0.0f || z > 1.0f)
                return false;

            screen.x = (XMVectorGetX(ndc) * 0.5f + 0.5f) * canvas.GetLogicalWidth();
            screen.y = (-XMVectorGetY(ndc) * 0.5f + 0.5f) * canvas.GetLogicalHeight();
            if (depth != nullptr)
                *depth = z;
            return PointInside(screen, viewport);
        }

        MarkerIconKind KindForLight(const wi::scene::LightComponent& light) noexcept
        {
            switch (light.type)
            {
            case wi::scene::LightComponent::SPOT:
                return MarkerIconKind::SpotLight;
            case wi::scene::LightComponent::DIRECTIONAL:
                return MarkerIconKind::DirectionalLight;
            case wi::scene::LightComponent::RECTANGLE:
                return MarkerIconKind::RectangleLight;
            case wi::scene::LightComponent::POINT:
            default:
                return MarkerIconKind::PointLight;
            }
        }

        class MarkerIconOverlay final : public wi::gui::Widget
        {
        public:
            MarkerIconOverlay()
            {
                SetName("Renegade Marker Icon Overlay");
                // Markers are creator handles, not decorative sprites. Keeping
                // the overlay enabled lets the GUI focus contract consume a
                // marker click before the normal scene picker can select the
                // object behind an editor-only marker.
                SetEnabled(true);
                SetVisible(true);
                SetShadowRadius(0.0f);
            }

            void Bind(StudioRenderPath& owner)
            {
                owner_ = &owner;
                RefreshResources(true);
            }

            void RefreshResources(const bool force = false)
            {
                auto* session = bridge::StudioSession::Current();
                std::string descriptor;
                if (session != nullptr && session->Projects().HasProject())
                    descriptor = session->Projects().CurrentProject().descriptorPath;

                if (!force && descriptor == projectDescriptor_)
                    return;
                projectDescriptor_ = descriptor;

                fs::path projectRoot;
                if (!descriptor.empty())
                    projectRoot = fs::path(descriptor).parent_path();

                for (std::size_t index = 0; index < MarkerFiles.size(); ++index)
                {
                    const fs::path fileName = MarkerFiles[index];
                    fs::path resolved;

                    if (!projectRoot.empty())
                    {
                        const fs::path candidate =
                            projectRoot / "Content" / "Editor" / "MarkerIcons" / fileName;
                        std::error_code error;
                        if (fs::exists(candidate, error) && fs::is_regular_file(candidate, error))
                            resolved = candidate;
                    }

                    if (resolved.empty())
                    {
                        const fs::path candidate =
                            fs::path("User") / "MarkerIcons" / fileName;
                        std::error_code error;
                        if (fs::exists(candidate, error) && fs::is_regular_file(candidate, error))
                            resolved = candidate;
                    }

                    if (resolved.empty())
                        resolved = fs::path("Content") / "Editor" / "MarkerIcons" / fileName;

                    const std::string next = resolved.generic_u8string();
                    if (force || next != resolvedPaths_[index])
                    {
                        resolvedPaths_[index] = next;
                        resources_[index] = wi::resourcemanager::Load(next);
                    }
                }
            }

            void Update(const wi::Canvas& canvas, const float dt) override
            {
                if (!CanUseMarkers())
                {
                    hoveredEntity_ = wi::ecs::INVALID_ENTITY;
                    if (state != wi::gui::IDLE)
                        Deactivate();
                    return;
                }

                Widget::Update(canvas, dt);
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;

                const auto& camera = wi::scene::GetCamera();
                const XMFLOAT4 viewport = owner_->StoryFlowWorkspaceBounds();
                if (viewport.z <= viewport.x || viewport.w <= viewport.y)
                    return;

                const XMFLOAT4 pointer = wi::input::GetPointer();
                const XMFLOAT2 pointer2(pointer.x, pointer.y);
                wi::ecs::Entity candidate = wi::ecs::INVALID_ENTITY;
                float bestDistance2 = HoverRadius * HoverRadius;
                float bestDepth = 2.0f;

                ForEachMarker(session->Scenes().GetScene(),
                    [&](const MarkerIconKind, const wi::ecs::Entity entity,
                        const XMFLOAT3& position)
                    {
                        XMFLOAT2 center = {};
                        float depth = 1.0f;
                        if (!ProjectPoint(position, camera, canvas, viewport, center, &depth))
                            return;
                        const float dx = pointer2.x - center.x;
                        const float dy = pointer2.y - center.y;
                        const float distance2 = dx * dx + dy * dy;
                        if (distance2 > HoverRadius * HoverRadius)
                            return;

                        // Prefer the marker whose centre the creator is
                        // actually closest to. If markers are effectively on
                        // top of each other, prefer the front-most marker.
                        constexpr float DistanceTie = 0.25f;
                        if (candidate == wi::ecs::INVALID_ENTITY ||
                            distance2 < bestDistance2 - DistanceTie ||
                            (std::abs(distance2 - bestDistance2) <= DistanceTie &&
                                depth < bestDepth))
                        {
                            candidate = entity;
                            bestDistance2 = distance2;
                            bestDepth = depth;
                        }
                    });

                hoveredEntity_ = candidate;
                if (candidate == wi::ecs::INVALID_ENTITY)
                {
                    if (state != wi::gui::ACTIVE)
                        state = wi::gui::IDLE;
                    if (state == wi::gui::ACTIVE &&
                        !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
                        Deactivate();
                    return;
                }

                state = wi::gui::FOCUS;
                if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
                {
                    session->Selection().Select(candidate);
                    Activate();
                }
                else if (state == wi::gui::ACTIVE &&
                    !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
                {
                    Deactivate();
                }
            }

            void Render(
                const wi::Canvas& canvas,
                const wi::graphics::CommandList cmd) const override
            {
                if (!CanUseMarkers())
                    return;

                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;

                const auto& camera = wi::scene::GetCamera();
                const XMFLOAT4 viewport = owner_->StoryFlowWorkspaceBounds();
                if (viewport.z <= viewport.x || viewport.w <= viewport.y)
                    return;

                auto& scene = session->Scenes().GetScene();
                const wi::ecs::Entity selected = session->Selection().SelectedEntity();
                const XMFLOAT4 pointer = wi::input::GetPointer();

                ForEachMarker(scene,
                    [&](const MarkerIconKind kind, const wi::ecs::Entity entity,
                        const XMFLOAT3& position)
                    {
                        XMFLOAT2 center = {};
                        if (!ProjectPoint(position, camera, canvas, viewport, center))
                            return;

                        const float dx = pointer.x - center.x;
                        const float dy = pointer.y - center.y;
                        const bool hovered = entity == hoveredEntity_ ||
                            dx * dx + dy * dy <= HoverRadius * HoverRadius;
                        const bool isSelected = entity == selected;
                        const float size = MarkerSize +
                            (isSelected ? SelectedExtraSize : hovered ? HoverExtraSize : 0.0f);
                        const std::size_t index = static_cast<std::size_t>(kind);
                        if (index >= resources_.size() || !resources_[index].IsValid())
                            return;

                        wi::image::Params params(
                            center.x - size * 0.5f,
                            center.y - size * 0.5f,
                            size,
                            size,
                            wi::Color::White());
                        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
                        params.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
                        params.opacity = isSelected ? 1.0f : hovered ? 0.98f : 0.90f;
                        wi::image::Draw(&resources_[index].GetTexture(), params, cmd);
                    });
            }

        private:
            [[nodiscard]] bool CanUseMarkers() const noexcept
            {
                return IsVisible() && owner_ != nullptr &&
                    !owner_->IsProjectHubVisible() &&
                    !owner_->IsTestLevelRuntimeActive();
            }

            void ForEachMarker(
                wi::scene::Scene& scene,
                const std::function<void(
                    MarkerIconKind, wi::ecs::Entity, const XMFLOAT3&)>& visit) const
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;

                const auto emit = [&](const MarkerIconKind kind,
                    const wi::ecs::Entity entity, const XMFLOAT3& position)
                {
                    if (entity != wi::ecs::INVALID_ENTITY &&
                        session->Scenes().IsHierarchyVisible(entity))
                    {
                        visit(kind, entity, position);
                    }
                };

                const auto player = bridge::ResolvePlayerStart(scene);
                if (player.resolution == bridge::PlayerStartResolution::Success)
                {
                    emit(
                        MarkerIconKind::PlayerStart,
                        player.start.entity,
                        player.start.transform.translation);
                }

                for (std::size_t index = 0; index < scene.lights.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.lights.GetEntity(index);
                    const auto* light = scene.lights.GetComponent(entity);
                    const auto* transform = scene.transforms.GetComponent(entity);
                    if (light != nullptr && transform != nullptr)
                        emit(KindForLight(*light), entity, transform->GetPosition());
                }

                for (std::size_t index = 0; index < scene.cameras.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.cameras.GetEntity(index);
                    const auto* transform = scene.transforms.GetComponent(entity);
                    if (transform != nullptr)
                        emit(MarkerIconKind::Camera, entity, transform->GetPosition());
                }

                for (std::size_t index = 0; index < scene.decals.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.decals.GetEntity(index);
                    const auto* transform = scene.transforms.GetComponent(entity);
                    if (transform != nullptr)
                        emit(MarkerIconKind::DecalProjector, entity, transform->GetPosition());
                }

                for (std::size_t index = 0; index < scene.sounds.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.sounds.GetEntity(index);
                    const auto* transform = scene.transforms.GetComponent(entity);
                    if (transform != nullptr && bridge::IsRenegadeSoundSource(scene, entity))
                        emit(MarkerIconKind::AudioSource, entity, transform->GetPosition());
                }

                for (std::size_t index = 0; index < scene.emitters.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.emitters.GetEntity(index);
                    const auto* transform = scene.transforms.GetComponent(entity);
                    if (transform != nullptr)
                        emit(MarkerIconKind::ParticleEmitter, entity, transform->GetPosition());
                }

                for (std::size_t index = 0; index < scene.scripts.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.scripts.GetEntity(index);
                    const auto* transform = scene.transforms.GetComponent(entity);
                    if (transform != nullptr &&
                        !scene.objects.Contains(entity) &&
                        !scene.humanoids.Contains(entity))
                    {
                        emit(MarkerIconKind::ScriptEntity, entity, transform->GetPosition());
                    }
                }
            }

            StudioRenderPath* owner_ = nullptr;
            std::string projectDescriptor_;
            wi::ecs::Entity hoveredEntity_ = wi::ecs::INVALID_ENTITY;
            std::array<wi::Resource, static_cast<std::size_t>(MarkerIconKind::Count)> resources_;
            std::array<std::string, static_cast<std::size_t>(MarkerIconKind::Count)> resolvedPaths_;
        };
    }

    void EnsureMarkerIconOverlay(StudioRenderPath& owner)
    {
        static MarkerIconOverlay overlay;
        static StudioRenderPath* registeredOwner = nullptr;
        if (registeredOwner != &owner)
        {
            overlay.Bind(owner);
            owner.RegisterStoryFlowLifecycleControl(overlay);
            registeredOwner = &owner;
        }
        else
        {
            overlay.RefreshResources();
        }
    }
}
