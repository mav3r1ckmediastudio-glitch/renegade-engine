namespace wi::physics
{
    bool ReadRenegadeFrontSteeringAngle(const wi::scene::RigidBodyPhysicsComponent& body, float& radians)
    {
        if (!body.IsCar() || !body.physicsobject)
            return false;
        const auto& native = GetRigidBody(body);
        if (!native.vehicle_constraint || native.vehicle_constraint->GetWheels().size() < 2)
            return false;
        const auto& wheels = native.vehicle_constraint->GetWheels();
        radians = 0.5f * (wheels[0]->GetSteerAngle() + wheels[1]->GetSteerAngle());
        return std::isfinite(radians);
    }
}

bool renegade::bridge::ReadFrontSteeringAngle(const wi::scene::RigidBodyPhysicsComponent& body, float& radians)
{
    return wi::physics::ReadRenegadeFrontSteeringAngle(body, radians);
}
