if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Interaction Switch",
        description = "Sends an event when the player presses Interact while close to this entity.",
        category = "Interaction",
        role = "ACTION",
        properties = {
            {
                name = "target",
                label = "Target",
                description = "Optional target entity. Leave unset to broadcast the event.",
                type = "entity",
            },
            {
                name = "event_name",
                label = "Event",
                type = "string",
                default = "activate",
            },
            {
                name = "payload",
                label = "Payload",
                type = "string",
                default = "",
            },
            {
                name = "use_distance",
                label = "Use Distance",
                type = "float",
                default = 2.0,
                min = 0.1,
                max = 50.0,
                step = 0.1,
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

local function send_event(self)
    local target = self.properties.target
    if target and renegade.entity.is_valid(target) then
        return renegade.events.send(
            target,
            self.properties.event_name,
            self.properties.payload)
    end
    return renegade.events.emit(
        self.properties.event_name,
        self.properties.payload)
end

return {
    on_start = function(self)
        self._used = false
    end,

    on_update = function(self, dt)
        if self._used and self.properties.one_shot then
            return
        end
        if not renegade.player.is_present() then
            return
        end

        local player_position = renegade.player.get_position()
        local switch_position = renegade.transform.get_world_position(self.entity)
        if not player_position or not switch_position then
            return
        end

        local radius = self.properties.use_distance
        if distance_squared(player_position, switch_position) > radius * radius then
            return
        end

        if renegade.input.was_pressed("interact") then
            if send_event(self) then
                self._used = true
            end
        end
    end,
}
