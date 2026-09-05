# Dedicated Gate 9 Studio diagnostics source. Render diagnostics remain
# transient editor tooling and do not enter the persistent RenderSettingsState.
target_sources(RenegadeStudio PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/Phase5Gate9RenderDiagnostics.cpp
)
