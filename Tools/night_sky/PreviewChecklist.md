# Night Sky final manual regression checklist

The owner accepted all three effects on 2026-10-16. See docs/NIGHT_SKY_OWNER_ACCEPTANCE.md. This does not close the remaining checks below.

Use the isolated preview launcher and disposable preview project; leave the active UI Designer project untouched. Do not open one project in two editors.

1. Verify normal blue/scattering daylight and native stars at midnight before extension checks.
2. In Environment, expand/collapse all nine headings. Confirm no separate Night Sky tab, no lost native controls and no empty rows between collapsed headings.
3. Check Night Atmosphere brightness, zenith/horizon colour wheel, Apply/Cancel and horizon glow.
4. Check aurora enable, intensity, coverage around the sides/rear and higher overhead, and animation speed. Check dusk/dawn fading.
5. Under Stars, check Milky Way intensity independently: zero/off, Milky Way alone, aurora alone, both, and native Stars at zero/nonzero.
6. Perform manual editor Undo/Redo, Save, close and Reopen for each new control; confirm values and rendering persist. The final Milky Way pass remains outstanding despite passing native command/disk tests.
7. Open a legacy scene copy with no night metadata. Confirm extensions remain off and native sky/stars/exposure are preserved. Check texture-skybox controls disable correctly.
8. Check clouds, fog and reflections in representative scenes; measure on/off GPU time at 1080p and 1440p.
9. Inspect standalone packaged Runtime output. Run PR CI and independent exact-head review before any release-gate closure.
