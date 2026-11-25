# Craftya — Android yerel kütüphane (libcraftya.so)
cmake_minimum_required(VERSION 3.18)
project(craftya C CXX)

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

# ---------------------------------------------------------------------------
# NativeActivity yardımcı katmanı (pencere/girdi döngüsü) — NDK ile gelir.
#
# DİKKAT: Yalnızca .c dosyası eklenmeli; NDK kökünü include yoluna eklemek
# android/input.h'i gölgeleyip "unknown type AInputEvent" hatası verir.
#
# DİKKAT: -fvisibility=hidden (yukarıdaki preset) EL İLE KALDIRILMALI.
# Bu glue dosyası Android'in uygulamayı başlatmak için aradığı
# ANativeActivity_onCreate sembolünü tanımlar. Sembol gizlenirse
# libcraftya.so yüklenirken "ANativeActivity_onCreate bulunamadı" hatası
# oluşur ve uygulama AÇILIRKEN ÇÖKER (ekran hiç gelmez).
# ---------------------------------------------------------------------------
add_library(native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue)
set_target_properties(native_app_glue PROPERTIES
        C_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF)

# Glue'nun tüm nesneleri .so'ya dahil edilsin; aksi halde statik kütüphane
# içindeki ANativeActivity_onCreate bağlanmaz ve sembol hiç var olmaz.
target_link_options(craftya PRIVATE
        "-Wl,--whole-archive" "$<TARGET_FILE:native_app_glue>" "-Wl,--no-whole-archive")

target_link_libraries(craftya
        android
        log
        EGL
        GLESv3
        m
)

# ---------------------------------------------------------------------------
# CI DOĞRULAMASI: APK'nın gerçekten açılabilir olduğunu derleme anında kanıtla.
# Sembol yoksa build BURADA durur — bir daha "çalışmayan APK" yayınlanmaz.
# ---------------------------------------------------------------------------
add_custom_command(TARGET craftya POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DLIB=$<TARGET_FILE:craftya>
                -DNM=${CMAKE_NM}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_entry.cmake
        COMMENT "libcraftya.so giris sembolu (ANativeActivity_onCreate) dogrulaniyor")
