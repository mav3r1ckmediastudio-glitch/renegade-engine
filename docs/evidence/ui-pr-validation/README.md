# Combined UI pre-PR evidence

Application implementation: `7fd95b5b3607974e7275673fa0bcc3e1b1c6af2a`; configure stamp `c2eccc33fa1c2e13c06bc50b94f16ffa308d12a5`. See [validation receipt](../../UI_COMBINED_PR_VALIDATION_20261117.md).

Earlier complete Debug/Release CI aggregate output, final incremental Studio/recovery build output, rerun unfiltered tests, focused source/help/policy output and machine-readable binary provenance accompany selected native captures. Paths are normalized; log whitespace is expanded/trimmed for text review.

| Native receipt | Evidence |
|---|---|
| Literal-ellipsis title, before / corrected / Undo | [Before](before-heading-stale.png), [corrected](after-heading-correct.png), [Undo](after-heading-undo.png) |
| Story Flow spacing and distinct commands | [Story Flow](final-storyflow.png), [Test Game help](final-testgame-help.png), [Build Game help](final-build-help.png) |
| Real Runtime launch and Stop | [Runtime ready](final-runtime-ready.png), [Stop help](final-stop-help.png), [stopped](final-testgame-stopped.png) |
| Immediate selection and Audio routing | [Light](final-light-immediate.png), [Audio](final-audio-inspector.png), [same light](final-audio-to-same-light.png) |
| All three viewport modes | [Menu](final-show-menu.png), [wireframe](final-wireframe.png), [overlay](final-overlay.png), [shaded restored](final-shaded-restored.png) |
| Modal layering, preview and narrow footer | [Light modal over overlay](final-light-modal-overlay.png), [1024 width](final-light-modal-1024.png), [closed](final-modal-closed-overlay.png) |
| Numeric inputs and selection scroll | [Environment](final-environment-numeric.png), [Render](final-render-numeric.png), [Player lower inputs](final-player-lower-numeric.png), [model reset](final-model-scroll-reset.png) |
| Consistent Add and Dialogue/Screen | [Add](final-add-menu.png), [Dialogue](final-dialogue.png), [Screen Editor](final-screen-editor.png) |
| Final B5AA Screen tooltip correction | [Full Fit View hint](verified-fit-help.png), [Save autosave guidance](verified-save-help.png), [clean scene](verified-clean-scene.png) |
| Final B5AA Audio handoff | [Audio](verified-audio.png), [selected light inspector](verified-audio-handoff.png) |

Initial exFAT regeneration and interrupted local attempts are excluded from successful receipts. Hosted CI status belongs to the PR.
