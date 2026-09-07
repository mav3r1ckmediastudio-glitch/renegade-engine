if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Activation Relay",
        description = "Receives one event and forwards another event, optionally after a delay.",
        category = "Logic",
        role = "ACTION",
        properties = {
            {
                name = "input_event",
                label = "Input Event",
                type = "string",
                default = "activate",
            },
            {
                name = "target",
                label = "Target",
                description = "Optional target entity. Leave unset to broadcast the output event.",
                type = "entity",
            },
            {
                name = "output_event",
                label = "Output Event",
                type = "string",
                default = "activate",
            },
            {
                name = "payload",
                label = "Payload",
                description = "When empty, the incoming payload is forwarded.",
                type = "string",
                default = "",
            },
            {
                name = "delay",
                label = "Delay",
                type = "float",
                default = 0.0,
                min = 0.0,
                max = 3600.0,
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

local function forward(self, incoming_payload)
    local payload = self.properties.payload
    if payload == "" then
        payload = incoming_payload or ""
    end

    local target = self.properties.target
    if target and renegade.entity.is_valid(target) then
        return renegade.events.send(
            target,
            self.properties.output_event,
            payload)
    end
    return renegade.events.emit(
        self.properties.output_event,
        payload)
end

return {
    on_start = function(self)
        self._fired = false
        self._pending = false
        self._remaining = 0.0
        self._pending_payload = ""
    end,

    on_event = function(self, event)
        if event.name ~= self.properties.input_event then
            return
        end
        if self._fired and self.properties.one_shot then
            return
        end

        if self.properties.delay <= 0.0 then
            if forward(self, event.payload) then
                self._fired = true
            end
            return
        end

        self._pending = true
        self._remaining = self.properties.delay
        self._pending_payload = event.payload
    end,

    on_update = function(self, dt)
        if not self._pending then
            return
        end
        self._remaining = self._remaining - dt
        if self._remaining > 0.0 then
            return
        end

        self._pending = false
        if forward(self, self._pending_payload) then
            self._fired = true
        end
    end,
}
