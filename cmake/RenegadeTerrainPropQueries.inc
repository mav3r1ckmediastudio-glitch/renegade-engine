// Queries use the existing Jolt world; no second simulation or body inventory.
#include <Jolt/Physics/Collision/CollideShape.h>

namespace wi::physics::terrain_props
{
    namespace
    {
        wi::ecs::Entity EntityOf(const JPH::Body& body)
        {
            if(!body.GetUserData())return wi::ecs::INVALID_ENTITY;
            return body.IsRigidBody()?
                reinterpret_cast<const wi::physics::RigidBody*>(body.GetUserData())->entity:
                reinterpret_cast<const wi::physics::SoftBody*>(body.GetUserData())->entity;
        }
        class GameplayFilter final:public JPH::BodyFilter
        {
        public:
            GameplayFilter(const wi::scene::Scene& scene,std::uint32_t mask,
                const std::vector<wi::ecs::Entity>& excluded,bool propsOnly)
                :scene_(scene),mask_(mask),excluded_(excluded),propsOnly_(propsOnly){}
            bool ShouldCollideLocked(const JPH::Body& body) const override
            {
                const auto entity=EntityOf(body);
                if(entity==wi::ecs::INVALID_ENTITY)return false;
                const auto* metadata=scene_.metadatas.GetComponent(entity);
                const bool prop=metadata&&metadata->runtimeProjectionOnly&&
                    metadata->string_values.has("renegade.terrain_props.collision");
                if(prop!=propsOnly_)return false;
                const auto* layer=scene_.layers.GetComponent(entity);
                if(layer&&!(layer->GetLayerMask()&mask_))return false;
                for(const auto root:excluded_)
                    if(root==entity||scene_.Entity_IsDescendant(entity,root))return false;
                return mask_!=0;
            }
        private:
            const wi::scene::Scene& scene_;
            std::uint32_t mask_;
            const std::vector<wi::ecs::Entity>& excluded_;
            bool propsOnly_;
        };
    }

    bool CastNativeScene(const wi::scene::Scene& scene,const XMFLOAT3& from,
        const XMFLOAT3& to,float radius,std::uint32_t mask,
        const std::vector<wi::ecs::Entity>& excluded,bool propsOnly,
        NativeHit& output,std::string& error)
    {
        if(!Finite(from)||!Finite(to)||!std::isfinite(radius)||radius<0)
        {error="Invalid native gameplay segment.";return false;}
        const double dx=double(to.x)-from.x,dy=double(to.y)-from.y,dz=double(to.z)-from.z;
        const double length=std::hypot(dx,dy,dz);
        if(length>1000000)
        {error="Native gameplay segment exceeds the one-million-metre query bound.";return false;}
        NativeHit result;
        if(!scene.physics_scene||mask==0) {output=result;error.clear();return true;}
        const auto& physics=*static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
        const JPH::RVec3 origin(from.x,from.y,from.z);
        const JPH::Vec3 direction{float(dx),float(dy),float(dz)};
        GameplayFilter filter(scene,mask,excluded,propsOnly);
        if(radius==0)
        {
            if(length==0) {output=result;error.clear();return true;}
            JPH::RayCastSettings settings;settings.mTreatConvexAsSolid=true;
            JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
            collector.ResetEarlyOutFraction(std::nextafter(1.f,2.f));
            physics.physics_system.GetNarrowPhaseQuery().CastRay(
                JPH::RRayCast(origin,direction),settings,collector,{},{},filter);
            if(collector.HadHit()&&collector.mHit.mFraction<=1)
            {
                const auto& hit=collector.mHit;
                JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),hit.mBodyID);
                if(!lock.Succeeded()) {error="Native hit body became unavailable.";return false;}
                const auto point=origin+direction*hit.mFraction;
                const auto normal=lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,point);
                result.entity=EntityOf(lock.GetBody());result.fraction=std::max(0.f,hit.mFraction);
                result.position={float(point.GetX()),float(point.GetY()),float(point.GetZ())};
                result.normal={normal.GetX(),normal.GetY(),normal.GetZ()};
            }
        }
        else
        {
            const JPH::SphereShape sphere(radius);
            const JPH::RShapeCast cast(&sphere,JPH::Vec3::sReplicate(1),
                JPH::RMat44::sTranslation(origin),direction);
            JPH::ShapeCastSettings settings;settings.mReturnDeepestPoint=true;
            settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
            JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
            physics.physics_system.GetNarrowPhaseQuery().CastShape(cast,settings,origin,collector,{},{},filter);
            if(collector.HadHit()&&collector.mHit.mFraction<=1)
            {
                const auto& hit=collector.mHit;
                JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),hit.mBodyID2);
                if(!lock.Succeeded()) {error="Native swept hit body became unavailable.";return false;}
                const auto point=origin+hit.mContactPointOn2;
                const auto normal=(-hit.mPenetrationAxis).NormalizedOr(
                    -direction.NormalizedOr(JPH::Vec3::sAxisY()));
                result.entity=EntityOf(lock.GetBody());result.fraction=std::max(0.f,hit.mFraction);
                result.position={float(point.GetX()),float(point.GetY()),float(point.GetZ())};
                result.normal={normal.GetX(),normal.GetY(),normal.GetZ()};
            }
        }
        output=result;error.clear();return true;
    }

    wi::scene::Scene::CapsuleIntersectionResult IntersectCapsule(
        const wi::scene::Scene& scene,const wi::primitive::Capsule& capsule,std::uint32_t mask)
    {
        wi::scene::Scene::CapsuleIntersectionResult result;
        if(!scene.physics_scene||!Finite(capsule.base)||!Finite(capsule.tip)||
            !std::isfinite(capsule.radius)||capsule.radius<=0||!mask)return result;
        const auto& physics=*static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
        const JPH::Vec3 delta(capsule.tip.x-capsule.base.x,capsule.tip.y-capsule.base.y,
            capsule.tip.z-capsule.base.z);
        const float halfHeight=std::max(0.f,delta.Length()*.5f-capsule.radius);
        // Query shapes stay on the stack; native character contact iterations
        // do not allocate temporary heap-owned Jolt shapes.
        const JPH::CapsuleShape capsuleShape(std::max(halfHeight,.000001f),capsule.radius);
        const JPH::SphereShape sphereShape(capsule.radius);
        const JPH::Shape* shape=halfHeight>.000001f?
            static_cast<const JPH::Shape*>(&capsuleShape):static_cast<const JPH::Shape*>(&sphereShape);
        const JPH::RVec3 center(float((double(capsule.base.x)+capsule.tip.x)*.5),
            float((double(capsule.base.y)+capsule.tip.y)*.5),float((double(capsule.base.z)+capsule.tip.z)*.5));
        const auto rotation=JPH::Quat::sFromTo(JPH::Vec3::sAxisY(),delta.NormalizedOr(JPH::Vec3::sAxisY()));
        const std::vector<wi::ecs::Entity> excluded;
        GameplayFilter filter(scene,mask,excluded,true);
        JPH::CollideShapeSettings settings;
        JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> collector;
        physics.physics_system.GetNarrowPhaseQuery().CollideShape(shape,
            JPH::Vec3::sReplicate(1),JPH::RMat44::sRotationTranslation(rotation,center),
            settings,center,collector,{},{},filter);
        if(collector.HadHit()&&collector.mHit.mPenetrationDepth>0)
        {
            const auto& hit=collector.mHit;
            JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),hit.mBodyID2);
            if(!lock.Succeeded())return result;
            const auto normal=(-hit.mPenetrationAxis).NormalizedOr(JPH::Vec3::sAxisY());
            const auto point=center+hit.mContactPointOn2;
            result.entity=EntityOf(lock.GetBody());result.depth=hit.mPenetrationDepth;
            result.normal={normal.GetX(),normal.GetY(),normal.GetZ()};
            result.position={float(point.GetX()),float(point.GetY()),float(point.GetZ())};
        }
        return result;
    }

    wi::scene::Scene::RayIntersectionResult IntersectRay(
        const wi::scene::Scene& scene,const wi::primitive::Ray& ray,std::uint32_t mask)
    {
        wi::scene::Scene::RayIntersectionResult result;
        if(!std::isfinite(ray.TMin)||!std::isfinite(ray.TMax)||ray.TMin<0||ray.TMax<ray.TMin)
            return result;
        const auto direction=XMLoadFloat3(&ray.direction);
        XMFLOAT3 from,to;
        XMStoreFloat3(&from,XMLoadFloat3(&ray.origin)+direction*ray.TMin);
        XMStoreFloat3(&to,XMLoadFloat3(&ray.origin)+direction*std::min(ray.TMax,1000000.f));
        NativeHit hit;std::string error;
        if(CastNativeScene(scene,from,to,0,mask,{},true,hit,error)&&hit.entity!=wi::ecs::INVALID_ENTITY)
        {
            result.entity=hit.entity;result.position=hit.position;result.normal=hit.normal;
            const auto dx=double(hit.position.x)-ray.origin.x,dy=double(hit.position.y)-ray.origin.y,
                dz=double(hit.position.z)-ray.origin.z;
            result.distance=float(std::hypot(dx,dy,dz));
        }
        return result;
    }
    bool HasLiveBody(const wi::scene::Scene& scene,wi::ecs::Entity entity) noexcept
    {
        const auto* component=scene.rigidbodies.GetComponent(entity);
        if(!scene.physics_scene||!component||!component->physicsobject)return false;
        const auto& native=*static_cast<const wi::physics::RigidBody*>(component->physicsobject.get());
        if(native.physics_scene!=scene.physics_scene||native.bodyID.IsInvalid())return false;
        const auto& physics=*static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
        JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),native.bodyID);
        return lock.Succeeded()&&lock.GetBody().IsInBroadPhase();
    }
    wi::ecs::Entity CharacterSupportEntity(const wi::scene::Scene& scene,wi::ecs::Entity entity) noexcept
    {
        if(!HasLiveBody(scene,entity))return wi::ecs::INVALID_ENTITY;
        const auto& native=*static_cast<const wi::physics::RigidBody*>(
            scene.rigidbodies.GetComponent(entity)->physicsobject.get());
        if(!native.character||!native.character->IsSupported())return wi::ecs::INVALID_ENTITY;
        const auto ground=native.character->GetGroundBodyID();
        if(ground.IsInvalid())return wi::ecs::INVALID_ENTITY;
        const auto& physics=*static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
        JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),ground);
        // Pinned Wicked user data is a RigidBody/SoftBody owner pointer.
        // Resolving through the live body also rejects removed/reused IDs.
        return lock.Succeeded()?EntityOf(lock.GetBody()):wi::ecs::INVALID_ENTITY;
    }
    bool BodyBounds(const wi::scene::Scene& scene,wi::ecs::Entity entity,
        XMFLOAT3& minimum,XMFLOAT3& maximum) noexcept
    {
        if(!HasLiveBody(scene,entity))return false;
        const auto& native=*static_cast<const wi::physics::RigidBody*>(
            scene.rigidbodies.GetComponent(entity)->physicsobject.get());
        const auto& physics=*static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
        JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),native.bodyID);
        if(!lock.Succeeded())return false;
        const auto& bounds=lock.GetBody().GetWorldSpaceBounds();
        minimum={bounds.mMin.GetX(),bounds.mMin.GetY(),bounds.mMin.GetZ()};
        maximum={bounds.mMax.GetX(),bounds.mMax.GetY(),bounds.mMax.GetZ()};
        return true;
    }
    bool IsBodySleeping(const wi::scene::Scene& scene,wi::ecs::Entity entity) noexcept
    {
        if(!HasLiveBody(scene,entity))return false;
        const auto& native=*static_cast<const wi::physics::RigidBody*>(
            scene.rigidbodies.GetComponent(entity)->physicsobject.get());
        const auto& physics=*static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get());
        JPH::BodyLockRead lock(physics.physics_system.GetBodyLockInterface(),native.bodyID);
        return lock.Succeeded()&&lock.GetBody().GetMotionType()==JPH::EMotionType::Dynamic&&
            !lock.GetBody().IsActive();
    }

}
