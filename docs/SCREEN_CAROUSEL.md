# Screen carousel

Add **Elements > Basic > Carousel** in the Screen Designer. Drag it onto the canvas or click the palette entry to create one.

Select the Carousel layer and choose **Edit carousel...**. This opens an in-editor Studio modal using the shared Renegade window, outline buttons, numeric field and checkbox. Its content scrolls when space is limited while Save and Cancel stay in the footer:

- Add or replace images using the file picker. External images use the existing governed texture importer and get a runtime image under project Content.
- Each image has its own optional caption. Enter text in the caption box; line breaks, Unicode, selection, clipboard and local text Undo/Redo are supported. Empty or whitespace-only captions draw nothing.
- Select an image, or use Previous and Next, to review its picture and caption.
- Move up and Move down reorder the complete image/caption pair.
- Set Seconds per image and optionally enable Shuffle images. Save applies the complete edit as one Screen Undo/Redo operation; Cancel discards the entry edits. Imported image assets remain available in the project.

A **Carousel caption** Text child is created with the carousel. Select that layer to change font, colour, alignment, size and position using the normal text inspector. Its initial layout spans the carousel width just below the image and follows resizing. Caption content comes from the selected image's entry.

Use **Preview** to see automatic playback. Studio Preview and Runtime use the same renderer. Images and captions advance together; shuffled playback visits every entry before reshuffling and avoids repeating the last image at the cycle boundary. Hidden presentations stop advancing.

Save and reopen the Screen normally. Copy/paste and deletion operate on the carousel and its child through the existing Screen hierarchy commands.

## Data and resource ownership

This is an optional addition to Screen schema 3. A carousel is an Image widget with a bounded ordered list of stable entry IDs, project-relative image paths, captions, playback duration and shuffle flag. Captions use hexadecimal UTF-8 storage so configuration punctuation, whitespace and line breaks round-trip. The child Text widget records its carousel-caption role and retains ordinary authored text styling and geometry.

A carousel supports up to 256 entries, a shared duration of 0.5 to 3600 seconds per image and captions up to 16384 UTF-8 bytes. Existing screens without carousel fields retain their behaviour. External image imports retain the existing texture workflow's limits and supported formats.

The Screen dependency reader includes every slide image. Portable UI adoption remaps slide resource paths and creates new slide identities. Rendering retains loaded image resource handles and changes the image and caption at the same playback boundary.
