# wwe2k15 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(wwe2k15 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(WWE2K15_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(wwe2k15 WIN32 ${WWE2K15_SOURCES})
else()
    add_executable(wwe2k15 ${WWE2K15_SOURCES})
endif()

rexglue_setup_target(wwe2k15 GPU_PLUGINS xenos)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Optional D3D call census for identifying WWE 2K15's title-specific XDK entry points.
# The first candidate window is anchored by sub_82BA5FA8, which calls VdSwap.
option(WWE2K15_D3D_CENSUS "Instrument the initial WWE 2K15 D3D address window" OFF)
if(WWE2K15_D3D_CENSUS)
    set(WWE2K15_D3D_CENSUS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/wwe2k15_d3d_census.cpp")
    add_custom_command(
        OUTPUT "${WWE2K15_D3D_CENSUS_SOURCE}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_d3d_census.py"
            --register "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/wwe2k15_register.cpp"
            --out "${WWE2K15_D3D_CENSUS_SOURCE}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/wwe2k15_register.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_d3d_census.py"
        COMMENT "Generating the WWE 2K15 D3D census hooks"
        VERBATIM
    )
    add_custom_target(wwe2k15_d3d_census_codegen DEPENDS "${WWE2K15_D3D_CENSUS_SOURCE}")
    add_dependencies(wwe2k15_d3d_census_codegen wwe2k15_codegen)
    target_sources(wwe2k15 PRIVATE "${WWE2K15_D3D_CENSUS_SOURCE}")
    add_dependencies(wwe2k15 wwe2k15_d3d_census_codegen)
endif()

# ReXGlue can emit a fatal stub for conditional branches whose destination is
# present as a local label in the same generated function. Patch those specific
# cases to a normal local goto after codegen and before compiling the sources.
add_custom_target(wwe2k15_patch_codegen
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_codegen.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Repairing local branches in generated code"
    VERBATIM
)
add_dependencies(wwe2k15_patch_codegen wwe2k15_codegen)
if(TARGET wwe2k15_recomp)
    add_dependencies(wwe2k15_recomp wwe2k15_patch_codegen)
endif()
add_dependencies(wwe2k15 wwe2k15_patch_codegen)

# Keep runtime settings beside the executable, where ReXApp loads wwe2k15.toml.
set_property(TARGET wwe2k15 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/wwe2k15_runtime.toml"
)
add_custom_command(TARGET wwe2k15 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/wwe2k15_runtime.toml"
        "$<TARGET_FILE_DIR:wwe2k15>/wwe2k15.toml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/wwe2k15-launcher.ps1"
        "$<TARGET_FILE_DIR:wwe2k15>/wwe2k15-launcher.ps1"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/launch-wwe2k15.bat"
        "$<TARGET_FILE_DIR:wwe2k15>/Lancer WWE 2K15.bat"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/launch-wwe2k15-no-obs.bat"
        "$<TARGET_FILE_DIR:wwe2k15>/Lancer WWE 2K15 - sans OBS.bat"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/launch-wwe2k15-sdl.bat"
        "$<TARGET_FILE_DIR:wwe2k15>/Lancer WWE 2K15 - manette SDL.bat"
    VERBATIM
)
