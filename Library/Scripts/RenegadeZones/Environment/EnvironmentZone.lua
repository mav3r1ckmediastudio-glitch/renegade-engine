if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Environment",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Environment settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.environment.enter" then
        renegade.events.send(self.entity, "zone.apply.environment.enter", event.payload)
    elseif event.name == "zone.environment.exit" then
        renegade.events.send(self.entity, "zone.apply.environment.exit", event.payload)
    end
end

return { on_event = on_event }
