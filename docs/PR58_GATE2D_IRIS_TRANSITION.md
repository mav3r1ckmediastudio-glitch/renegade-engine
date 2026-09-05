# PR #58 — Gate 2D Iris Transition

Status: **IMPLEMENTED / CI + OWNER ACCEPTANCE PENDING**

Gate 2D replaces Gate 2C's temporary black ENTER HUB handoff with the approved mechanical iris reveal. It does not redesign the Project Hub and does not alter Gate 2A, Gate 2B, Gate 2C identity persistence, Gate 7 performance work, importer behavior, project lifecycle semantics, or governed asset behavior.

## Accepted visual contract

When the user clicks **ENTER HUB**:

1. Renegade renders the actual live Studio/Project Hub frame while the accepted Gate 2C surface still fully covers the client area.
2. A Renegade-owned Gate 2D overlay takes ownership of the accepted final handshake frame before Gate 2C is removed, preventing Hub/editor flash.
3. The surrounding handshake HUD fades away quickly.
4. Only the central mechanical iris remains visually solid.
5. The iris splits on its vertical centerline into two clipped half-irises.
6. The left half travels left and the right half travels right, revealing the actual live Hub underneath.
7. The iris halves soften only near the end of travel and disappear off-screen.
8. The transition layer is destroyed and control remains with the real live Hub.

This is a deterministic code-driven transition. It does **not** use another generated full-screen video and it does not reveal a screenshot pretending to be the Hub.

## Implementation

New isolated component:

- `Studio/src/StartupIrisTransition.h`

Integration points:

- `Studio/src/main_Windows.cpp`
- `Studio/CMakeLists.txt`

The transition reuses the accepted Gate 2C final bitmap already required by the owner runtime package:

- `Content/startup/renegade_identity_handshake_final.bmp`

No additional binary visual asset is required for Gate 2D.

### Layering

The Gate 2D component creates:

- one full-client layered overlay containing the accepted final handshake plate;
- two independent layered popup surfaces containing ellipse-clipped left and right halves of the central iris.

The full-client plate fades over roughly 280 ms after a short ~70 ms mechanical hold. The iris halves travel for roughly 920 ms using a smoothstep curve. They remain opaque through most of the travel and fade only in the final portion.

The iris crop is normalized from the accepted 1280×720 Gate 2C final still, approximately centered at `(640,315)` with an outer radius of `300 px`, so the transition continues to scale correctly when the Renegade window is resized or maximized.

### Failure behavior

Gate 2D is cosmetic and must never strand startup. If the final bitmap cannot be loaded or the transition windows cannot be created, Renegade fails open directly to the already-rendered live Hub.

Dedicated diagnostics:

- `Saved/Diagnostics/PR58Gate2DStartup.log`

Expected lifecycle markers include:

- `PROCESS_START`
- `ENTER_HUB_TRANSITION_REQUESTED`
- `LIVE_HUB_FRAME_READY_BEHIND_HANDSHAKE`
- `IRIS_SPLIT_ACTIVE`
- `IRIS_SPLIT_FINISHED`
- `HANDOFF_TO_LIVE_HUB`
- `PROCESS_EXIT`

Failure path:

- `IRIS_SPLIT_CREATE_FAILED // fail_open=1`

## Acceptance checklist

Gate 2D is not passed until all of the following are owner-confirmed on an exact-head Release artifact:

- Gate 2A reveal still works.
- Gate 2B saved identity and first-run reset path still work.
- Gate 2C handshake and ENTER HUB still work.
- Clicking ENTER HUB does not expose a black intermediate handoff.
- The actual Hub is visible underneath the opening transition.
- The surrounding handshake HUD fades rather than splitting as two full-screen rectangles.
- The circular iris splits vertically into recognisable left/right mechanical halves.
- Left/right halves travel smoothly away from the center.
- No Hub/editor flash occurs before ENTER HUB.
- No crash, hang, stuck overlay, or lost window controls occurs.
- Maximize/resize remains viable.
- Direct EXE startup works.
- DX12 launcher works.
- Vulkan launcher works.

Gate 2D must remain acceptance-pending until both exact-head CI and owner runtime validation are complete.
