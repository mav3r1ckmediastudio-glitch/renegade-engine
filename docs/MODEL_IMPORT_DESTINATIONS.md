# Model import destinations and player roles

IMPORT MODEL now separates folder organisation from player classification.
Choose an existing Content folder from Folder, or type a new project-relative
folder in Import to (for example Content/Equipment/Medieval). Import creates new
folders. The selected Asset Browser folder is the default when it is a valid
Content path; otherwise the default remains Content/Models. Paths outside
Content, traversal and symbolic-link folders are rejected.

Asset role is General model, Player arms or Weapon. Player arms requires a
skinned model with bones. Assembly recognises explicitly marked arms/weapons in
any Content folder. Legacy Content/Player/Arms and Content/Player/Weapons and
existing assembly recipe roles remain recognised. A Weapon role does not convert
a static mesh into an animated weapon, or create an equipment definition.
The role is stored as a reserved player-role:arms or player-role:weapon catalogue
tag keyed by stable asset ID. Free-text import tags cannot set reserved roles.
Optional comma-separated Tags are searchable labels; they never route files.

The existing governed import transaction writes the model, projection, thumbnail,
registry and catalogue metadata together. Original sources, texture bundles and
external animations remain retained under SourceAssets/Models. Imported names
still protect retained source collisions even across different Content folders.

To reorganise an existing registered model, select its Asset Browser card and
click MOVE. Choose an existing folder or type a new Content folder, then MOVE
ASSET. The model product, managed projection and optional thumbnail move through
one journaled transaction with rollback/recovery; collisions are rejected.
Stable IDs, source bundles, creator tags and dependency edges remain unchanged.
Existing assembly references and stable-ID placed instances therefore retain
their bindings. This action is limited to reusable model .rasset products;
textures, equipment, prefabs and arbitrary files are not generalised by this work.
Disk moves are asset operations and do not enter scene Undo/Redo; move back using
the same action. New folders may remain empty after a rejected transaction.

Owner usability review and independent exact-commit verification remain required.
