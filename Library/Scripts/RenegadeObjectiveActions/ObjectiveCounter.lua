if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Objective Counter",
        description = "Counts gameplay events, reports progress, and sends a completion event when the required count is reached.",
        category = "Objectives",
        role = "ACTION",
        properties = {
            {
                name = "start_active",
                label = "Start Active",
                description = "Begin counting immediately. Disable this to require Start Event first.",
                type = "boolean",
                default = true,
            },
            {
                name = "start_event",
                label = "Start Event",
                type = "string",
                default = "start_objective",
            },
            {
                name = "count_event",
                label = "Count Event",
                description = "Each matching event advances objective progress by one.",
                type = "string",
                default = "pickup",
            },
            {
                name = "required_count",
                label = "Required Count",
                type = "integer",
                default = 3,
                min = 1,
                max = 1000,
                step = 1,
            },
            {
                name = "completion_target",
                label = "Completion Target",
                description = "Optional target entity. Leave unset to broadcast Completion Event.",
                type = "entity",
            },
            {
                name = "completion_event",
                label = "Completion Event",
                type = "string",
                default = "open",
            },
            {
                name = "completion_payload",
                label = "Completion Payload",
                type = "string",
                default = "",
            },
            {
                name = "reset_event",
                label = "Reset Event",
                type = "string",
                default = "reset_objective",
            },
            {
                name = "progress_prefix",
                label = "Progress Text",
                description = "Displayed as '<text> current/required' after each counted event.",
                type = "string",
                default = "Objective",
            },
            {
                name = "completion_text",
                label = "Completion Text",
                type = "string",
                default = "Objective complete",
            },
            {
                name = "message_seconds",
                label = "Message Seconds",
                type = "float",
                default = 1.5,
                min = 0.0,
                max = 10.0,
                step = 0.1,
            },
            {
                name = "one_shot",
                label = "One Shot",
                description = "Ignore further Count Events after completion until Reset Event or gameplay reset.",
                type = "boolean",
                default = true,
            },
        },
    })
end

local function show_message(self, text)
    self._message = text or ""
    self._message_remaining = math.max(0.0, self.properties.message_seconds or 0.0)
end

local function reset_objective(self)
    self._active = self.properties.start_active
    self._count = 0
    self._completed = false
    self._message = ""
    self._message_remaining = 0.0
end

local function send_completion(self, incoming_payload)
    local payload = self.properties.completion_payload
    if payload == "" then
        payload = incoming_payload or ""
    end

    local target = self.properties.completion_target
    if target and renegade.entity.is_valid(target) then
        return renegade.events.send(
            target,
            self.properties.completion_event,
            payload)
    end

    return renegade.events.emit(
        self.properties.completion_event,
        payload)
end

return {
    on_start = function(self)
        reset_objective(self)
    end,

    on_reset = function(self)
        reset_objective(self)
    end,

    on_event = function(self, event)
        local reset_event = self.properties.reset_event
        if reset_event ~= "" and event.name == reset_event then
            reset_objective(self)
            return
        end

        local start_event = self.properties.start_event
        if start_event ~= "" and event.name == start_event then
            if not self._completed or not self.properties.one_shot then
                self._active = true
            end
            return
        end

        if not self._active then
            return
        end
        if self._completed and self.properties.one_shot then
            return
        end
        if event.name ~= self.properties.count_event then
            return
        end

        self._count = self._count + 1
        local required = math.max(1, self.properties.required_count)

        if self._count >= required then
            self._completed = true
            self._active = false
            show_message(self, self.properties.completion_text)
            send_completion(self, event.payload)
            return
        end

        show_message(
            self,
            self.properties.progress_prefix ..
                " " .. tostring(self._count) ..
                "/" .. tostring(required))
    end,

    on_update = function(self, dt)
        if self._message_remaining <= 0.0 or self._message == "" then
            return
        end

        renegade.ui.show_prompt(self._message)
        self._message_remaining = math.max(0.0, self._message_remaining - dt)
    end,
}
