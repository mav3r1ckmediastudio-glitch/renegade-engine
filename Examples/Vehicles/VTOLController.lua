-- Attach to the dynamic hull and set Player Controls to Scripted VTOL.
local physics = renegade.physics
local target_altitude

local function fly(self)
    local thrust, yaw, climb = physics.vtol_pilot_input(self.entity)
    if thrust == nil then
        target_altitude = nil
        return
    end

    -- Change these gains to tune the pilot's response.
    target_altitude = physics.vtol_pilot_drive(
        self.entity, thrust * 0.8, yaw * 0.75, climb, target_altitude
    )
end

return {
    on_update = fly,
    on_reset = function() target_altitude = nil end,
    on_stop = function() target_altitude = nil end,
}
