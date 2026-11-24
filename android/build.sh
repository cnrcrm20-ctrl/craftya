#!/usr/bin/env bash
# Craftya — Android APK derleme (NDK + Gradle gerekir)
set -e
cd "$(dirname "$0")"

if [ -z "$ANDROID_NDK_HOME" ]; then
  echo "✗ ANDROID_NDK_HOME ayarlı değil"
  echo "  export ANDROID_NDK_HOME=\$ANDROID_SDK_ROOT/ndk/26.1.10909125"
  exit 1
fi
if [ -z "$ANDROID_SDK_ROOT" ] && [ -z "$ANDROID_HOME" ]; then
  echo "✗ ANDROID_SDK_ROOT ayarlı değil"
  exit 1
fi

SDK=${ANDROID_SDK_ROOT:-$ANDROID_HOME}

# release imzası (keys/README.md'de parmak izleri)
export CRAFTYA_STORE_PASS=${CRAFTYA_STORE_PASS:-Craftya2026Release}
export CRAFTYA_KEY_PASS=${CRAFTYA_KEY_PASS:-$CRAFTYA_STORE_PASS}

echo "▸ APK derleniyor (arm64-v8a + armeabi-v7a)…"
./gradlew :app:assembleRelease

OUT=app/build/outputs/apk/release
if [ -f "$OUT/app-release.apk" ]; then
  SIZE=$(stat -c%s "$OUT/app-release.apk" 2>/dev/null || stat -f%z "$OUT/app-release.apk")
  echo "✓ APK: $OUT/app-release.apk  ($((SIZE/1024/1024)) MB)"
  echo "  kurulum: adb install -r $OUT/app-release.apk"
else
  echo "✗ APK bulunamadı"
  exit 1
fi
