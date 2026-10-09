add_executable(RenegadeProjectileSimulationTests
    ${CMAKE_CURRENT_LIST_DIR}/ProjectileSimulationTests.cpp
)
target_include_directories(RenegadeProjectileSimulationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/EngineBridge/include
)
target_compile_features(RenegadeProjectileSimulationTests PRIVATE cxx_std_17)
set_target_properties(RenegadeProjectileSimulationTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeProjectileSimulationTests)
add_test(NAME RenegadeProjectileSimulationTests COMMAND RenegadeProjectileSimulationTests)

add_executable(RenegadeProjectileWorldTests
    ${CMAKE_CURRENT_LIST_DIR}/ProjectileWorldTests.cpp
)
target_link_libraries(RenegadeProjectileWorldTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeProjectileWorldTests PRIVATE ${CMAKE_SOURCE_DIR}/Runtime/src)
target_compile_features(RenegadeProjectileWorldTests PRIVATE cxx_std_17)
set_target_properties(RenegadeProjectileWorldTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeProjectileWorldTests)
add_test(NAME RenegadeProjectileWorldTests COMMAND RenegadeProjectileWorldTests)

add_executable(RenegadeProjectileAssetTests ${CMAKE_CURRENT_LIST_DIR}/ProjectileAssetTests.cpp)
target_link_libraries(RenegadeProjectileAssetTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeProjectileAssetTests PRIVATE "${CMAKE_SOURCE_DIR}/WickedEngine/Editor")
target_compile_features(RenegadeProjectileAssetTests PRIVATE cxx_std_17)
set_target_properties(RenegadeProjectileAssetTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeProjectileAssetTests)
add_test(NAME RenegadeProjectileAssetTests COMMAND RenegadeProjectileAssetTests)

enable_language(RC)
configure_file("${CMAKE_SOURCE_DIR}/Runtime/RuntimeImpactTextures.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ImpactTestTextures.rc" @ONLY)

add_executable(RenegadeImpactAudioTests ${CMAKE_CURRENT_LIST_DIR}/ImpactAudioTests.cpp
    ${CMAKE_SOURCE_DIR}/Runtime/src/RuntimeImpactDustResource.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ImpactTestTextures.rc)
target_link_libraries(RenegadeImpactAudioTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeImpactAudioTests PRIVATE ${CMAKE_SOURCE_DIR}/WickedEngine/Editor)
target_compile_features(RenegadeImpactAudioTests PRIVATE cxx_std_17)
add_test(NAME RenegadeImpactAudioTests COMMAND RenegadeImpactAudioTests)
add_dependencies(RenegadeBridgeTests RenegadeImpactAudioTests)

add_executable(RenegadeRuntimeProjectileSessionTests ${CMAKE_CURRENT_LIST_DIR}/RuntimeProjectileSessionTests.cpp
    ${CMAKE_SOURCE_DIR}/Runtime/src/RuntimeImpactDustResource.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ImpactTestTextures.rc)
target_link_libraries(RenegadeRuntimeProjectileSessionTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeRuntimeProjectileSessionTests PRIVATE ${CMAKE_SOURCE_DIR}/Runtime/src)
target_compile_features(RenegadeRuntimeProjectileSessionTests PRIVATE cxx_std_17)
set_target_properties(RenegadeRuntimeProjectileSessionTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeRuntimeProjectileSessionTests)
add_test(NAME RenegadeRuntimeProjectileSessionTests COMMAND RenegadeRuntimeProjectileSessionTests)
