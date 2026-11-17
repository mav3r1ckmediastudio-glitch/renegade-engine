# Explicit developer test configuration. Production/default builds keep the
# pinned hardware-selection path. This changes only adapter selection in a
# generated backend so owner hardware can exercise the CI software-GPU path.
if(NOT WIN32 OR NOT BUILD_TESTING)
    message(FATAL_ERROR "RENEGADE_TEST_FORCE_WARP requires Windows and BUILD_TESTING")
endif()
set(_warp_source "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiGraphicsDevice_DX12.cpp")
file(READ "${_warp_source}" _warp_text)
set(_warp_start "\t\t// pick the highest performance adapter that is able to create the device")
set(_warp_end "\t\tassert(dxgiAdapter != nullptr);")
string(FIND "${_warp_text}" "${_warp_start}" _warp_start_pos)
string(FIND "${_warp_text}" "${_warp_end}" _warp_end_pos)
if(_warp_start_pos EQUAL -1 OR _warp_end_pos LESS _warp_start_pos)
    message(FATAL_ERROR "Pinned DX12 adapter-selection anchors changed")
endif()
string(SUBSTRING "${_warp_text}" 0 ${_warp_start_pos} _warp_prefix)
string(SUBSTRING "${_warp_text}" ${_warp_end_pos} -1 _warp_suffix)
set(_warp_selection [=[
        // Test-only: select real WARP explicitly instead of disabling a GPU.
        ComPtr<IDXGIAdapter> warpAdapter;
        hr = dxgiFactory->EnumWarpAdapter(PPV_ARGS(warpAdapter));
        if (SUCCEEDED(hr))
            hr = warpAdapter.As(&dxgiAdapter);
        if (SUCCEEDED(hr))
            hr = D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, PPV_ARGS(device));

]=])
set(_warp_generated "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix/wiGraphicsDevice_DX12_Warp.cpp")
set(_warp_output "${_warp_prefix}${_warp_selection}${_warp_suffix}")
set(_warp_previous "")
if(EXISTS "${_warp_generated}")
    file(READ "${_warp_generated}" _warp_previous)
endif()
if(NOT _warp_previous STREQUAL _warp_output)
    file(WRITE "${_warp_generated}" "${_warp_output}")
endif()
set_source_files_properties("${_warp_source}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties("${_warp_generated}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
target_sources(WickedEngine_common PRIVATE "${_warp_generated}")
target_include_directories(WickedEngine_common PRIVATE
    "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
message(STATUS "TEST-ONLY: native DX12 fixtures use real WARP; not a distributable configuration")
