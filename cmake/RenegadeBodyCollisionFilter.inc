namespace wi::physics
{
    namespace
    {
        // Keep Wicked's motion class in the low two bits. An unmodified
        // Wicked layer decodes to gameplay layer zero with every bit allowed.
        static constexpr JPH::ObjectLayer MotionLayerBits = 3;
        static_assert(renegade::bridge::BodyCollisionLayerCount == 8);

        JPH::ObjectLayer MotionLayer(JPH::ObjectLayer layer)
        {
            return layer & MotionLayerBits;
        }

        JPH::ObjectLayer WithMotionLayer(JPH::ObjectLayer layer, JPH::ObjectLayer motion)
        {
            return JPH::ObjectLayer((layer & ~MotionLayerBits) | motion);
        }

        JPH::ObjectLayer EncodeCollisionLayer(JPH::ObjectLayer motion, renegade::bridge::BodyCollisionFilter filter)
        {
            return JPH::ObjectLayer(motion | (filter.layer << 2) |
                ((filter.mask ^ renegade::bridge::AllBodyCollisionLayers) << 5));
        }

        bool CollisionMasksAllow(JPH::ObjectLayer first, JPH::ObjectLayer second)
        {
            const int firstLayer = (first >> 2) & 7;
            const int secondLayer = (second >> 2) & 7;
            const int firstBlocked = first >> 5;
            const int secondBlocked = second >> 5;
            return !(firstBlocked & (1 << secondLayer)) && !(secondBlocked & (1 << firstLayer));
        }
    }
}
