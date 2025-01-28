cmake_minimum_required(VERSION 3.30)
project(mapper LANGUAGES CXX C)
include(FetchContent)

function(SetDefaultCompileOptions target)
    target_compile_features(${target} PUBLIC cxx_std_26)
    target_compile_options(${target} PUBLIC
        -Wall -Wextra -Wpedantic
        -Werror
        -Wno-missing-field-initializers
        -Wno-comment
        )
endfunction()

set(VENDOR_DIR .build/3rdparty)
set(BUILD_SHARED_LIBS OFF)

# ------------------------------------------------------------------------------
#       SDL
# ------------------------------------------------------------------------------

add_subdirectory(${VENDOR_DIR}/SDL EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
#       glad
# ------------------------------------------------------------------------------

add_subdirectory(${VENDOR_DIR}/glad/cmake EXCLUDE_FROM_ALL)
glad_add_library(glad
    REPRODUCIBLE EXCLUDE_FROM_ALL LOADER
    API gl:core=4.6
    EXTENSIONS GL_ARB_bindless_texture GL_EXT_texture_compression_s3tc
    )

# ------------------------------------------------------------------------------
#       ImGui
# ------------------------------------------------------------------------------

add_library(imgui)
target_sources(imgui
    PRIVATE
    ${VENDOR_DIR}/imgui/imgui.h
    ${VENDOR_DIR}/imgui/imgui.cpp
    ${VENDOR_DIR}/imgui/imgui_demo.cpp
    ${VENDOR_DIR}/imgui/imgui_tables.cpp
    ${VENDOR_DIR}/imgui/imgui_draw.cpp
    ${VENDOR_DIR}/imgui/imgui_widgets.cpp
    ${VENDOR_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${VENDOR_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    )
target_include_directories(imgui
    PUBLIC
    ${VENDOR_DIR}/imgui
    )
target_link_libraries(imgui
    PUBLIC
    SDL3::SDL3
    )

# ------------------------------------------------------------------------------
#       LuaJIT
# ------------------------------------------------------------------------------

add_library(luajit INTERFACE)
target_include_directories(luajit INTERFACE ${VENDOR_DIR}/luajit/src)
target_link_libraries(luajit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${VENDOR_DIR}/luajit/src/libluajit.a)

# ------------------------------------------------------------------------------
#       sol2
# ------------------------------------------------------------------------------

add_subdirectory(${VENDOR_DIR}/sol2 EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
#       Mapper
# ------------------------------------------------------------------------------

add_executable(${PROJECT_NAME})
SetDefaultCompileOptions(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_sources(${PROJECT_NAME}
    PUBLIC
    src/vjoystick.hpp
    PRIVATE
    src/main.cpp
    src/vjoystick.cpp
    )
target_include_directories(${PROJECT_NAME}
    PUBLIC
    src
    ${glew_SOURCE_DIR}/include
    /usr/include/libevdev-1.0
    )
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    SDL3::SDL3
    glad
    imgui
    luajit
    sol2::sol2
    evdev
    )
