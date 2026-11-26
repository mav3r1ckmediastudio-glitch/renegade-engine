if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Message / Tutorial",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Message / Tutorial settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.message.enter" then
        renegade.events.send(self.entity, "zone.apply.message.enter", event.payload)
    elseif event.name == "zone.message.exit" then
        renegade.events.send(self.entity, "zone.apply.message.exit", event.payload)
    end
end

return { on_event = on_event }
