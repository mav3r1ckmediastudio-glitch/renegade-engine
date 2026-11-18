// Appended to the existing generated physics wrapper; see TERRAIN_PROP_P3.md.
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <cmath>
#include <limits>

namespace wi::physics::terrain_props
{
    namespace
    {
        struct ShapeState { JPH::ShapeRefC shape; };
        bool Finite(const XMFLOAT3& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
        const ShapeState& State(const std::shared_ptr<const void>& p)
        {
            return *static_cast<const ShapeState*>(p.get());
        }
    }

    bool CreateShape(ShapeKind kind, const XMFLOAT3& extent, Shape& output, std::string& error)
    {
        if (JPH::Factory::sInstance == nullptr || !Finite(extent) ||
            extent.x <= 0 || extent.y <= 0 || extent.z <= 0 ||
            (kind != ShapeKind::Capsule && kind != ShapeKind::Box))
        {
            error = "Terrain collision requires initialized physics and positive finite dimensions.";
            return false;
        }
        wi::scene::RigidBodyPhysicsComponent prototype;
        prototype.shape = kind == ShapeKind::Capsule ? prototype.CAPSULE : prototype.BOX;
        prototype.box.halfextents = extent;
        prototype.capsule.radius = extent.x;
        prototype.capsule.height = extent.y;
        wi::physics::CreateRigidBodyShape(prototype);
        const auto& body = wi::physics::GetRigidBody(prototype);
        if (body.shape == nullptr) { error = "Native terrain collision shape creation failed."; return false; }
        auto state = std::make_shared<ShapeState>();
        state->shape = body.shape;
        Shape candidate;
        candidate.state_ = std::move(state);
        output = std::move(candidate);
        error.clear();
        return true;
    }

    bool CastSegment(const Shape& shape, const XMFLOAT3& from, const XMFLOAT3& to,
        float radius, CastHit& output, std::string& error)
    {
        if (!shape.IsValid() || !Finite(from) || !Finite(to) ||
            !std::isfinite(radius) || radius < 0)
        { error = "Invalid terrain collision query."; return false; }
        const JPH::Vec3 start(from.x, from.y, from.z);
        const JPH::Vec3 direction(to.x-from.x, to.y-from.y, to.z-from.z);
        if (!std::isfinite(direction.LengthSq()))
        { error = "Terrain collision query exceeds finite native range."; return false; }
        CastHit result;
        const auto& native = State(shape.state_).shape;
        if (radius == 0)
        {
            JPH::RayCast ray(start, direction);
            JPH::RayCastResult hit;
            // Include a contact at exactly the segment endpoint.
            hit.mFraction = std::nextafter(1.f, 2.f);
            if (native->CastRay(ray, JPH::SubShapeIDCreator(), hit) && hit.mFraction <= 1.f)
            {
                result.hit = true;
                result.fraction = std::max(0.f, hit.mFraction);
                const auto normal = hit.mFraction > 0 ?
                    native->GetSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction)) :
                    -direction.NormalizedOr(JPH::Vec3::sAxisY());
                result.normal = {normal.GetX(), normal.GetY(), normal.GetZ()};
            }
        }
        else
        {
            JPH::ShapeRefC sphere = new JPH::SphereShape(radius);
            JPH::ShapeCast cast(sphere.GetPtr(), JPH::Vec3::sReplicate(1),
                JPH::Mat44::sTranslation(start), direction);
            JPH::ShapeCastSettings settings;
            settings.mReturnDeepestPoint = true;
            settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
            JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
            JPH::CollisionDispatch::sCastShapeVsShapeLocalSpace(cast, settings,
                native.GetPtr(), JPH::Vec3::sReplicate(1), JPH::ShapeFilter(),
                JPH::Mat44::sIdentity(), JPH::SubShapeIDCreator(), JPH::SubShapeIDCreator(), collector);
            if (collector.HadHit() && collector.mHit.mFraction <= 1.f)
            {
                result.hit = true;
                result.fraction = std::max(0.f, collector.mHit.mFraction);
                const auto normal = (-collector.mHit.mPenetrationAxis).NormalizedOr(
                    -direction.NormalizedOr(JPH::Vec3::sAxisY()));
                result.normal = {normal.GetX(), normal.GetY(), normal.GetZ()};
            }
        }
        output = result;
        error.clear();
        return true;
    }

    Capacity BodyCapacity(const wi::scene::Scene& scene) noexcept
    {
        Capacity result;
        result.maximum = wi::physics::cMaxBodies;
        if (scene.physics_scene != nullptr)
        {
            const auto* physics = static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
            result.allocated = physics->physics_system.GetNumBodies();
        }
        return result;
    }

    bool PrepareStaticBody(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const Shape& shape, float scale, std::uint32_t reserve, std::string& error)
    {
        auto* body = scene.rigidbodies.GetComponent(entity);
        const auto* transform = scene.transforms.GetComponent(entity);
        if (!shape.IsValid() || !body || !transform || body->mass != 0 ||
            body->IsCharacterPhysics() || body->IsVehicle() || body->physicsobject != nullptr ||
            !std::isfinite(scale) || scale <= 0 || !Finite(transform->GetPosition()))
        { error = "Expected a new static terrain body at a physics-safe point."; return false; }
        auto capacity = BodyCapacity(scene);
        if (reserve >= capacity.maximum || capacity.allocated >= capacity.maximum - reserve)
        { error = "Terrain collision cannot consume the reserved native body capacity."; return false; }
        // Initialize scene ownership before AddRigidBody captures its shared lifetime.
        (void)wi::physics::GetPhysicsScene(scene);
        auto& native = wi::physics::GetRigidBody(*body);
        if (scale == 1.f) native.shape = State(shape.state_).shape;
        else
        {
            auto scaled = State(shape.state_).shape->ScaleShape(JPH::Vec3::sReplicate(scale));
            if (!scaled.IsValid()) { body->physicsobject.reset(); error = scaled.GetError().c_str(); return false; }
            native.shape = scaled.Get();
        }
        wi::physics::AddRigidBody(scene, entity, *body, *transform, nullptr);
        if (native.bodyID.IsInvalid())
        { body->physicsobject.reset(); error = "Native terrain body allocation failed."; return false; }
        body->SetRefreshParametersNeeded(false);
        error.clear();
        return true;
    }
}
