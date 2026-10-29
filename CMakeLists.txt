cmake_minimum_required(VERSION 3.20)
project(overlay-main LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "overlay-main requires Windows x64.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(config_header "${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/config.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${config_header}")
file(READ "${config_header}" config_text)
set(feature_definitions)
foreach(feature IN ITEMS OVERLAY_TARGET_MODE OVERLAY_VIRTUAL_CURSOR OVERLAY_ZORDER_MODE OVERLAY_USE_ZBID OVERLAY_ENABLE_CONSOLE)
    if(NOT DEFINED ${feature})
        string(REGEX MATCH "#define[ \t]+${feature}[ \t]+([01])([ \t\r\n]|$)" definition "${config_text}")
        if(NOT definition)
            message(FATAL_ERROR "${feature} must have a 0 or 1 default in config.h.")
        endif()
        set(${feature} "${CMAKE_MATCH_1}")
    endif()
    string(TOUPPER "${${feature}}" feature_value)
    if(NOT feature_value MATCHES "^(0|1|ON|OFF|TRUE|FALSE)$")
        message(FATAL_ERROR "${feature} must be 0, 1, ON or OFF.")
    endif()
    list(APPEND feature_definitions "${feature}=$<BOOL:${${feature}}>")
endforeach()

if(OVERLAY_ZORDER_MODE AND (NOT OVERLAY_TARGET_MODE OR OVERLAY_USE_ZBID))
    message(FATAL_ERROR "Z-order mode requires target mode and cannot be combined with ZBID.")
endif()

include(FetchContent)
FetchContent_Declare(imgui_source
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui_source)

add_library(imgui STATIC
    ${imgui_source_SOURCE_DIR}/imgui.cpp
    ${imgui_source_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_source_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_source_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_source_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_source_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
target_include_directories(imgui PUBLIC ${imgui_source_SOURCE_DIR} ${imgui_source_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601)

add_executable(overlay WIN32
    src/main.cpp
    src/overlay/window_manager.cpp
    src/overlay/background.cpp
    src/overlay/menu.cpp
    src/overlay/render/drawing.cpp
    src/overlay/render/render.cpp)
if(OVERLAY_VIRTUAL_CURSOR)
    target_sources(overlay PRIVATE src/overlay/render/cursor.cpp)
endif()
if(OVERLAY_USE_ZBID)
    target_sources(overlay PRIVATE src/overlay/zbid.cpp)
endif()
target_include_directories(overlay PRIVATE src)
target_compile_definitions(overlay PRIVATE ${feature_definitions})
target_link_libraries(overlay PRIVATE imgui d3d11 dxgi dwmapi user32 gdi32)

if(MSVC)
    foreach(target IN ITEMS imgui overlay)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(${target} PRIVATE /permissive- /utf-8 /Zc:__cplusplus /EHsc)
    endforeach()
    target_compile_options(imgui PRIVATE /W3)
    target_compile_options(overlay PRIVATE /W4)
endif()
