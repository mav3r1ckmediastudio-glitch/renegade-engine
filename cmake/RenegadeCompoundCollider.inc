        struct CompoundShapeSource
        {
            renegade::bridge::CompoundParts parts;
            XMFLOAT3 scale{1.f, 1.f, 1.f};
            uint64_t signature = 0;
        };

        CompoundShapeSource ReadCompoundShape(const Scene& scene, Entity entity,
            const RigidBodyPhysicsComponent& body, const TransformComponent& transform)
        {
            using namespace renegade::bridge;
            CompoundShapeSource source;
            if (!SupportsCompoundCollider(body) || !HasCompoundCollider(scene, entity))
                return source;
            source.parts = GetCompoundParts(scene, entity);
            if (!source.parts.valid || source.parts.count == 0)
                return source;
            // Derive a stable scale directly from the authored transform on unparented
            // bodies. Jolt pose feedback can otherwise change the decomposed world scale
            // by a few bits on every update.
            const auto scale = scene.hierarchy.Contains(entity) ? transform.GetScale() : transform.scale_local;
            const auto stable = [](float value) { return std::round(value * 10000.f) / 10000.f; };
            source.scale = {stable(scale.x), stable(scale.y), stable(scale.z)};
            if (!std::isfinite(source.scale.x) || !std::isfinite(source.scale.y) || !std::isfinite(source.scale.z) ||
                std::abs(source.scale.x) < .001f || std::abs(source.scale.y) < .001f || std::abs(source.scale.z) < .001f)
                return source;

            uint64_t fingerprint = 14695981039346656037ull;
            const auto add = [&](float value) {
                uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                fingerprint = (fingerprint ^ bits) * 1099511628211ull;
            };
            add(source.scale.x);
            add(source.scale.y);
            add(source.scale.z);
            for (size_t i = 0; i < source.parts.count; ++i)
            {
                const auto& part = source.parts.parts[i];
                add(static_cast<float>(part.shape));
                add(part.position.x); add(part.position.y); add(part.position.z);
                add(part.orientation.x); add(part.orientation.y); add(part.orientation.z); add(part.orientation.w);
                add(part.halfExtents.x); add(part.halfExtents.y); add(part.halfExtents.z);
            }
            source.signature = fingerprint ? fingerprint : 1;
            return source;
        }

        ShapeRefC CreateCompoundShape(const CompoundShapeSource& source)
        {
            using renegade::bridge::CompoundPartShape;
            if (!source.signature)
                return nullptr;

            StaticCompoundShapeSettings compound;
            for (size_t i = 0; i < source.parts.count; ++i)
            {
                const auto& part = source.parts.parts[i];
                const auto position = cast(part.position);
                const auto rotation = Quat(part.orientation.x, part.orientation.y, part.orientation.z, part.orientation.w);
                const auto& half = part.halfExtents;
                if (part.shape == CompoundPartShape::Box)
                {
                    const auto shape = BoxShapeSettings(cast(half), 0.f).Create();
                    if (!shape.IsValid()) return nullptr;
                    compound.AddShape(position, rotation, shape.Get().GetPtr());
                }
                else if (part.shape == CompoundPartShape::Sphere)
                {
                    const auto shape = SphereShapeSettings(half.x).Create();
                    if (!shape.IsValid()) return nullptr;
                    compound.AddShape(position, rotation, shape.Get().GetPtr());
                }
                else
                {
                    const auto shape = CapsuleShapeSettings(half.y, half.x).Create();
                    if (!shape.IsValid()) return nullptr;
                    compound.AddShape(position, rotation, shape.Get().GetPtr());
                }
            }
            const auto shape = compound.Create();
            if (!shape.IsValid()) return nullptr;
            const auto scaled = shape.Get()->ScaleShape(cast(source.scale));
            if (!scaled.IsValid()) return nullptr;
            return ShapeRefC(scaled.Get());
        }
