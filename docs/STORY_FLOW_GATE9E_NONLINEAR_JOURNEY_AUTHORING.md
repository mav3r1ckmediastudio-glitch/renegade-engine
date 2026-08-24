# Story Flow Gate 9E — Advanced Nonlinear Journey Authoring

## Scope

Gate 9E makes nonlinear topology authorable from the accepted Journey surface.
It does not redesign Journey, replace Graph, or add decorative controls.

The authoritative boundary remains `StoryFlowAuthoringSession`:

- **Add Action** creates one real route from the selected non-terminal card;
- Screen routes use an unused action from the governed Runtime Screen action list;
- each Inspector destination change updates the same stable route ID;
- branches, merges, loops, hubs and returns project as exits between existing
  cards—semantic nodes are never duplicated;
- Journey and Graph rebuild from the same Flow document and command history;
- Undo, Redo, Save and reopen preserve the exact topology;
- branch lanes may collapse or expand without mutating Runtime semantics.

Unsupported mutations fail closed. Terminal cards cannot add actions, Game
Start cannot gain a second entry route, and a Screen cannot route an action that
does not exist in its governed action list.

## Automated acceptance

`RenegadeStoryFlowGate9ENonlinearAuthoringTests` proves the functional gate:

1. add an alternate route from a hub;
2. merge the alternate route into an existing main-route destination;
3. rewire an existing route into a loop without changing its stable ID;
4. prove Journey contains one card per semantic node and the exact same route
   IDs as the shared authoring/Graph model;
5. Undo and Redo the rewire through the shared command history;
6. Save, reopen and rebuild the same branch/merge/loop topology.

The Journey recovery source contract additionally requires a real, visible and
honestly enabled **+ ADD ACTION** button, governed route creation, stable-ID
rewiring and Journey-only Inspector routing.

## Packaged Release owner acceptance

Use the packaged Release artifact from the exact passing PR head.

1. Open Story Flow in **Journey** and select a non-terminal card.
2. Confirm **+ ADD ACTION** is visible beside **ACTIONS / EXITS**. Terminal cards
   may show it disabled, never deceptively active.
3. Add an action. Confirm a real exit row appears immediately and its initial
   destination can be changed in the Inspector.
4. Route the action to a new alternate destination. Confirm the card appears in
   a colour-coded branch row; main cards retain neutral borders.
5. Route that branch back to an existing main card. Confirm the existing card is
   reused rather than duplicated.
6. Route an exit back to an earlier card. Confirm the loop is represented
   without wires and without duplicating cards.
7. Collapse and expand each alternate branch row. Confirm only presentation
   changes and the Inspector remains usable.
8. Undo and Redo the last route change from the top toolbar. Confirm Journey and
   Inspector update together.
9. Switch to **Graph**. Confirm it shows the same branch, merge and loop. Switch
   back to Journey and confirm no topology changes.
10. Save, close and reopen the project. Confirm the same routes, branch rows and
    destinations return.

Gate 9E is complete only when exact-head Debug and Release CI are green and this
owner test passes. The draft pull request must not be merged without explicit
owner authorization.

## Deliberate boundary

Presentation-only chapters/groups, automatic insertion/splicing, the supplied
brand-logo replacement and the remaining concept-fidelity pass are not claimed
by 9E. They remain 9F/pre-release work and must not be faked by altering Runtime
semantics or layering another visual shell over Journey.
