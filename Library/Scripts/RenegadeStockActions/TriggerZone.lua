if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Player Trigger Zone",
        description = "Sends enter and exit events when the player crosses a spherical trigger radius.",
        category = "Triggers",
        role = "ACTION",
        properties = {
            {
                name = "target",
                label = "Target",
                description = "Optional target entity. Leave unset to broadcast events.",
                type = "entity",
            },
            {
                name = "radius",
                label = "Radius",
                type = "float",
                default = 2.0,
                min = 0.1,
                max = 1000.0,
                step = 0.1,
            },
            {
                name = "enter_event",
                label = "Enter Event",
                type = "string",
                default = "enter",
            },
            {
                name = "exit_event",
                label = "Exit Event",
                type = "string",
                default = "exit",
            },
            {
                name = "payload",
                label = "Payload",
                type = "string",
                default = "",
            },
            {
                name = "one_shot",
                label = "One Shot",
                type = "boolean",
                default = false,
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

local function send(self, name)
    if name == "" then
        return true
    end
    local target = self.properties.target
    if target and renegade.entity.is_valid(target) then
        return renegade.events.send(target, name, self.properties.payload)
    end
    return renegade.events.emit(name, self.properties.payload)
end

return {
    on_start = function(self)
        self._inside = false
        self._done = false
    end,

    on_update = function(self, dt)
        if self._done then
            return
        end
        if not renegade.player.is_present() then
            self._inside = false
            return
        end

        local player_position = renegade.player.get_position()
        local zone_position = renegade.transform.get_world_position(self.entity)
        if not player_position or not zone_position then
            return
        end

        local radius = self.properties.radius
        local inside = distance_squared(player_position, zone_position) <= radius * radius

        if inside and not self._inside then
            if send(self, self.properties.enter_event) and self.properties.one_shot then
                self._done = true
            end
        elseif not inside and self._inside then
            send(self, self.properties.exit_event)
        end

        self._inside = inside
    end,
}
