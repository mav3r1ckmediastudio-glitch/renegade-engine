if(NOT DEFINED INPUT_FIXTURE OR
   NOT DEFINED LEVEL_TWO_SCENE OR
   NOT DEFINED OUTPUT_FIXTURE)
    message(FATAL_ERROR
        "LP07 Gate 6 fixture preparation requires INPUT_FIXTURE, LEVEL_TWO_SCENE and OUTPUT_FIXTURE.")
endif()

if(NOT IS_DIRECTORY "${INPUT_FIXTURE}")
    message(FATAL_ERROR "LP07 Gate 6 input fixture is unavailable: ${INPUT_FIXTURE}")
endif()
if(NOT EXISTS "${LEVEL_TWO_SCENE}")
    message(FATAL_ERROR "LP07 Gate 6 generated Level Two scene is unavailable: ${LEVEL_TWO_SCENE}")
endif()

file(REMOVE_RECURSE "${OUTPUT_FIXTURE}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${INPUT_FIXTURE}"
        "${OUTPUT_FIXTURE}"
    RESULT_VARIABLE copy_fixture_result
)
if(NOT copy_fixture_result EQUAL 0)
    message(FATAL_ERROR "Could not copy the LP03 fixture for LP07 Gate 6.")
endif()

file(MAKE_DIRECTORY "${OUTPUT_FIXTURE}/Content/Scenes")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${LEVEL_TWO_SCENE}"
        "${OUTPUT_FIXTURE}/Content/Scenes/LevelTwo.wiscene"
    RESULT_VARIABLE copy_scene_result
)
if(NOT copy_scene_result EQUAL 0 OR
   NOT EXISTS "${OUTPUT_FIXTURE}/Content/Scenes/LevelTwo.wiscene")
    message(FATAL_ERROR "Could not stage the valid LP07 Gate 6 Level Two WISCENE fixture.")
endif()

message(STATUS
    "LP07 Gate 6 derived fixture prepared at: ${OUTPUT_FIXTURE}")
