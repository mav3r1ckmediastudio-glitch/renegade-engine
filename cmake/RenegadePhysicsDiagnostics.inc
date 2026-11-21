namespace renegade::bridge
{
    PhysicsDiagnostics SamplePhysicsDiagnostics(wi::scene::Scene& scene)
    {
        if (!scene.physics_scene || !wi::physics::IsEnabled())
            return {};
        auto& native = wi::physics::GetPhysicsScene(scene);
        native.diagnostics_requested = true;
        auto stats = native.diagnostics;
        stats.ready = true;
        return stats;
    }

    bool RequestPhysicsStep(wi::scene::Scene& scene)
    {
        if (!scene.physics_scene || !wi::physics::IsEnabled() || wi::physics::IsSimulationEnabled())
            return false;
        auto& native = wi::physics::GetPhysicsScene(scene);
        return !std::exchange(native.single_step_requested, true);
    }
}
