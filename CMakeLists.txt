cmake_minimum_required(VERSION 3.26)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HeatSignatureMods LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Heat Signature mods require a 32-bit Windows build. Configure with -A Win32.")
endif()

set(
    HEATSIG_MODLOADER_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../HeatSignatureModLoader"
    CACHE PATH
    "Path to a HeatSignatureModLoader checkout"
)

if(NOT EXISTS "${HEATSIG_MODLOADER_DIR}/ModLoader/ModInterface.h")
    message(FATAL_ERROR
        "HEATSIG_MODLOADER_DIR must point to a HeatSignatureModLoader checkout")
endif()

set(HEATSIG_DIR "" CACHE PATH "Path to the Heat Signature game directory")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/HeatSignatureMods.cmake")

option(HS_BUILD_ITEM_EDITOR "Build the ItemEditor mod" ON)

if(HS_BUILD_ITEM_EDITOR)
    add_subdirectory(mods/item-editor)
endif()
