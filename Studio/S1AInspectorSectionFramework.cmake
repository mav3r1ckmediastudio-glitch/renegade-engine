add_library(RenegadeInspectorSectionFramework STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/InspectorSectionFramework.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/InspectorSectionFramework.h"
)

add_library(
    Renegade::InspectorSectionFramework
    ALIAS RenegadeInspectorSectionFramework
)

target_include_directories(RenegadeInspectorSectionFramework
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/src"
)

target_link_libraries(RenegadeInspectorSectionFramework
    PUBLIC
        Renegade::EngineBridge
)

target_compile_options(RenegadeInspectorSectionFramework
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeInspectorSectionFramework PROPERTIES
    FOLDER "Renegade/Studio"
)

target_link_libraries(RenegadeStudio
    PRIVATE
        Renegade::InspectorSectionFramework
)
