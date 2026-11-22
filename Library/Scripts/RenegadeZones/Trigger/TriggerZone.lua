if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Custom Trigger",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Handles player entry and exit using the events and target in the zone inspector.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.trigger.enter" then
        renegade.events.send(self.entity, "zone.forward.enter", event.payload)
    elseif event.name == "zone.trigger.exit" then
        renegade.events.send(self.entity, "zone.forward.exit", event.payload)
    end
end

return { on_event = on_event }
