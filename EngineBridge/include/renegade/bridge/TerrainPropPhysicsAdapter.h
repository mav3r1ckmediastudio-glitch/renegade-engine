#pragma once
#include <wiScene.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Implemented only in the composed, Renegade-owned physics translation unit.
// No Jolt IDs or pointers escape into documents, Runtime or UI.
namespace wi::physics::terrain_props
{
    enum class ShapeKind { Capsule, Box };
    struct CastHit { bool hit = false; float fraction = 1; XMFLOAT3 normal{0,0,0}; };
    class Shape
    {
    public:
        bool IsValid() const noexcept { return state_ != nullptr; }
    private:
        std::shared_ptr<const void> state_;
        friend bool CreateShape(ShapeKind, const XMFLOAT3&, Shape&, std::string&);
        friend bool CastSegment(const Shape&, const XMFLOAT3&, const XMFLOAT3&, float,
            CastHit&, std::string&);
        friend bool PrepareStaticBody(wi::scene::Scene&, wi::ecs::Entity,
            const Shape&, float, std::uint32_t, std::string&);
    };
    struct Capacity
    {
        std::uint32_t allocated = 0;
        std::uint32_t maximum = 0;
    };
    // Physics initialization must have completed; shape coordinates are local.
    bool CreateShape(ShapeKind, const XMFLOAT3& halfExtents, Shape&, std::string&);
    // Local-space segment and optional swept-sphere radius. Return false only
    // for invalid/unavailable input, never to disguise an unknown query as clear.
    // Misses and endpoint hits are distinguished by CastHit::hit.
    bool CastSegment(const Shape&, const XMFLOAT3& from, const XMFLOAT3& to,
        float radius, CastHit& hit, std::string& error);
    // Caller owns the scene/physics-safe point. No second scene update or step.
    Capacity BodyCapacity(const wi::scene::Scene&) noexcept;
    bool HasLiveBody(const wi::scene::Scene&,wi::ecs::Entity) noexcept;
    bool IsBodySleeping(const wi::scene::Scene&,wi::ecs::Entity) noexcept;
    wi::ecs::Entity CharacterSupportEntity(const wi::scene::Scene&,wi::ecs::Entity) noexcept;
    bool BodyBounds(const wi::scene::Scene&,wi::ecs::Entity,XMFLOAT3& minimum,XMFLOAT3& maximum) noexcept;
    bool PrepareStaticBody(wi::scene::Scene&, wi::ecs::Entity,
        const Shape&, float uniformScale, std::uint32_t reserve, std::string&);
    struct NativeHit
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        float fraction = 1;
        XMFLOAT3 position{0,0,0}, normal{0,0,0};
    };
    // Ordinary native bodies are merged with the all-map prop index by callers.
    // propsOnly is reserved for native CharacterComponent surface queries.
    bool CastNativeScene(const wi::scene::Scene&,const XMFLOAT3& from,const XMFLOAT3& to,
        float radius,std::uint32_t layerMask,const std::vector<wi::ecs::Entity>& excludedRoots,
        bool propsOnly,NativeHit&,std::string& error,bool includeSensors=false);
    wi::scene::Scene::CapsuleIntersectionResult IntersectCapsule(
        const wi::scene::Scene&,const wi::primitive::Capsule&,std::uint32_t layerMask);
    wi::scene::Scene::RayIntersectionResult IntersectRay(
        const wi::scene::Scene&,const wi::primitive::Ray&,std::uint32_t layerMask);

}
