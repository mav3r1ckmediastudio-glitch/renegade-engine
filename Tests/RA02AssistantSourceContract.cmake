cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED RENEGADE_SOURCE_DIR)
    get_filename_component(RENEGADE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
set(studio "${RENEGADE_SOURCE_DIR}/Studio/src")
file(READ "${studio}/RenegadeAssistantDock.h" assistant_header)
file(READ "${studio}/RenegadeAssistantDock.cpp" assistant_source)
file(READ "${studio}/StudioApplication.cpp" application)
file(READ "${studio}/RenegadeStudioChrome.cpp" chrome)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt" cmake_source)

foreach(symbol IN ITEMS
    "class RenegadeAssistantDock"
    "class RenegadeAssistantPortrait"
    "void NextGuideStep()"
    "void PreviousGuideStep()"
    "void StartRetargetGuide()"
    "void SubmitText(std::string text)"
)
    string(FIND "${assistant_header}" "${symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Assistant header missing: ${symbol}")
    endif()
endforeach()

foreach(symbol IN ITEMS
    "Saved/Assistant/StudioChatHistory.txt"
    "Saved/Assistant/RetargetGuideStep.txt"
    "HUMANOID / RETARGET"
    "AUTO-MAP"
    "IMPORT + RETARGET"
    "ANIMATION"
    "127.0.0.1"
    "WinHttpOpenRequest"
    "std::async"
    "std::quoted"
    "I can't create an entire level or game"
)
    string(FIND "${assistant_source}${assistant_header}" "${symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Assistant implementation missing: ${symbol}")
    endif()
endforeach()

foreach(symbol IN ITEMS
    "assistantDock_.Create()"
    "assistantDock_.Tick()"
    "assistantDock_.SetLayout("
    "assistantDock_.SetOpen("
    "studioChrome_.SetAssistantDockWidth("
    "Action::ToggleAssistant"
)
    string(FIND "${application}" "${symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio integration missing: ${symbol}")
    endif()
endforeach()

foreach(symbol IN ITEMS
    "width_ - inspectorWidth_ - assistantDockWidth_"
    "SHOW ASSISTANT"
    "Action::ToggleAssistant"
)
    string(FIND "${chrome}" "${symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Viewport reserve / menu missing: ${symbol}")
    endif()
endforeach()

foreach(symbol IN ITEMS
    "src/RenegadeAssistantDock.cpp"
    "winhttp"
)
    string(FIND "${cmake_source}" "${symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Build registration missing: ${symbol}")
    endif()
endforeach()

message(STATUS "RA-02 initial assistant source contract passed (read-only shell + guide + loopback)")
