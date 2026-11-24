cmake_minimum_required(VERSION 3.22.1)
project(craftya CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine)

add_library(craftya SHARED
    native_main.cpp
    android_platform.cpp
    ${ENGINE_ROOT}/src/game.cpp
    ${ENGINE_ROOT}/src/render.cpp
    ${ENGINE_ROOT}/src/mesher.cpp
    ${ENGINE_ROOT}/src/physics.cpp
    ${ENGINE_ROOT}/src/platform.cpp
    ${ENGINE_ROOT}/src/json.cpp
    ${ENGINE_ROOT}/src/lighting.cpp
    ${ENGINE_ROOT}/src/atlas.cpp
    ${ENGINE_ROOT}/src/renderer.cpp
    ${ENGINE_ROOT}/src/inventory.cpp
    ${ENGINE_ROOT}/src/save.cpp
    ${ENGINE_ROOT}/src/net.cpp
    ${ENGINE_ROOT}/src/ecs.cpp
    ${ENGINE_ROOT}/src/audio.cpp
    ${ENGINE_ROOT}/src/gui.cpp
    ${ENGINE_ROOT}/src/interact.cpp
    ${ENGINE_ROOT}/src/anim.cpp
)

target_include_directories(craftya PRIVATE ${ENGINE_ROOT}/include)

target_compile_options(craftya PRIVATE
    -O2
    -Wall
    -Wextra
    -Wno-unused-parameter
    -funroll-loops
    -fno-exceptions
    -fno-rtti
)

target_link_libraries(craftya
    android
    log
    EGL
    GLESv3
    atomic
)
