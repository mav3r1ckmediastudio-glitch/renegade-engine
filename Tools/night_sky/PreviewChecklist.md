# Night Sky visual regression check

Use only the isolated Night Sky build. Do not edit the active UI Designer project or open the same project in two editors.

1. In Downloads > **Renegade Night Sky Preview**, double-click **Open Night Sky Studio.cmd**. The guarded launcher must print successful ABI/scene checks before Studio starts.
2. **Open Project** and select `Renegade Night Sky Visual Test\Renegade Night Sky Visual Test.renegade`. This is a real Renegade project, **not** Add Existing Level or File > Open for a loose `.wiscene`. Open its starter scene in the Level Editor.
3. **First regression gate:** In daylight (Sun midday), check the sky is blue and not black. At midnight, check the existing native **Stars** slider shows stars and twinkling. If either fails, stop and report it; do not continue tuning aurora.
4. Select **ENVIRONMENT** in top chrome. Confirm there is no Night Sky tab. Toggle every section heading; children must disappear and the remaining headings/actions reflow without empty rows. Reopen Environment to check session disclosure retention. Expand **NIGHT ATMOSPHERE** and check **Brightness** from 0 to ~0.8, expecting nearly black zenith transitioning smoothly to desaturated indigo/violet horizon.
5. Toggle Aurora Enabled. Try intensity 0.4–0.8, coverage 0.4–0.8 and speed 0.2–0.7. The experimental curtains are centered near the +Z horizon; pan the camera.
6. Check dusk/dawn fading, realistic clouds/fog, skybox controls disabled, Undo/Redo and save/reopen. Test a legacy scene **copy** with no Night Sky metadata to confirm no changes.
7. Measure GPU frame-time at 1080p and 1440p with aurora on and off.

**Acceptance remains pending:** compilation and CPU state tests cannot establish onscreen sky or UI correctness. Consult HANDOFF.md for the latest exact build and blocker evidence.
