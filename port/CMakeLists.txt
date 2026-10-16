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

# ReXGlue can emit a fatal stub for conditional branches whose destination is
# present as a local label in the same generated function. Patch those specific
# cases to a normal local goto after codegen and before compiling the sources.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
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
    VERBATIM
)
