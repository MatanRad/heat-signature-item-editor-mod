add_library(ItemEditor SHARED
    src/GameMakerPropertyAccess.cpp
    src/GameMakerPropertyAccess.h
    src/GadgetEditor.cpp
    src/GadgetEditor.h
    src/GadgetEditorLayout.cpp
    src/GadgetEditorLayout.h
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

heat_signature_configure_mod(ItemEditor)

target_include_directories(ItemEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(ItemEditor PRIVATE imgui::imgui)
