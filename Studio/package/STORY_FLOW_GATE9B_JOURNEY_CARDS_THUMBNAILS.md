# Story Flow Gate 9B — Owner Release Check

Use this packaged Release build only after the exact PR head is green in both authoritative CI workflows.

Verify the Journey surface now uses native Renegade lane/card objects, not the old primitive giant-card presentation and not concept artwork used as a background.

For a governed Level card, click the small `IMG` control. The image picker must open without opening the Level or starting a drag. Choose a JPG/JPEG/PNG/BMP/TGA image and confirm it appears in the card. Replace it and confirm the image refreshes. Then open the Level, return to Story Flow, close/reopen the project, and confirm the same thumbnail remains associated with that Level.

Also verify Level and Screen double-click opening, card selection/dragging, Journey/Graph switching, Save/Undo/Redo, lifecycle controls and existing Screen action/Story Flow routing behavior.

Gate 9B fails on any crash, input overlap, broken Level/Screen activation, lost thumbnail on reopen, absolute machine-path dependency, concept-art background implementation, or Story Flow semantic/runtime regression.
