-- Renegade G3-B reference Action (not automatically installed in projects).
-- Attach to a world Entity with stable scene identity. Configure properties:
--   source_container : String (e.g. "world:herb-pickup" or "chest")
--   target_container : String (e.g. "player" or "chest")
--   interaction_radius : Float (e.g. 2.5)
--   quantity : Integer (e.g. 1)
--   prompt : String (e.g. "Press Interact to pick up potion")
-- Runtime must be explicitly opted into a .rinventory snapshot, and this
-- script must be granted read+withdraw(source) and read+deposit(target).
-- Does not remove meshes when empty; world presentation remains separate.
return {
    on_update = function(self, dt)
        local inv = renegade.inventory
        if inv == nil or self.entity == nil then return end
        local props = self.properties or {}
        local source = props.source_container
        local target = props.target_container
        if type(source) ~= "string" or type(target) ~= "string" then return end
        if source == target or #source == 0 or #target == 0 then return end

        local page = inv.list(source, 0, 1)
        if page == nil or #page.stacks == 0 then return end
        local world = renegade.transform.get_world_position(self.entity)
        local player = renegade.player.get_position()
        if world == nil or player == nil then return end

        local radius = props.interaction_radius or 2.5
        if type(radius) ~= "number" or radius <= 0 or radius > 8 then
            radius = 2.5
        end
        local dx = world.x - player.x
        local dy = world.y - player.y
        local dz = world.z - player.z
        if dx * dx + dy * dy + dz * dz > radius * radius then return end

        local label = props.prompt or "Press Interact to transfer an item"
        renegade.ui.show_prompt(label)
        if not renegade.input.was_pressed("interact") then return end

        local stack = page.stacks[1]
        local amount = props.quantity or 1
        if type(amount) ~= "number" or amount < 1 or amount % 1 ~= 0 then
            amount = 1
        end
        if amount > stack.quantity then amount = stack.quantity end
        local ok = inv.transfer(source, target, stack.instance_id, amount)
        if not ok then
            renegade.ui.show_prompt("Inventory transfer unavailable")
        end
    end
}
