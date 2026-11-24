# Craftya — Android yerel kütüphane (libcraftya.so)
cmake_minimum_required(VERSION 3.18)
project(craftya CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fno-math-errno")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine)

add_library(craftya SHARED
        android_main.cpp
        gles_backend.cpp
        ui_renderer.cpp
        ${ENGINE_DIR}/src/json.cpp
        ${ENGINE_DIR}/src/lighting.cpp
        ${ENGINE_DIR}/src/atlas.cpp
        ${ENGINE_DIR}/src/engine.cpp
        ${ENGINE_DIR}/src/mesher.cpp
        ${ENGINE_DIR}/src/physics.cpp
        ${ENGINE_DIR}/src/inventory.cpp
        ${ENGINE_DIR}/src/render.cpp
        ${ENGINE_DIR}/src/renderer.cpp
        ${ENGINE_DIR}/src/game.cpp
        ${ENGINE_DIR}/src/platform.cpp
        ${ENGINE_DIR}/src/audio.cpp
        ${ENGINE_DIR}/src/net.cpp
        ${ENGINE_DIR}/src/save.cpp
        ${ENGINE_DIR}/src/gui.cpp
        ${ENGINE_DIR}/src/interact.cpp
        ${ENGINE_DIR}/src/anim.cpp
        ${ENGINE_DIR}/src/ecs.cpp
)

target_include_directories(craftya PRIVATE ${ENGINE_DIR}/include)

# NativeActivity yardımcı katmanı (pencere/girdi döngüsü) — NDK ile birlikte gelir
add_library(native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(craftya
        native_app_glue
        android
        log
        EGL
        GLESv3
        m
)
