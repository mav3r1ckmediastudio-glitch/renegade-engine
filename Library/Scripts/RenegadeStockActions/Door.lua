if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Sliding Door",
        description = "Slides this entity with a nearby Interact prompt or open, close, and toggle events.",
        category = "Interaction",
        role = "ACTION",
        properties = {
            {
                name = "open_offset",
                label = "Open Offset",
                description = "Local-space offset from the closed position.",
                type = "vector3",
                default = { x = 0.0, y = 2.0, z = 0.0 },
            },
            {
                name = "speed",
                label = "Speed",
                description = "Movement speed in metres per second.",
                type = "float",
                default = 2.0,
                min = 0.01,
                max = 100.0,
                step = 0.1,
            },
            {
                name = "start_open",
                label = "Start Open",
                type = "boolean",
                default = false,
            },
            {
                name = "direct_interaction",
                label = "Direct Interaction",
                description = "Let the player use this door directly with the Interact key.",
                type = "boolean",
                default = true,
            },
            {
                name = "interaction_distance",
                label = "Interaction Distance",
                type = "float",
                default = 2.0,
                min = 0.1,
                max = 50.0,
                step = 0.1,
            },
            {
                name = "prompt_text",
                label = "Prompt Text",
                type = "string",
                default = "Press E to open / close",
            },
            {
                name = "auto_close",
                label = "Auto Close",
                type = "boolean",
                default = false,
            },
            {
                name = "auto_close_delay",
                label = "Auto Close Delay",
                type = "float",
                default = 3.0,
                min = 0.0,
                max = 120.0,
                step = 0.1,
            },
            {
                name = "open_event",
                label = "Open Event",
                type = "string",
                default = "open",
            },
            {
                name = "close_event",
                label = "Close Event",
                type = "string",
                default = "close",
            },
            {
                name = "toggle_event",
                label = "Toggle Event",
                type = "string",
                default = "toggle",
            },
        },
    })
end

local function add(a, b)
    return { x = a.x + b.x, y = a.y + b.y, z = a.z + b.z }
end

local function move_towards(current, target, max_delta)
    local dx = target.x - current.x
    local dy = target.y - current.y
    local dz = target.z - current.z
    local distance = math.sqrt(dx * dx + dy * dy + dz * dz)
    if distance <= max_delta or distance <= 0.000001 then
        return { x = target.x, y = target.y, z = target.z }
    end
    local scale = max_delta / distance
    return {
        x = current.x + dx * scale,
        y = current.y + dy * scale,
        z = current.z + dz * scale,
    }
end

local function distance_squared(a, b)
    local dx = a.x - b.x
    local dy = a.y - b.y
    local dz = a.z - b.z
    return dx * dx + dy * dy + dz * dz
end

local function set_open(self, open)
    self._open = open
    self._target = open and self._open_position or self._closed_position
    if open and self.properties.auto_close then
        self._auto_close_remaining = self.properties.auto_close_delay
    else
        self._auto_close_remaining = nil
    end
end

return {
    on_start = function(self)
        local position = renegade.transform.get_local_position(self.entity)
        if not position then
            return
        end
        self._closed_position = position
        self._open_position = add(position, self.properties.open_offset)
        set_open(self, self.properties.start_open)
        if self._open then
            renegade.transform.set_local_position(self.entity, self._open_position)
        end
    end,

    on_event = function(self, event)
        if not self._target then
            return
        end
        if event.name == self.properties.open_event then
            set_open(self, true)
        elseif event.name == self.properties.close_event then
            set_open(self, false)
        elseif event.name == self.properties.toggle_event then
            set_open(self, not self._open)
        end
    end,

    on_update = function(self, dt)
        if not self._target then
            return
        end

        if self.properties.direct_interaction and renegade.player.is_present() then
            local player_position = renegade.player.get_position()
            local door_position = renegade.transform.get_world_position(self.entity)
            local radius = self.properties.interaction_distance
            if player_position and door_position and
                distance_squared(player_position, door_position) <= radius * radius then
                renegade.ui.show_prompt(self.properties.prompt_text)
                if renegade.input.was_pressed("interact") then
                    set_open(self, not self._open)
                end
            end
        end

        if self._auto_close_remaining then
            self._auto_close_remaining = self._auto_close_remaining - dt
            if self._auto_close_remaining <= 0.0 then
                set_open(self, false)
            end
        end

        local current = renegade.transform.get_local_position(self.entity)
        if not current then
            return
        end
        local next_position = move_towards(
            current,
            self._target,
            self.properties.speed * dt)
        renegade.transform.set_local_position(self.entity, next_position)
    end,
}
