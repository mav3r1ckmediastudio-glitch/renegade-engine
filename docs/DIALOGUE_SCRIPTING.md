# Dialogue from creator Lua

Use the normal governed SCRIPT or GLOBAL SCRIPT workflow. Author and save the
conversation in **Add > Dialogue Editor** first. Ordinary NPC interaction still
works without a script. These functions control the same Runtime conversation,
Screen panel, audio and saved memory as the Interact key.

`renegade.dialogue.contract_version` is `1`.

| Call | Result |
| --- | --- |
| `start(ctx.entity)` | Start the conversation assigned to this script's NPC. |
| `start(ctx.properties.guard)` | Start an NPC selected in an Entity Reference property. |
| `start("Gate Guard")` | Start a saved conversation by exact, case-sensitive name. |
| `start("asset-uuid", "captain")` | Start by asset ID at an authored line ID. The optional line ID works with NPCs and names too. |
| `stop()` | Close the conversation and stop its voice; safe when already closed. |
| `is_active()` | Whether any conversation is open. |
| `get_flag("has_permit")` | Boolean; missing flags are false. |
| `set_flag("has_permit", true)` | Set a flag used by Advanced dialogue conditions. |
| `get_value("quest.stage")` | Integer; missing values are zero. |
| `set_value("quest.stage", 3)` | Set a value used by minimum-value conditions. |

Start, stop and setters return `true` on success or `false, error` on failure.
Getters return the value or `nil, error` for an invalid request. An active
conversation is never replaced by another start call: stop it first if intended.
An unknown entry line fails without starting. Duplicate names require the asset
ID instead. Entity references must belong to a live NPC in the current Level;
raw native entity numbers, Player Start markers and the runtime player are rejected.

Flags and numbers share dialogue's existing project-session memory. A flag is
stored as 0/1; `get_flag` interprets any nonzero stored value as true. Keys use
1-128 letters, digits, underscores, dashes, dots or colons. Numeric values must be
whole signed 32-bit integers. At most 16,384 distinct keys fit the saved state.
Changing a value refreshes available replies on the next Runtime update. It does
not replay a line, execute an outcome or automatically choose a different branch.
Quick-save/quick-load includes this memory; preview test flags remain isolated.

## Copyable NPC example

Attach this as an entity SCRIPT to an NPC with a saved conversation. This example
starts immediately when the Level opens. In a reusable trigger or quest script,
put the `start` call in that script's existing event handler instead.

```lua
local dialogue = renegade.dialogue

return {
    on_start = function(ctx)
        assert(dialogue.set_flag("has_permit", false))
        local ok, err = dialogue.start(ctx.entity)
        assert(ok, err)
    end,

    on_event = function(ctx, event)
        if event.name == "dialogue.finished" and
            renegade.entity.equals(ctx.entity, event.sender) then
            -- An authored branch reached its end. Continue your quest here.
            assert(dialogue.set_value("quest.stage", 2))
        elseif event.name == "dialogue.action" and event.payload == "open_gate:1" then
            -- Call your own gate script/API here.
            assert(dialogue.set_flag("gate.requested", true))
        end
    end
}
```

The basic guard example deliberately has fixed responses. For conditional entry,
enable Advanced on **Let me through**, add a `has_permit == 1` outcome pointing
to the approval line, and keep the final unconditional Otherwise pointing to
refusal. A quest script can then call `set_flag("has_permit", true)` when earned.
The first matching outcome wins. No scripting change is needed for each new line.

## Events and assets

| Event | When | Payload |
| --- | --- | --- |
| `dialogue.started` | A start call or NPC interaction succeeds. | Conversation asset ID. |
| `dialogue.finished` | A reply/Continue reaches the authored end. | Conversation asset ID. |
| `dialogue.closed` | Escape, script stop or an interruption closes an active conversation. | Conversation asset ID. |
| `dialogue.action` | An authored Event action is accepted. | Existing `key:value` string. |

Lifecycle events supply `event.sender` as the NPC EntityRef when started through
an NPC; name/ID starts have no NPC sender. Lifecycle events are broadcast; filter the
asset payload or NPC sender when a script should react to only one conversation.
Events use the existing bounded FIFO
and dispatch through `on_event`; they do not run callbacks reentrantly. Lifecycle
queue overflow is logged; no second unbounded event queue is created. A finished
conversation does not also emit closed. Repeated stop calls emit no extra event.

Save each dialogue asset before launching Test Game. Saving registers the asset
and local voice clips in the project's packaging roots, including conversations
used only by scripts. Script references use names/IDs, never machine file paths.
Runtime uses its launch snapshot; restart Test Game to use newly saved assets.
Dialogue flags are available only in gameplay Lua, not editor-preview execution.
Quest services, automatic conditional NPC continuation, graph editing and inline
Lua construction of dialogue documents are outside this beta API.
