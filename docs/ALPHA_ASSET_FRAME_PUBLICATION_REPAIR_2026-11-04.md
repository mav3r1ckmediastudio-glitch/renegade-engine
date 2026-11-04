# Asset publication frame-boundary repair — 2026-11-04

Actual packaged Studio testing blocked revision 04d8531 after textured OBJ placement on maximum terrain. Windows recorded access violation c0000005 at RVA 0x97dd8b. A diagnostic link of the same objects produced byte-identical .text and resolved the fault to Scene::ComputeObjectLODForView+0xb, called by DrawShadowmaps on a native worker.

Native ObjectComponent::mesh_index is valid for one frame and is assigned by Scene::Update. Creator asset placement and preview instantiation previously inserted objects after that update, exposing uninitialized or prefab-local mesh indices to shadow rendering.

Studio now captures placement input and publishes new model/player-prefab objects and drag previews at EVENT_THREAD_SAFE_POINT, before the following native scene update. Preview input edges and view data are copied. Session, project, scene revision and cancellation tickets guard delayed work. Completed placement is consumed once by the regular editor update. Undo/Redo retains its existing frame-safe route. Native shadows and LOD remain enabled, and the Wicked dependency pin is unchanged.

The shared production queue has a GPU-hosted regression for deferred, repeated and cancelled publication, native mesh-index identity and the exact shadow-view LOD lookup. Actual Studio replay of ordinary placement, drag/cancel and history is required before release acceptance. A compile pass alone does not clear the earlier release block.

Maximum terrain testing also observed long synchronous editor operations on DESIGN with its existing debugging configuration: expansion Undo about67s, Redo about75s, sculpt capture/commit roughly3minutes. These are material performance observations and must accompany owner download evidence; responsive maximum-size sculpting is not established.


## Cancelled release recovery

Actual maximum-terrain GUI testing found that Escape removed the live preview
but Chrome still delivered its mouse-up through the fallback drop route, creating
an unwanted fifth instance despite the CANCELLED status. A shared gesture gate
now suppresses both fallback intake and pending-drop processing until that
cancelled gesture is consumed. A fresh explicit press rearms placement.
Player-prefab fallback uses the same cancellation check. The production gate is
covered by the native frame-boundary regression; actual Escape/RMB, pending
preparation, fresh subsequent placement and saved-scene counts remain release
acceptance requirements.
