# Packaged GPU handoff and Terrain workspace target repair

The packaged acceptance run found two failures after the terrain collision repair.

On Windows, the endpoint provider returned real loopback listeners with State=0.
A State=Listen query therefore missed a healthy, GPU-resident local model.
The shared port guard now enumerates bound endpoints without filtering their
reported state, verifies executable and required arguments against the package,
and probes the loopback port before declaring it free. An unknown or foreign
owner fails closed. GPU suspension still verifies process creation identity
before stopping only the owned model; its lease watcher restores that model.
Voice, orb, knowledge and conversation gateway processes remain running.

Portable startup uses the same guard and verifies the ownership of healthy
knowledge and conversation services before reuse. A service from another
extracted package cannot silently satisfy a new package's startup checks.

The Terrain workspace already displays its terrain while Player Start or a
river point is selected. Expansion and terrain sliders incorrectly used that
unrelated selection as their authoring target. Display and authoring now share
the same target resolver, preserving selection and the Player Start preview.

Validation includes unknown-state endpoint fixtures, a real disposable Windows
TCP listener, rejected executable/argument mismatches, and terrain target
resolution with Player Start, river, empty selection and no terrain. Actual
packaged GPU suspend/restore, terrain controls with Player Start selected,
maximum terrain, gameplay and archive acceptance remain required release gates.
No upstream source or pin changes are involved.
