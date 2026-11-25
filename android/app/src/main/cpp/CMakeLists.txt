# Craftya — Android yerel kütüphane (libcraftya.so)
cmake_minimum_required(VERSION 3.18)
project(craftya C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fno-math-errno")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine)
set(GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)

# ---------------------------------------------------------------------------
# NativeActivity yardımcı katmanı (pencere/girdi döngüsü).
#
# KÖK NEDEN NOTU: Bu dosya Android'in uygulamayı başlatmak için aradığı
# ANativeActivity_onCreate sembolünü tanımlar. Daha önce ayrı bir STATIC
# kütüphane olarak derleniyordu; yukarıdaki -fvisibility=hidden preset'i
# yüzünden sembol gizleniyor ve uygulama AÇILIRKEN ÇÖKÜYORDU.
#
# Bu yüzden glue dosyası doğrudan hedefe kaynak olarak ekleniyor ve SADECE
# onun için -fvisibility=default veriliyor. Böylece:
#   * header yolu (android_native_app_glue.h) doğru şekilde bulunur,
#   * sembol .so'nun dinamik tablosuna çıkar,
#   * statik kütüphane / --whole-archive karmaşası gerekmez.
#
# DİKKAT: NDK kökünü include yoluna eklemeyin; android/input.h'i gölgeler.
# ---------------------------------------------------------------------------
set(GLUE_SRC ${GLUE_DIR}/android_native_app_glue.c)
set_source_files_properties(${GLUE_SRC} PROPERTIES
        COMPILE_OPTIONS "-fvisibility=default")

add_library(craftya SHARED
        android_main.cpp
        gles_backend.cpp
        ui_renderer.cpp
        ${GLUE_SRC}
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

target_include_directories(craftya PRIVATE
        ${ENGINE_DIR}/include
        ${GLUE_DIR})

target_link_libraries(craftya
        android
        log
        EGL
        GLESv3
        m
)

# ---------------------------------------------------------------------------
# CI/yerel DOĞRULAMA: APK'nın gerçekten açılabilir olduğunu derleme anında
# kanıtla. Sembol yoksa build BURADA durur — bir daha "çalışmayan APK"
# yayınlanmaz.
# ---------------------------------------------------------------------------
add_custom_command(TARGET craftya POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DLIB=$<TARGET_FILE:craftya>
                -DNM=${CMAKE_NM}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_entry.cmake
        COMMENT "libcraftya.so giris sembolu (ANativeActivity_onCreate) dogrulaniyor")
