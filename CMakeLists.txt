cmake_minimum_required(VERSION 3.26)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HeatSignatureItemEditor LANGUAGES CXX)

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

add_library(ItemEditor SHARED
    src/GameMakerPropertyAccess.cpp
    src/GameMakerPropertyAccess.h
    src/GunEditor.cpp
    src/GunEditor.h
    src/GunEditorLayout.cpp
    src/GunEditorLayout.h
    src/ItemEditor.cpp
    src/ItemEditorLayout.h
    src/ItemEditorWindow.cpp
    src/ItemEditorWindow.h
    src/MeleeEditor.cpp
    src/MeleeEditor.h
    src/MeleeEditorLayout.cpp
    src/MeleeEditorLayout.h
)

target_compile_features(ItemEditor PRIVATE cxx_std_17)

target_include_directories(ItemEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HEATSIG_MODLOADER_DIR}/ModLoader
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(ItemEditor PRIVATE
    imgui::imgui
)

set_target_properties(ItemEditor PROPERTIES
    OUTPUT_NAME "ItemEditor"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(HEATSIG_DIR)
    add_custom_command(TARGET ItemEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HEATSIG_DIR}/mods"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:ItemEditor>
            "${HEATSIG_DIR}/mods/$<TARGET_FILE_NAME:ItemEditor>"
    )
else()
    message(WARNING "HEATSIG_DIR is not set. ItemEditor.dll will not be copied automatically.")
endif()
