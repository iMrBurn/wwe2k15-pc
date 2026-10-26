# wwe2k15 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(wwe2k15 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# SDL needs an explicit mapping for some DualSense HID revisions on Windows.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gamecontrollerdb.txt"
    "${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt" COPYONLY)

# Sources
set(WWE2K15_SOURCES
    src/main.cpp
    src/render_wait_hook.cpp
    src/framerate_hook.cpp
    src/crash_report.cpp
    src/text_hook.cpp
)

if(WIN32)
    add_executable(wwe2k15 WIN32 ${WWE2K15_SOURCES})
else()
    add_executable(wwe2k15 ${WWE2K15_SOURCES})
endif()

rexglue_setup_target(wwe2k15 GPU_PLUGINS xenos)

# Keep a PDB beside the release exe (not shipped) so crash/hang addresses and CPU profiles
# can be symbolized; /OPT:REF,ICF keep the exe the same as a plain release link.
if(MSVC)
    target_link_options(wwe2k15 PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()
if(WIN32)
    target_sources(wwe2k15 PRIVATE res/wwe2k15.rc)
    # rc.exe dependency scanning misses the icon; rebuild the resource when it changes.
    set_source_files_properties(res/wwe2k15.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/wwe2k15.ico)
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Native Win32 launcher, styled after the project's WWE '12 launcher.
if(WIN32)
    set(WWE2K15_IMGUI_DIR "" CACHE PATH "Path to Dear ImGui sources (SDK thirdparty/imgui)")
    if(NOT WWE2K15_IMGUI_DIR AND REXSDK_DIR)
        set(WWE2K15_IMGUI_DIR "${REXSDK_DIR}/thirdparty/imgui")
    endif()
    if(NOT EXISTS "${WWE2K15_IMGUI_DIR}/imgui.cpp")
        message(FATAL_ERROR "Dear ImGui source path missing. Set WWE2K15_IMGUI_DIR to the SDK thirdparty/imgui folder.")
    endif()
    add_executable(wwe2k15_launcher WIN32
        src/launcher/launcher.cpp
        "${WWE2K15_IMGUI_DIR}/imgui.cpp"
        "${WWE2K15_IMGUI_DIR}/imgui_draw.cpp"
        "${WWE2K15_IMGUI_DIR}/imgui_tables.cpp"
        "${WWE2K15_IMGUI_DIR}/imgui_widgets.cpp"
        "${WWE2K15_IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        "${WWE2K15_IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
        res/wwe2k15.rc)
    target_include_directories(wwe2k15_launcher PRIVATE
        "${WWE2K15_IMGUI_DIR}" "${WWE2K15_IMGUI_DIR}/backends")
    find_package(SDL3 CONFIG REQUIRED)
    target_link_libraries(wwe2k15_launcher PRIVATE SDL3::SDL3-static d3d11 dxgi d3dcompiler shell32 ole32 oleaut32 dwmapi winmm setupapi imm32 version cfgmgr32 winhttp)
    set_target_properties(wwe2k15_launcher PROPERTIES OUTPUT_NAME "WWE 2K15 Launcher")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" WWE2K15_VERSION LIMIT_COUNT 1)
    target_compile_definitions(wwe2k15_launcher PRIVATE WWE2K15_VERSION="${WWE2K15_VERSION}")
    add_dependencies(wwe2k15 wwe2k15_launcher)
endif()

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
