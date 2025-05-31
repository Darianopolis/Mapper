cmake_minimum_required(VERSION 3.30)
project(mapper LANGUAGES CXX C)

function(SetDefaultCompileOptions target)
    if(MSVC)
        # clang-cl
        target_compile_options(${target} PUBLIC
            /clang:-std=c++26
            /clang:-Wall /clang:-Wextra /clang:-Wpedantic
            /clang:-Werror
            /clang:-Wno-missing-field-initializers
            /clang:-Wno-unknown-pragmas
            /clang:-Wno-comment
            )
    else()
        # clang
        target_compile_features(${target} PUBLIC cxx_std_26)
        target_compile_options(${target} PUBLIC
            -Wall -Wextra -Wpedantic
            -Werror
            -Wno-missing-field-initializers
            -Wno-unknown-pragmas
            -Wno-comment
            )
    endif()
    if (UNIX)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PUBLIC -ggdb)
        endif()
    endif()
endfunction()

set(BUILD_SHARED_LIBS OFF)

# ------------------------------------------------------------------------------
#       SDL
# ------------------------------------------------------------------------------

set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
add_subdirectory(${VENDOR_DIR}/SDL EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
#       GLFW
# ------------------------------------------------------------------------------

add_subdirectory(${VENDOR_DIR}/GLFW EXCLUDE_FROM_ALL)

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
    ${VENDOR_DIR}/imgui/backends/imgui_impl_glfw.cpp
    )
target_include_directories(imgui
    PUBLIC
    ${VENDOR_DIR}/imgui
    )
target_link_libraries(imgui
    PUBLIC
    # SDL3::SDL3
    glfw
    )

# ------------------------------------------------------------------------------
#       LuaJIT
# ------------------------------------------------------------------------------

add_library(luajit INTERFACE)
target_include_directories(luajit INTERFACE ${VENDOR_DIR}/luajit/src)
if(UNIX)
    target_link_libraries(luajit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${VENDOR_DIR}/luajit/src/libluajit.a)
endif()
if(WIN32)
    target_link_libraries(luajit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${VENDOR_DIR}/luajit/src/luajit.lib)
    target_link_libraries(luajit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${VENDOR_DIR}/luajit/src/lua51.lib)
endif()

# ------------------------------------------------------------------------------
#       sol2
# ------------------------------------------------------------------------------

add_subdirectory(${VENDOR_DIR}/sol2 EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
#       Mapper
# ------------------------------------------------------------------------------

add_executable(${PROJECT_NAME})
SetDefaultCompileOptions(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PUBLIC
    src/vjoystick.hpp
    src/mapper.hpp
    src/common.hpp
    PRIVATE
    src/main.cpp
    src/gui.cpp
    src/engine.cpp
    src/scripts.cpp
    )
target_include_directories(${PROJECT_NAME}
    PUBLIC
    src
    )
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    SDL3::SDL3
    glfw
    imgui
    luajit
    sol2::sol2
    )

if (WIN32)
    target_sources(${PROJECT_NAME}
        PUBLIC
        src/windows/vjoy.hpp
        PRIVATE
        src/windows/vjoy.cpp
        src/windows/vjoystick.cpp
        resources/mapper.rc
        )
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Opengl32.lib
        )
endif()

if (UNIX)
    target_sources(${PROJECT_NAME}
        PRIVATE
        src/linux/vjoystick.cpp
        )
    target_include_directories(${PROJECT_NAME}
        PUBLIC
        /usr/include/libevdev-1.0
        )
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        evdev
        GL)
endif()
