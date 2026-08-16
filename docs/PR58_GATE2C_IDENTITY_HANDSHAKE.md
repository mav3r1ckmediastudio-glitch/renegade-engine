# PR #58 Gate 2C — Identity Handshake

Status: **IMPLEMENTED — OWNER ACCEPTANCE PENDING**

Gate 2C sits between the accepted Gate 2B identity preference and the existing Studio/Project Hub handoff. It does not redesign the Project Hub, implement the Gate 2D iris split, optimize startup, or add reveal skipping.

## Accepted input authority

The owner-selected final Gemini motion plate is the visual source for the Gate 2C handshake. Renegade does **not** trust generated text in that plate as application state.

The owner-test motion plate is a deterministic post-processed derivative of the owner-selected clip. Like Gate 2A media during acceptance, it is injected into the runnable Release package rather than required for CI source checkout:

- runtime path: `Content/startup/renegade_identity_handshake_v1.mp4`
- optional source-tree packaging path: `Studio/assets/startup/renegade_identity_handshake_v1.mp4`
- H.264 video, 1280×720, 24 fps
- AAC stereo audio, 48 kHz
- duration: approximately 10.005 seconds
- SHA-256: `3d7742a0d357b23ed694a9345b61f7a91eb00f919068a2f8f9234edd49635e0d`

The only post-processing applied to the selected owner clip is a deterministic correction of the upper-left protocol subtitle to `SECURE ID HANDSHAKE PROTOCOL`. The mechanical iris, rotating biometric head, scan animation, timing, camera and audio remain the selected motion plate.

## Deterministic final state

A still frame is extracted from the same accepted motion plate at approximately 9.5 seconds and packaged beside it:

- runtime path: `Content/startup/renegade_identity_handshake_final.bmp`
- optional source-tree packaging path: `Studio/assets/startup/renegade_identity_handshake_final.bmp`
- 1280×720 24-bit bitmap
- SHA-256: `e78040de783190baad2d88d33b41e4ee3406cc42018f20a32460b1a210ad6aea`

After Media Foundation finishes the motion plate, Renegade replaces the generated playback surface with this exact still and renders all authoritative identity UI itself:

- `WELCOME, <saved developer identity>`
- `RENEGADE // IDENTITY ACCEPTED`
- a real `ENTER HUB` control

The bottom-center generated progress/status area is covered by a Renegade-owned panel before the native identity text is drawn. The generated plate therefore remains visual atmosphere only; it cannot supply the developer name or accepted-state authority.

## Startup state flow

### First run

`Gate 2A reveal → Gate 2B name prompt → Gate 2C motion plate → native final identity screen → ENTER HUB click → Studio`

The completed Gate 2B prompt remains opaque until Gate 2C has established a video surface or deterministic final-screen fallback, preventing an editor/Hub flash between states.

### Subsequent run

`Gate 2A reveal → saved identity loaded → Gate 2C motion plate → native final identity screen → ENTER HUB click → Studio`

Gate 2B remains skipped exactly as accepted when a valid saved identity exists.

## Failure behavior

Gate 2C must never strand startup.

- If the motion plate is unavailable or playback fails, Renegade can still show the final native identity screen using the packaged final bitmap.
- If the bitmap cannot be loaded, the final identity screen falls back to a dark Renegade-owned background while preserving the saved identity and `ENTER HUB` interaction.
- If the final identity window itself cannot be created, startup fails open to Studio using the existing black/opaque handoff discipline.

Diagnostics are written to:

`Saved/Diagnostics/PR58Gate2CStartup.log`

The log records media presence/size, playback start/failure/completion, final identity UI creation, `ENTER HUB` acceptance, first Studio frame readiness behind the handshake, and final Studio handoff.

## Gate boundary

Gate 2C intentionally ends with a simple black-covered handoff after `ENTER HUB` is clicked. Gate 2D will replace that handoff with the approved vertical iris split/reveal while keeping the same final identity state and live Project Hub underneath.

## Owner acceptance checklist

Gate 2C is not closed until the owner confirms a Release build on the exact implementation head:

1. Existing Gate 2A reveal still plays correctly with audio.
2. First-run Gate 2B entry still works and persists the developer identity.
3. Subsequent launch still skips Gate 2B.
4. Gate 2C motion plate plays after identity is available.
5. The biometric head visibly rotates and the iris/scan animation remains intact.
6. The final displayed developer name exactly matches the saved Gate 2B identity.
7. `RENEGADE // IDENTITY ACCEPTED` is crisp native text.
8. `ENTER HUB` is a real click target and no automatic timeout enters Studio.
9. Clicking elsewhere does not enter the Hub.
10. Clicking `ENTER HUB` produces a clean black handoff into the existing Studio/Hub with no editor flash before acceptance.
11. Window maximize/restore/resize remains functional through the startup sequence.
12. Direct EXE, DX12 launcher and Vulkan launcher remain viable.

**Do not mark Gate 2C PASSED until owner runtime acceptance is complete.**
