# Retain the verified file generation used by a governed DDS across atomic
# project reimports. Upstream remembers just a path and reopens it for every mip.
set(_src "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiResourceManager.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_src}")
file(READ "${_src}" _rm)
function(renegade_texture_replace before after)
    string(FIND "${_rm}" "${before}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pinned Wicked texture loader anchor missing: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" result "${_rm}")
    set(_rm "${result}" PARENT_SCOPE)
endfunction()
renegade_texture_replace([=[#include "wiResourceManager.h"]=]
    "#include \"${CMAKE_SOURCE_DIR}/cmake/RenegadeTextureBacking.h\"")
renegade_texture_replace([=[std::string container_filename;]=]
    [=[std::string container_filename;
        std::shared_ptr<const resourcemanager::GovernedTextureBacking> governed_backing;]=])
renegade_texture_replace("Resource Load(\n" "static Resource LoadInternal(\n")
renegade_texture_replace("size_t container_fileoffset\n\t\t)" [=[size_t container_fileoffset,
            std::shared_ptr<const GovernedTextureBacking> backing
        )]=])
renegade_texture_replace("resource->container_fileoffset = container_fileoffset;"
    "resource->container_fileoffset = container_fileoffset;\n\t\t\t\tresource->governed_backing = std::move(backing);")
renegade_texture_replace("bool Contains(const std::string& name)" [=[Resource Load(const std::string& name, Flags flags, const uint8_t* data,
            size_t length, const std::string& container, size_t offset)
        {
            return LoadInternal(name, flags, data, length, container, offset, {});
        }

        Resource LoadGovernedTexture(const std::string& name, const uint8_t* data,
            size_t length, const std::string& container, size_t offset,
            std::shared_ptr<const GovernedTextureBacking> backing)
        {
            if (!backing || !data || !length)
                return {};
            return LoadInternal(name, Flags::STREAMING, data, length, container, offset, std::move(backing));
        }

        bool Contains(const std::string& name)]=])
renegade_texture_replace("if (!wi::helper::FileRead(\n\t\t\t\t\t\t\t\tresource->container_filename,"
    "if (resource->governed_backing ? !resource->governed_backing->Read(mip_data_offset, filesize, streaming_file) : !wi::helper::FileRead(\n\t\t\t\t\t\t\t\tresource->container_filename,")
renegade_texture_replace("if (resource->filedata.empty())\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\t// Directly re-read"
    "wi::vector<uint8_t> governed_archive_data;\n                        if (resource->governed_backing)\n                            resource->governed_backing->Read(0, resource->container_filesize, governed_archive_data);\n                        else if (resource->filedata.empty())\n                        {\n                            // Directly re-read")
renegade_texture_replace("archive << resource->filedata;"
    "archive << (resource->governed_backing ? governed_archive_data : resource->filedata);")
renegade_texture_replace("if (!archive.GetSourceFileName().empty())\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\t// Refresh"
    "if (!resource->governed_backing && !archive.GetSourceFileName().empty())\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\t// Refresh")
set(_out "${CMAKE_BINARY_DIR}/RenegadeWickedFix/wiResourceManager.cpp")
file(CONFIGURE OUTPUT "${_out}" CONTENT "${_rm}" @ONLY)
set_source_files_properties("${_src}" TARGET_DIRECTORY WickedEngine_common PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(WickedEngine_common PRIVATE "${_out}")
set_source_files_properties("${_out}" TARGET_DIRECTORY WickedEngine_common PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
