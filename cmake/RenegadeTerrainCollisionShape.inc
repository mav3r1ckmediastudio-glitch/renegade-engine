Vec3 bottom_offset = Vec3::sZero();

        // Pinned terrain meshes and Jolt heightfields use opposite cell diagonals.
        // Retain compact native heightfields for constant-height chunks; indexed
        // non-flat static heightfields must collide with their rendered LOD0.
        bool renegade_exact_heightfield_mesh = false;
        if (physicscomponent.shape == RigidBodyPhysicsComponent::CollisionShape::HEIGHTFIELD &&
            physicscomponent.mass == 0 && mesh != nullptr &&
            !mesh->vertex_positions.empty() && !mesh->indices.empty())
        {
            uint32_t first = 0, last = 0;
            mesh->GetLODSubsetRange(0, first, last);
            bool has_triangles = false;
            for (uint32_t i = first; i < last; ++i)
                has_triangles = has_triangles || mesh->subsets[i].indexCount >= 3;
            if (has_triangles)
            {
                const float height = mesh->vertex_positions.front().y;
                for (const auto& vertex : mesh->vertex_positions)
                    if (vertex.y != height)
                    {
                        renegade_exact_heightfield_mesh = true;
                        break;
                    }
            }
        }
        const auto renegade_collision_shape = renegade_exact_heightfield_mesh
            ? RigidBodyPhysicsComponent::CollisionShape::TRIANGLE_MESH
            : physicscomponent.shape;
        switch (renegade_collision_shape)
