# Required native material resources; use the same staging path for local builds and CI.
foreach(required IN ITEMS RENEGADE_ROCKY_SOURCE RENEGADE_ROCKY_OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing rocky riverbed resource directory: ${required}")
    endif()
endforeach()
if(NOT IS_ABSOLUTE "${RENEGADE_ROCKY_SOURCE}" OR
   NOT IS_ABSOLUTE "${RENEGADE_ROCKY_OUTPUT}" OR
   RENEGADE_ROCKY_SOURCE STREQUAL RENEGADE_ROCKY_OUTPUT)
    message(FATAL_ERROR "Rocky resource staging needs distinct absolute source and output directories")
endif()
file(MAKE_DIRECTORY "${RENEGADE_ROCKY_OUTPUT}")
foreach(resource IN ITEMS rocky_basecolor.png rocky_normal_dx.png rocky_surface.png material_manifest.json rocky_height_reference.png rocky_preview.jpg)
    if(NOT EXISTS "${RENEGADE_ROCKY_SOURCE}/${resource}")
        message(FATAL_ERROR "Missing supplied rocky riverbed resource: ${resource}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${RENEGADE_ROCKY_SOURCE}/${resource}"
        "${RENEGADE_ROCKY_OUTPUT}/${resource}"
        RESULT_VARIABLE copy_result)
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Could not stage rocky riverbed resource: ${resource}")
    endif()
endforeach()
