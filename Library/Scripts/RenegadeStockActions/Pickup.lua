if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Proximity Pickup",
        description = "Collects this entity when the player enters its pickup radius and sends an event.",
        category = "Gameplay",
        role = "ACTION",
        properties = {
            {
                name = "target",
                label = "Target",
                description = "Optional target entity. Leave unset to broadcast the pickup event.",
                type = "entity",
            },
            {
                name = "pickup_event",
                label = "Pickup Event",
                type = "string",
                default = "pickup",
            },
            {
                name = "payload",
                label = "Payload",
                type = "string",
                default = "",
            },
            {
                name = "pickup_radius",
                label = "Pickup Radius",
                type = "float",
                default = 1.0,
                min = 0.1,
                max = 50.0,
                step = 0.1,
            },
            {
                name = "require_interact",
                label = "Require Interact",
                description = "Show a prompt and wait for Interact instead of collecting automatically.",
                type = "boolean",
                default = false,
            },
            {
                name = "prompt_text",
                label = "Prompt Text",
                type = "string",
                default = "Press E to pick up",
            },
            {
                name = "hide_on_pickup",
                label = "Hide On Pickup",
                type = "boolean",
                default = true,
            },
        },
    })
end

local function distance_squared(a, b)
    local dx = a.x - b.x
    local dy = a.y - b.y
    local dz = a.z - b.z
    return dx * dx + dy * dy + dz * dz
end

local function send_pickup(self)
    local target = self.properties.target
    if target and renegade.entity.is_valid(target) then
        return renegade.events.send(
            target,
            self.properties.pickup_event,
            self.properties.payload)
    end
    return renegade.events.emit(
        self.properties.pickup_event,
        self.properties.payload)
end

return {
    on_start = function(self)
        self._collected = false
        self._original_scale = renegade.transform.get_local_scale(self.entity)
    end,

    on_update = function(self, dt)
        if self._collected or not renegade.player.is_present() then
            return
        end

        local player_position = renegade.player.get_position()
        local pickup_position = renegade.transform.get_world_position(self.entity)
        if not player_position or not pickup_position then
            return
        end

        local radius = self.properties.pickup_radius
        if distance_squared(player_position, pickup_position) > radius * radius then
            return
        end

        if self.properties.require_interact then
            renegade.ui.show_prompt(self.properties.prompt_text)
            if not renegade.input.was_pressed("interact") then
                return
            end
        end

        if send_pickup(self) then
            self._collected = true
            if self.properties.hide_on_pickup then
                renegade.transform.set_local_scale(
                    self.entity,
                    { x = 0.0, y = 0.0, z = 0.0 })
            end
        end
    end,

    on_stop = function(self)
        if self._collected and self.properties.hide_on_pickup and self._original_scale then
            renegade.transform.set_local_scale(self.entity, self._original_scale)
        end
    end,
}
