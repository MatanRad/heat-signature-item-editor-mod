function(heat_signature_configure_mod target)
    target_compile_features(${target} PRIVATE cxx_std_17)

    target_include_directories(${target} PRIVATE
        "${HEATSIG_MODLOADER_DIR}/ModLoader"
    )

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${target}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    if(HEATSIG_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${HEATSIG_DIR}/mods"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${target}>
                "${HEATSIG_DIR}/mods/$<TARGET_FILE_NAME:${target}>"
            VERBATIM
        )
    else()
        message(WARNING
            "HEATSIG_DIR is not set. ${target}.dll will not be copied automatically.")
    endif()
endfunction()
