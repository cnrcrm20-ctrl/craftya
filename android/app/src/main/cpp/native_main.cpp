// Craftya — NativeActivity giriş noktası
// Oyun döngüsü ayrı bir iş parçacığında döner (ANA thread bloklanmaz).
#include "craftya/game.hpp"
#include "craftya/platform.hpp"

#include <android/native_activity.h>
#include <android/looper.h>
#include <android/log.h>
#include <thread>
#include <atomic>
#include <cmath>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Craftya", __VA_ARGS__)

using namespace craftya;

namespace {

std::atomic<bool> g_running{false};
std::atomic<bool> g_paused{false};
std::atomic<bool> g_surfaceReady{false};
ANativeWindow* g_window = nullptr;
std::thread g_gameThread;
Game* g_game = nullptr;
ALooper* g_looper = nullptr;
int g_inputQueuePipe[2] = {-1,-1};

// oyun döngüsü — kendi iş parçacığında
void gameLoop() {
    LOGI("oyun döngüsü başladı");
    g_game = new Game(424242, true);
    Clock clock;
    InputState in;

    while (g_running.load()) {
        float dt = clock.tick();
        if (g_paused.load() || !g_surfaceReady.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            clock.tick();  // birikmiş zamanı sıfırla
            continue;
        }

        double workStart = clock.nowSeconds();
        g_game->update(dt, in);

        // kare bütçesi: 60 FPS
        float work = static_cast<float>(clock.nowSeconds() - workStart);
        frameSleep(dt, g_game->settings().targetFps, work);
    }

    LOGI("oyun döngüsü bitti");
    delete g_game;
    g_game = nullptr;
}

void onWindowCreated(ANativeActivity* act) {
    (void)act;
    g_surfaceReady.store(true);
    LOGI("pencere oluştu");
}
void onWindowResized(ANativeActivity* act, ANativeWindow* w) {
    (void)act; (void)w;
}
void onWindowDestroyed(ANativeActivity* act) {
    (void)act;
    g_surfaceReady.store(false);
    LOGI("pencere yok edildi");
}
void onContentRectChanged(ANativeActivity*, const ARect*) {}
void onConfigurationChanged(ANativeActivity*) {}
void onLowMemory(ANativeActivity*) { LOGI("düşük bellek!"); }
void onStart(ANativeActivity*) { LOGI("onStart"); }
void onResume(ANativeActivity*) { g_paused.store(false); LOGI("onResume"); }
void onSaveInstanceState(ANativeActivity*, size_t* len) { *len = 0; }
void onPause(ANativeActivity*) { g_paused.store(true); LOGI("onPause"); }
void onStop(ANativeActivity*) { LOGI("onStop"); }
void* onSaveState(ANativeActivity*, size_t*) { return nullptr; }

void onDestroy(ANativeActivity* act) {
    (void)act;
    LOGI("onDestroy");
    g_running.store(false);
    if (g_gameThread.joinable()) g_gameThread.join();
    if (g_looper) ALooper_release(g_looper);
}

void onInputQueueCreated(ANativeActivity*, AInputQueue* queue) {
    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    AInputQueue_attachLooper(queue, looper, ALOOPER_POLL_CALLBACK, nullptr, nullptr);
    g_looper = looper;
    LOGI("girdi kuyruğu bağlandı");
}
void onInputQueueDestroyed(ANativeActivity*, AInputQueue* queue) {
    if (g_looper) AInputQueue_detachLooper(queue);
}

} // namespace

// --- JNI köprüsü: uygulama açılışında çağrılır ---
extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    (void)savedState; (void)savedStateSize;
    LOGI("Craftya ANativeActivity_onCreate");

    activity->callbacks->onStart = onStart;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onStop;
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onWindowFocusChanged = nullptr;
    activity->callbacks->onNativeWindowCreated = onWindowCreated;
    activity->callbacks->onNativeWindowResized = onWindowResized;
    activity->callbacks->onNativeWindowRedrawNeeded = nullptr;
    activity->callbacks->onNativeWindowDestroyed = onWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->callbacks->onContentRectChanged = onContentRectChanged;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    activity->callbacks->onLowMemory = onLowMemory;

    g_running.store(true);
    g_gameThread = std::thread(gameLoop);
}
