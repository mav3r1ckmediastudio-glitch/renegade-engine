function(require_source path needle label)
    file(READ "${RENEGADE_SOURCE_DIR}/${path}" content)
    string(FIND "${content}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S6 source contract missing ${label}: ${needle} in ${path}")
    endif()
endfunction()

# S6A/S6B: immutable manifest identity + deterministic transitive closure.
require_source(
    "EngineBridge/include/renegade/bridge/ScriptLibraryService.h"
    "renegade-script-package"
    "package manifest schema")
require_source(
    "EngineBridge/src/ScriptLibraryService.cpp"
    "BuildClosure"
    "deterministic package closure")
require_source(
    "EngineBridge/src/ScriptLibraryService.cpp"
    "content_hash"
    "immutable file provenance")

# S6C/S6F: transactional project adoption and destructive-update refusal.
require_source(
    "EngineBridge/src/ScriptLibraryService.cpp"
    "ProjectDocumentTransaction transaction"
    "transactional adoption boundary")
require_source(
    "EngineBridge/src/ScriptLibraryService.cpp"
    "fs::path(\"Content\") / \"Scripts\" / \"Library\""
    "project-owned script namespace")
require_source(
    "EngineBridge/src/ScriptLibraryService.cpp"
    "have local edits"
    "creator modification conflict guard")
require_source(
    "EngineBridge/include/renegade/bridge/ScriptLibraryService.h"
    "library-lock.json"
    "persistent adoption baseline")

# S6D: the established SCRIPT/ACTION/GLOBAL SCRIPT source catalogue now merges
# Creator Library entries and first-use ADD materializes them before attaching.
require_source(
    "EngineBridge/src/ScriptAuthoringService.cpp"
    "library.EnumerateEntries"
    "Creator Library enumeration")
require_source(
    "EngineBridge/src/ScriptAuthoringService.cpp"
    "MaterializeLibrarySource"
    "first-use adoption from existing Inspector flow")
require_source(
    "EngineBridge/src/ScriptAuthoringService.cpp"
    "MakeReplaceScriptSourceCommand"
    "Undo-aware package binding refresh")

# S6E: Test Level already snapshots the project-owned Content/Scripts tree. Build
# Game receives the same companion/source/module closure through normal project
# Always Include roots; the Runtime keeps governed require() and no package/io/os.
require_source(
    "EngineBridge/src/TestLevelSnapshotService.cpp"
    "CopyCreatorScriptTree"
    "Test Level script-tree parity")
require_source(
    "EngineBridge/src/ScriptAuthoringService.cpp"
    "generated_data:"
    "Build Game rscripts companion root")
require_source(
    "EngineBridge/src/ScriptAuthoringService.cpp"
    "script:"
    "Build Game script/module roots")
require_source(
    "EngineBridge/src/WindowsGameBuildProjectService.cpp"
    "ProjectDependencyProvider projectProvider"
    "Build Game project dependency authority")
require_source(
    "Runtime/src/RuntimeScriptRuntime.cpp"
    "RemoveGlobal(\"package\")"
    "Runtime package-library denial")
require_source(
    "Runtime/src/RuntimeScriptRuntime.cpp"
    "ScriptDependencyKind::ScriptModule"
    "governed declared-module require")

message(STATUS "S6 script library/adoption/package-closure source contract passed")
