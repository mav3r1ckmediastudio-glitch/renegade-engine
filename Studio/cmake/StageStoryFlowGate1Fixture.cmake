if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()
if(NOT DEFINED SOURCE_SCENE OR SOURCE_SCENE STREQUAL "")
    message(FATAL_ERROR "SOURCE_SCENE is required")
endif()
if(NOT EXISTS "${SOURCE_SCENE}")
    message(FATAL_ERROR "Story Flow Gate 1 fixture source scene does not exist: ${SOURCE_SCENE}")
endif()

set(FIXTURE_ROOT "${OUTPUT_DIR}/Gate1OwnerFixture")
set(SCENE_DIR "${FIXTURE_ROOT}/Content/Scenes")
set(FLOW_DIR "${FIXTURE_ROOT}/Content/Flow")

file(REMOVE_RECURSE "${FIXTURE_ROOT}")
file(MAKE_DIRECTORY "${SCENE_DIR}")
file(MAKE_DIRECTORY "${FLOW_DIR}")

# These IDs are fixture-local, stable and deliberately fixed so the packaged
# owner-test project is reproducible across CI runs.
set(PROJECT_ID "11111111-1111-4111-8111-111111111111")
set(FLOW_ID "22222222-2222-4222-8222-222222222222")
set(GAME_START_ID "33333333-3333-4333-8333-333333333333")
set(LEVEL_ONE_NODE_ID "44444444-4444-4444-8444-444444444444")
set(LEVEL_TWO_NODE_ID "55555555-5555-4555-8555-555555555555")
set(COMPLETE_NODE_ID "66666666-6666-4666-8666-666666666666")
set(LEVEL_ONE_SCENE_ID "77777777-7777-4777-8777-777777777777")
set(LEVEL_TWO_SCENE_ID "88888888-8888-4888-8888-888888888888")
set(ROUTE_START_ID "99999999-9999-4999-8999-999999999999")
set(ROUTE_ONE_ID "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
set(ROUTE_TWO_ID "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb")

configure_file("${SOURCE_SCENE}" "${SCENE_DIR}/LevelOne.wiscene" COPYONLY)
configure_file("${SOURCE_SCENE}" "${SCENE_DIR}/LevelTwo.wiscene" COPYONLY)

file(WRITE "${SCENE_DIR}/LevelOne.wiscene.rmeta" "format = renegade-document\nversion = 1\n\n[document]\nid = ${LEVEL_ONE_SCENE_ID}\nproject_id = ${PROJECT_ID}\ntype = scene\npath_hint = Content/Scenes/LevelOne.wiscene\ngenerator = Renegade Story Flow Gate 1 packaged owner fixture\nmigrated_from = 0\n")

file(WRITE "${SCENE_DIR}/LevelTwo.wiscene.rmeta" "format = renegade-document\nversion = 1\n\n[document]\nid = ${LEVEL_TWO_SCENE_ID}\nproject_id = ${PROJECT_ID}\ntype = scene\npath_hint = Content/Scenes/LevelTwo.wiscene\ngenerator = Renegade Story Flow Gate 1 packaged owner fixture\nmigrated_from = 0\n")

file(WRITE "${FLOW_DIR}/Main.renegade-flow" "format = renegade-document\nversion = 1\n\n[document]\nid = ${FLOW_ID}\nproject_id = ${PROJECT_ID}\ntype = story-flow\npath_hint = Content/Flow/Main.renegade-flow\ngenerator = Renegade Story Flow Gate 1 packaged owner fixture\nmigrated_from = 0\n\n[flow]\nstart_node = ${GAME_START_ID}\nnode_count = 4\nroute_count = 3\n\n[node_0]\nid = ${GAME_START_ID}\nkind = game_start\nname = Game Start\nscene_asset_id =\nscene_path_hint =\n\n[node_1]\nid = ${LEVEL_ONE_NODE_ID}\nkind = level\nname = Level One\nscene_asset_id = ${LEVEL_ONE_SCENE_ID}\nscene_path_hint = Content/Scenes/LevelOne.wiscene\n\n[node_2]\nid = ${LEVEL_TWO_NODE_ID}\nkind = level\nname = Level Two\nscene_asset_id = ${LEVEL_TWO_SCENE_ID}\nscene_path_hint = Content/Scenes/LevelTwo.wiscene\n\n[node_3]\nid = ${COMPLETE_NODE_ID}\nkind = complete_game\nname = Complete Game\nscene_asset_id =\nscene_path_hint =\n\n[route_0]\nid = ${ROUTE_START_ID}\nsource = ${GAME_START_ID}\noutcome = renegade.flow.start\ndestination = ${LEVEL_ONE_NODE_ID}\ndestination_entry = default\npriority = 0\ncondition_count = 0\n\n[route_1]\nid = ${ROUTE_ONE_ID}\nsource = ${LEVEL_ONE_NODE_ID}\noutcome = level.complete\ndestination = ${LEVEL_TWO_NODE_ID}\ndestination_entry = from-level-one\npriority = 0\ncondition_count = 0\n\n[route_2]\nid = ${ROUTE_TWO_ID}\nsource = ${LEVEL_TWO_NODE_ID}\noutcome = level.complete\ndestination = ${COMPLETE_NODE_ID}\ndestination_entry =\npriority = 0\ncondition_count = 0\n")

file(WRITE "${FIXTURE_ROOT}/StoryFlowGate1Fixture.renegade" "format = renegade-project\nversion = 1\n\n[project]\nproject_id = ${PROJECT_ID}\nname = Story Flow Gate 1 Fixture\nstartup_scene = Content/Scenes/LevelOne.wiscene\nstartup_flow_id = ${FLOW_ID}\nstartup_flow = Content/Flow/Main.renegade-flow\n")

file(WRITE "${FIXTURE_ROOT}/OPEN_THIS_PROJECT.txt" "STORY FLOW GATE 1 OWNER TEST\n\n1. Start RenegadeStudio.exe from the Release build.\n2. In Project Hub choose Open Project.\n3. Browse into this Gate1OwnerFixture folder.\n4. Open StoryFlowGate1Fixture.renegade.\n\nExpected Story Flow:\nGAME START -> LEVEL ONE -> LEVEL TWO -> COMPLETE GAME\n\nNo command line, PowerShell or project preparation is required.\n")

message(STATUS "Staged click-only Story Flow Gate 1 owner fixture: ${FIXTURE_ROOT}")
