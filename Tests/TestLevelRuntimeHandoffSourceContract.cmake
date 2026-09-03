set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")

foreach(required_file IN ITEMS "${studio_header}" "${studio_source}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Test Level Runtime handoff file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${studio_header}" header_text)
file(READ "${studio_source}" source_text)

foreach(token IN ITEMS
    "void PreRender() override;"
    "IsTestLevelRuntimeActive() const noexcept")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Test Level Runtime handoff header contract is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "wi::RenderPath2D::Update(dt);"
    "wi::RenderPath2D::PreRender();"
    "wi::RenderPath2D::Render();"
    "wi::RenderPath2D::Compose(cmd);"
    "Runtime owns the live 3D world during Test Level"
    "The child Runtime is the sole 3D owner while Test Level runs")
    string(FIND "${source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Test Level Runtime handoff source contract is missing ${token}")
    endif()
endforeach()

# The Test Level active handoff must be evaluated before the normal Studio 3D
# update. Otherwise Studio and Runtime both advance/render the authored world.
string(FIND "${source_text}" "PollTestLevel();" poll_pos)
string(FIND "${source_text}" "wi::RenderPath2D::Update(dt);" handoff_pos)
string(FIND "${source_text}" "RenderPath3D::Update(dt);" studio_3d_pos)
if(poll_pos EQUAL -1 OR handoff_pos EQUAL -1 OR studio_3d_pos EQUAL -1)
    message(FATAL_ERROR "Test Level Runtime handoff ordering tokens are missing")
endif()
if(NOT poll_pos LESS handoff_pos OR NOT handoff_pos LESS studio_3d_pos)
    message(FATAL_ERROR "Test Level Runtime handoff must precede Studio RenderPath3D::Update")
endif()

# Physics Lab has its own Render/Compose routing. Test Level must take
# precedence there too or that specialist path can accidentally re-enter 3D.
string(FIND "${header_text}" "if (IsTestLevelRuntimeActive())" physics_precedence)
if(physics_precedence EQUAL -1)
    message(FATAL_ERROR "Physics Lab does not preserve Test Level handoff precedence")
endif()

# The old late active-session guard sat after vegetation/drag work. Keeping its
# explanatory marker would indicate the expensive duplicate path has returned.
string(FIND "${source_text}" "StopTestLevel is the only editor action that must still take" stale_late_guard)
if(NOT stale_late_guard EQUAL -1)
    message(FATAL_ERROR "Legacy late Test Level guard is still present")
endif()
