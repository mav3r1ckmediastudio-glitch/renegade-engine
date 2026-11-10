# Shared allocator registry correction, 8 November 2026

The brazier FBX transaction committed its managed files, then Studio terminated
while deleting ModelImportPreview. The captured stack reaches the DX12
AllocationHandler destructor through a shared pointer that should own a Scene.
The installed executable has no PDB. Relinking the existing object response
file with a map produced an identical .text section, allowing exact RVA mapping.

The dump has all 256 shared allocator slots populated while its uint8 counter
contains 17: registration has wrapped. The pinned header declares its variable
template inline static, creating separate allocators per translation unit.
A 3-translation-unit regression referencing 100 identical types reproduces 300
registrations and a wrapped count of 44. Registry entries are overwritten,
so pointer cleanup can invoke a destructor belonging to a different type.

## Bounded core-patch decision
Remove only static from the shared_block_allocator variable template in
WickedEngine/WickedEngine/wiAllocator.h. C++17 inline external linkage supplies
one allocator per specialization across translation units. Keep the pinned
submodule commit, pointer representation, refcounts, pool size and serialization.
The configured overlay is owned by cmake/WickedSharedAllocatorFix.cmake and
checks the expected declaration. A bridge-only adapter cannot fix allocations
already created inside Wicked; quoted header includes prevent a header-search
override. No renderer, shader or asset format changes are required.

## Protection and compatibility
Tests/SharedAllocatorRegistry builds 100 types in three separate translation
units, requires one shared address per type and exactly 100 registrations,
then checks copying, cross-unit release and exactly-once destruction.
All affected engine and application translation units must rebuild together.
Mixing pre-fix and post-fix object files is unsupported.
The actual Studio import, cancel/repeat preview, reload and Test Game must pass
before replacing the owner's private editor. Native solution builds must apply
the same header correction; CMake applies it during configuration.
Future conflicts are localized to allocator declaration/registry changes.
This correction is suitable for upstream review after independent validation.
Current diagnostic evidence is a local candidate, not a release gate closure.
