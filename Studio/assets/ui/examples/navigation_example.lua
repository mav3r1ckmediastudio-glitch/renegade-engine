-- Attach to a landmark entity. The authored HUD bar follows the live camera;
-- this script also creates an independent west-facing bar.
local nav = renegade.navigation
return {
    on_start = function(self)
        self.bar = assert(nav.create{
            name="Scripted west compass", x=896, y=156, width=320, height=88,
            source="manual", heading=270, span=120, categories="town",
            marker_gap=80, background_colour="171f2bca", border_colour="697a93ff"
        })
        local quest={id="navigation-demo.quest",label="Quest",category="quest",priority=100}
        if self.entity and renegade.entity.is_valid(self.entity) then
            quest.entity=self.entity
        else
            quest.position={x=0,y=0,z=8}
        end
        assert(nav.put_target(quest))
        assert(nav.put_target{
            id="navigation-demo.castle", label="Castle", category="castle",
            position={x=25,y=0,z=40}, priority=10
        })
        assert(nav.put_target{
            id="navigation-demo.town", label="Town", category="town",
            position={x=-20,y=0,z=0}
        })
        assert(nav.put_target{
            id="navigation-demo.merchant", label="Merchant", category="merchant",
            position={x=-16,y=0,z=30}
        })
    end,
    on_stop = function(self)
        if self.bar then nav.remove(self.bar) end
        nav.remove_target("navigation-demo.quest")
        nav.remove_target("navigation-demo.castle")
        nav.remove_target("navigation-demo.town")
        nav.remove_target("navigation-demo.merchant")
    end
}
