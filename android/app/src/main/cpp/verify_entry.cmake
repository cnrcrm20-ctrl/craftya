# libcraftya.so "açılabilir mi?" doğrulaması.
#
# Android, uygulamayı başlatmak için kütüphanede ANativeActivity_onCreate
# sembolünü arar. Sembol dinamik tabloda yoksa libcraftya.so yüklenemez ve
# uygulama açılırken çöker (ekran hiç gelmez). Bu, sahada bildirilen
# "APK'ların hiçbiri çalışmıyor" durumunun en olası kök nedenidir.
#
# Sembolü gizleyen iki yapılandırma hatası:
#   1) CMAKE_C_VISIBILITY_PRESET hidden ile derlenen glue dosyası
#   2) statik kütüphanenin --whole-archive olmadan bağlanması
#
# Bu betik ikisini de yakalar ve derlemeyi durdurur.

if(NOT EXISTS "${LIB}")
    message(FATAL_ERROR "libcraftya.so bulunamadi: ${LIB}")
endif()

set(NM_BIN "${NM}")
if(NOT NM_BIN OR NOT EXISTS "${NM_BIN}")
    find_program(NM_BIN NAMES llvm-nm nm aarch64-linux-android-nm)
endif()
if(NOT NM_BIN)
    message(WARNING "nm bulunamadi — giris sembolu dogrulanamadi")
    return()
endif()

execute_process(
    COMMAND "${NM_BIN}" -D --defined-only "${LIB}"
    OUTPUT_VARIABLE NM_OUT
    ERROR_VARIABLE NM_ERR
    RESULT_VARIABLE NM_RC)

if(NOT NM_RC EQUAL 0)
    # bazi nm surumleri -D desteklemez; normal ciktiyla dene
    execute_process(
        COMMAND "${NM_BIN}" --defined-only "${LIB}"
        OUTPUT_VARIABLE NM_OUT
        ERROR_VARIABLE NM_ERR
        RESULT_VARIABLE NM_RC)
endif()

if(NOT NM_RC EQUAL 0)
    message(FATAL_ERROR "nm basarisiz (${NM_RC}): ${NM_ERR}")
endif()

string(FIND "${NM_OUT}" "ANativeActivity_onCreate" POS)
if(POS EQUAL -1)
    message(FATAL_ERROR
        "ANativeActivity_onCreate sembolu libcraftya.so icinde DISARI ACILMAMIS.\n"
        "Bu APK acilirken coker: Android giris noktasini bulamaz.\n"
        "Cozum: native_app_glue'yu C_VISIBILITY_PRESET default ile derle ve\n"
        "--whole-archive ile bagla (bkz. CMakeLists.txt).")
endif()

message(STATUS "giris sembolu dogrulandi: ANativeActivity_onCreate mevcut")
