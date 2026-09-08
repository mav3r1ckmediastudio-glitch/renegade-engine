# S7 Stock Actions — Creator Setup and Owner Test

S7 ships six representative Lua Actions. They work with ordinary placed scene
objects; special door, switch, pickup, relay, or trigger meshes are not
required. A Player Start is required for player proximity and **E / Interact**
tests, and Play Sound requires a Renegade 3D Sound Source with an audio asset.

## Attach an Action

1. Open a Renegade project and save the Level once.
2. Select the object that should own the behavior.
3. Expand **ACTION** in the Scene Inspector.
4. Choose a stock Action, then press **ADD**. If the imported object predates
   Renegade identity, ADD assigns the missing stable identity through normal
   Undo/Redo automatically.
5. Set the exposed properties, save, and press **PLAY** to launch Test Level.

The picker must list six entries. **REFRESH** rescans the project and installed
library; it does not attach anything. The first ADD copies the selected stock
package into the project's governed `Content/Scripts` area, so Test Level and
Build Game execute the project-owned copy.

## Fast smoke test: one generic cube door

1. Add or import any visible object and place it close to Player Start.
2. Attach **Interaction / Sliding Door** to that object.
3. Keep **Direct Interaction** enabled. Set **Open Offset** to a clearly visible
   value such as `(0, 2, 0)` and keep **Interaction Distance** at `2` or higher.
4. Run Test Level and walk close to it.
5. Confirm **Press E to open / close** appears, then press **E**. The object must
   slide to the offset. Press **E** again to close it.

No separate Switch is needed for this smoke test. To test event wiring instead,
disable Direct Interaction on the door, attach **Interaction Switch** to a
second object, set its **Target** to the door, and set **Event** to `toggle`.

## What each Action does

| Action | Minimal setup | Expected Runtime behavior |
|---|---|---|
| Sliding Door | Any transformed object near Player Start | Shows its prompt and toggles with **E** by default; also receives `open`, `close`, and `toggle` events. Optional Auto Close starts when opened. |
| Interaction Switch | Any transformed object; optionally set Target | Shows its prompt in Use Distance and sends Event when **E** is pressed. With no Target it broadcasts. |
| Player Trigger Zone | Any transformed object; set Radius and optional Target | Sends Enter Event when the player crosses into its spherical radius and Exit Event when leaving. It is automatic and does not use **E**. |
| Proximity Pickup | Any transformed object near Player Start | Collects automatically in Pickup Radius. Enable Require Interact to show its prompt and require **E**. Hide On Pickup scales it to zero for the play session. |
| Activation Relay | Any transformed object; set Input/Output Event and optional Target | Forwards matching events immediately or after Delay. It is event-driven and does not use **E** directly. |
| Play Sound | Attach anywhere; set Sound Source to a Renegade 3D Sound Source | Plays or stops the selected source when it receives Play Event or Stop Event. Play On Start is the fastest isolated test. |

## Acceptance checks

- Close and reopen the saved Level: attachments, order, enabled state, and
  properties remain intact.
- Undo immediately after adding to an unidentified imported object: the Action
  attachment is removed first; a second Undo removes the generated identity.
  Redo restores both.
- Test Level and a packaged Build Game show the same prompt and behavior.
- The packaged Studio still lists all six stock Actions after using Open/Save
  dialogs; library discovery must not depend on the process working directory.
- A broken Action produces an isolated scripting diagnostic and does not stop
  other attached Actions.

