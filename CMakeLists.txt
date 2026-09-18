cmake_minimum_required(VERSION 3.21)

# "Apparel Preview" - preview armor on the player in Container/Barter menus
# without equipping. One CommonLibSSE-NG DLL for SE 1.5.97 and AE 1.6.317 and
# later (AE offsets verified against the 1.6.1170 binary; 1.7.104 in testing,
# see VersionCheck.cpp).
project(ApparelPreview VERSION 1.2.0 LANGUAGES CXX)

# alandtse's CommonLibSSE-NG, branch ng, pinned as a submodule (see
# .gitmodules). Consumed with add_subdirectory. Its own tests and their
# Catch2 dependency stay out of this build, and the add_commonlibsse_plugin
# helper is only pulled in by the library's prebuilt path, so the source
# build includes it here.
set(BUILD_TESTS OFF CACHE BOOL "CommonLibSSE-NG tests are not built by this consumer" FORCE)
add_subdirectory(extern/CommonLibSSE-NG EXCLUDE_FROM_ALL)
include("${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake")
get_directory_property(AP_COMMONLIB_VERSION DIRECTORY extern/CommonLibSSE-NG DEFINITION CommonLibSSE_VERSION)
message(STATUS "ApparelPreview builds on CommonLibSSE-NG ${AP_COMMONLIB_VERSION}")
# jsoncpp's exported config predates CMake 3.5's policy floor.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
find_package(jsoncpp CONFIG REQUIRED)  # DIII_API.h passes Json::Value across the API
unset(CMAKE_POLICY_VERSION_MINIMUM)
find_package(imgui CONFIG REQUIRED)  # FLICK/FUCK API surface (FUCK_API.h needs imgui >= 1.92 types)

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES
        src/PCH.h
        src/plugin.cpp
        src/VersionCheck.h
        src/VersionCheck.cpp
        src/PreviewSet.h
        src/ItemPreviewSuppression.h
        src/FittingRoomSignal.h
        src/MarkerRefresh.h
        src/MenuStudioApi.h
        src/MenuStudioApi.cpp
        src/Settings.h
        src/Settings.cpp
        src/REAugments.h
        src/REAugments.cpp
        src/PreviewSession.h
        src/PreviewSession.cpp
        src/MenuWatcher.h
        src/MenuWatcher.cpp
        src/ListMarker.h
        src/ListMarker.cpp
        src/HoverTracker.h
        src/HoverTracker.cpp
        src/BipedHooks.h
        src/BipedHooks.cpp
        src/InputListener.h
        src/InputListener.cpp
        src/NavButton.h
        src/NavButton.cpp
        src/DIIIIntegration.h
        src/DIIIIntegration.cpp
        src/SettingsUI.h
        src/SettingsUI.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE extern)
target_link_libraries(${PROJECT_NAME} PRIVATE JsonCpp::JsonCpp imgui::imgui)
# FLICK / FUCK is consumed at runtime via its C-ABI (extern/FUCK_API.h +
# FUCK.dll's RequestFUCK export); we only need imgui's headers/types for the
# API surface, FUCK.dll does the rendering.
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS
    AP_COMMONLIB_VERSION="${AP_COMMONLIB_VERSION}")  # printed by the load-time self-check

# Debug symbols in Release. A crash log names "ApparelPreview.dll+offset" and
# nothing else; without the PDB that offset cannot be turned into a function.
# /Zi emits the debug info and /DEBUG makes the linker write the PDB; /OPT:REF
# and /OPT:ICF put back the two optimisations /DEBUG turns off, so the shipped
# code stays a plain Release build. The PDB stays in build/release next to
# the DLL the packager copies, is never deployed or packaged, and is archived
# beside each release zip for offline resolution (Tools/re/pdb_syms.py).
target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:ICF)

# Pure-logic unit tests (no RE/engine deps).
add_executable(PreviewSetTests tests/test_preview_set.cpp)
target_compile_features(PreviewSetTests PRIVATE cxx_std_23)
target_include_directories(PreviewSetTests PRIVATE src)

if(NOT DEFINED OUTPUT_FOLDER)
    set(OUTPUT_FOLDER "C:/Games/Nolvus/Instances/Nolvus Awakening/MODS/mods/Apparel Preview")
endif()
set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
message(STATUS "ApparelPreview deploy folder: ${DLL_FOLDER}")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/dist" "${OUTPUT_FOLDER}"
    VERBATIM)
