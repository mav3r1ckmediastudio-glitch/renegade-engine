if renegade and renegade.metadata then
    renegade.metadata({
        schema_version=1, name="Location Zone", category="Gameplay Zones",
        role="SCRIPT", description="Editable location entry and exit events. Name, icon and proximity announcements use the owning Location Zone settings.", properties={}
    })
end
return {
    on_event=function(self,event)
        if not renegade.entity.equals(event.target,self.entity) then return end
        if event.name=="zone.location.enter" then
            renegade.events.send(self.entity,"location.entered",event.payload)
        elseif event.name=="zone.location.exit" then
            renegade.events.send(self.entity,"location.exited",event.payload)
        end
    end
}
