if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Audio / Ambience",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Audio / Ambience settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.ambience.enter" then
        renegade.events.send(self.entity, "zone.apply.ambience.enter", event.payload)
    elseif event.name == "zone.ambience.exit" then
        renegade.events.send(self.entity, "zone.apply.ambience.exit", event.payload)
    end
end

return { on_event = on_event }
