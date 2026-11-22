if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Teleport",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Teleport settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.teleport.enter" then
        renegade.events.send(self.entity, "zone.apply.teleport.enter", event.payload)
    end
end

return { on_event = on_event }
