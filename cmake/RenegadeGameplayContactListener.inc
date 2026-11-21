#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <unordered_map>
#include <chrono>
#include "../EngineBridge/include/renegade/bridge/PhysicsDiagnosticsService.h"

namespace wi::physics
{
    struct DestructibleImpact
    {
        wi::ecs::Entity target;
        wi::ecs::Entity impactor;
        float speed;
        JPH::Vec3 direction;
    };

    struct DestructibleRattle
    {
        struct Piece
        {
            wi::ecs::Entity entity;
            XMFLOAT3 rest;
            XMFLOAT3 displayed;
            XMFLOAT3 offset;
        };
        std::vector<Piece> pieces;
        float elapsed = 0.f;
    };

    struct DestructibleDebris
    {
        wi::ecs::Entity entity;
        float stayRemaining;
        float fadeSeconds;
        float baseOpacity;
    };

    class GameplayContactListener final : public JPH::ContactListener
    {
    public:
        void Enable(bool value);
        renegade::bridge::ContactEventBatch Drain();
        void SetDestructible(wi::ecs::Entity entity, float threshold, float hitPoints = 0.f);
        bool DamageDestructible(wi::ecs::Entity entity, wi::ecs::Entity impactor, float damage,
            JPH::Vec3Arg direction);
        void ResetDestructibles();
        bool NeedsListener() const;
        bool RequestBreak(wi::ecs::Entity entity);
        bool PopImpact(DestructibleImpact& impact, bool rattle = false);
        std::vector<renegade::bridge::DestructibleBreakEvent> DrainBreaks();
        void ReportBreak(const renegade::bridge::DestructibleBreakEvent& event);
        std::atomic<bool> sample_diagnostics{false};
        std::atomic<uint32_t> diagnostic_contacts{0};

        void OnContactAdded(const JPH::Body& a, const JPH::Body& b,
            const JPH::ContactManifold& manifold, JPH::ContactSettings&) override;
        void OnContactPersisted(const JPH::Body& a, const JPH::Body& b,
            const JPH::ContactManifold& manifold, JPH::ContactSettings&) override;
        void OnContactRemoved(const JPH::SubShapeIDPair& key) override;

    private:
        void Remember(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold);
        void Queue(renegade::bridge::ContactEvent event);
        void Drop();
        void RecordImpact(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold);

        std::atomic<bool> enabled_ = false;
        std::atomic<bool> has_destructibles_ = false;
        std::mutex mutex_;
        std::unordered_map<JPH::SubShapeIDPair, renegade::bridge::ContactEvent> active_;
        renegade::bridge::ContactEventBatch batch_;
        struct DestructibleTarget
        {
            float impactSpeed = 0.f;
            float hitPoints = 0.f;
            float remaining = 0.f;
        };
        std::unordered_map<wi::ecs::Entity, DestructibleTarget> destructibles_;
        std::unordered_map<wi::ecs::Entity, DestructibleImpact> impacts_;
        std::unordered_map<wi::ecs::Entity, DestructibleImpact> rattles_;
        std::vector<renegade::bridge::DestructibleBreakEvent> breaks_;
    };
}
