# Story Flow Gate 8D — Creator Controls

## Outcome

Gate 8D turns the Gate 8C Screen Editor shell into the creator-facing Screen authoring surface. The Screen document remains the only semantic source of truth and the shared Gate 8B renderer remains the only Runtime/editor presentation path.

Gate 8D does **not** create a Gate 8F and does not redesign Story Flow. Gate 8E remains the Story Flow outcome + packaged standalone parity closeout; the recovered Journey View UI/UX programme remains Gate 9.

## Locked Gate 8D boundary

Gate 8D owns all of the following before it can be called complete:

- create and delete Screen elements;
- explicit Text, Button, Image and Background creator affordances;
- duplicate and authored back/front layer ordering;
- image/resource selection through the governed project resource seam;
- the broader creator control catalogue supported by the Screen contract;
- reusable creator components without inventing a second Screen document format;
- complete authored visual-state/style editing for normal, hover, pressed, focused and disabled states;
- editable typography/font resource identity rather than developer-hardcoded fonts;
- typed action/data binding with validation against the governed Screen contract;
- the same Screen-specific Undo/Redo, transactional Save/Open and dirty-return guarantees established in Gate 8C.

Every user-facing Screen property must remain serialized and editable. Runtime styling, fonts, actions or layout defaults must never silently override authored values.

## Rebuilt creator-transaction foundation

The original Codex scratch preparation was lost before it reached GitHub. This branch reconstructs that bounded foundation from the final owner-accepted Gate 8C merge rather than from the older pre-closeout 8C head.

The reconstructed foundation adds:

- visible **+ TEXT**, **+ BUTTON**, **+ IMAGE** and **+ BACKGROUND** creation controls;
- Duplicate, Delete, Back and Front creator operations;
- validated `ScreenAuthoringSession` transactions for create, duplicate, delete and layer ordering;
- stable-ID generation for newly-created and duplicated elements;
- Button action preservation and exact focus-order maintenance;
- full-document validation before any creator mutation enters history;
- creator operations in the existing bounded Screen Undo/Redo history;
- transactional Save/Open persistence through the existing Screen document writer;
- Image semantics that permit an unassigned resource while the creator is authoring and permit Images to be deliberately hidden;
- no parallel Runtime/editor representation.

This is the **foundation of Gate 8D**, not permission to move unfinished 8D scope into 8E.

## Creator transaction invariants

1. A creator operation constructs a complete candidate `ScreenDocument`.
2. The candidate must pass `ValidateScreenDocument` before it enters history.
3. Newly-created elements receive a valid stable widget ID.
4. Duplicates receive a new stable ID while preserving authored content, layout, style, resource identity and Button action binding.
5. New Buttons enter focus order exactly once; duplicate Buttons are inserted immediately after their source focus target; deleted Buttons are removed from focus order.
6. Layer operations reorder the authoritative `widgets` collection because the shared renderer already consumes that collection as authored back-to-front order.
7. Deleting a parent with child elements is refused rather than silently orphaning descendants.
8. A mutation that would leave the Screen invalid — for example deleting the final required action Button — is refused by full-document validation.
9. Image `visible` is authored state. Validation must never force it true.
10. An Image may be created before a resource is chosen. An empty resource path means unassigned, while any non-empty resource remains governed and project-contained.

## Automated proof for this foundation

`RenegadeScreenAuthoringSessionTests` extends the accepted Gate 8C proof to cover:

- Text creation with a generated stable ID;
- Button creation while retaining authored action identity and focus order;
- Button duplication with a distinct stable ID and adjacent focus target;
- back/front layer transactions;
- hidden, currently-unassigned Image creation;
- deletion with focus-order cleanup;
- Undo/Redo across creator transactions;
- transactional Save/Open preserving created elements, action identity, focus order and Image visibility.

Windows Debug and Release CI remain authoritative for compilation and regression proof.

## Owner-visible acceptance for the creator-transaction foundation

In the packaged Release Screen Editor:

1. Open an existing governed Screen from Story Flow.
2. Confirm clearly-labelled creator controls are visible for Text, Button, Image and Background plus Duplicate, Delete, Back and Front.
3. Create Text and Button elements and confirm each appears immediately in both hierarchy and the exact shared Runtime preview.
4. Duplicate the Button and confirm the duplicate is independently selectable/editable.
5. Send an element to Back and Front and confirm visible overlap ordering changes accordingly.
6. Create an Image/Background before assigning a resource and confirm the editor remains valid rather than rejecting the element.
7. Toggle an Image invisible and confirm the authored state is accepted.
8. Undo and Redo creator operations and confirm hierarchy, preview and selection return to the corresponding validated snapshots.
9. Save, return to Story Flow, reopen the Screen and confirm created elements and layer state persist.

The remaining Gate 8D resource/style/component/binding work must receive its own owner-visible proof before Gate 8D is accepted as a whole.
