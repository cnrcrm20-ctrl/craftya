// Craftya — Android platform katmanı (NDK, NativeActivity)
// Amaç: oyun döngüsünü Android'e bağlamak. Render arka ucu (wgpu/GLES) ayrı dosyada.
#include "craftya/platform.hpp"

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/looper.h>
#include <android/input.h>
#include <android/sensor.h>

#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <atomic>

#define LOG_TAG "Craftya"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace craftya {

// ---------------------------------------------------------------------------
// Android platform uygulaması
// ---------------------------------------------------------------------------
class AndroidPlatform : public IPlatform {
public:
    ~AndroidPlatform() override { shutdown(); }

    bool init(const WindowConfig& cfg) override {
        cfg_ = cfg;
        clock_ = Clock();
        LOGI("Craftya init: %dx%d, targetFps=%d, vsync=%d",
             cfg.width, cfg.height, cfg.targetFps, (int)cfg.vsync);

        // Dokunmatik: joystick sol yarı, bakış sağ yarı
        joystickCenterX_ = cfg.width * 0.22f;
        joystickCenterY_ = cfg.height * 0.72f;
        joystickRadius_  = cfg.width * 0.14f;
        return true;
    }

    void setNativeWindow(ANativeWindow* w) {
        window_ = w;
        if (w) {
            width_ = ANativeWindow_getWidth(w);
            height_ = ANativeWindow_getHeight(w);
            LOGI("yüzey hazır: %dx%d", width_, height_);
            surfaceReady_ = true;
        } else {
            surfaceReady_ = false;
            LOGI("yüzey kayboldu (arka plana alındı)");
        }
    }

    void shutdown() override {
        window_ = nullptr;
        surfaceReady_ = false;
    }

    bool pumpEvents(InputState& in) override {
        if (shouldQuit_) return false;

        // Android'de olaylar AInputEvent arayüzünden gelir (aşağıdaki handleInput).
        // Burada birikmiş girdiyi dışa aktar.
        in = pending_;
        pending_.lookDX = 0; pending_.lookDY = 0;
        pending_.placeBlock = false;
        pending_.breakBlock = false;
        pending_.cycleBlock = false;
        return true;
    }

    float deltaSeconds() override { return clock_.tick(); }
    int width() const override { return width_; }
    int height() const override { return height_; }
    float aspect() const override {
        return height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 16.f/9.f;
    }

    bool readFile(const std::string& path, std::vector<uint8_t>& out) override {
        // APK içi asset: AAssetManager üzerinden okunur (asset yöneticisi JNI'den gelir)
        (void)path; (void)out;
        return false;
    }
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override {
        (void)path; (void)data;
        return false;
    }
    std::string assetRoot() const override { return "assets/"; }
    std::string cacheRoot() const override { return cacheDir_; }

    bool isPaused() const override { return paused_; }
    void requestQuit() override { shouldQuit_ = true; }

    void toast(const std::string& msg) override { LOGI("toast: %s", msg.c_str()); }

    // --- yaşam döngüsü (JNI'den çağrılır) ---
    void onPause()  { paused_ = true;  LOGI("onPause"); }
    void onResume() { paused_ = false; LOGI("onResume"); }
    void setCacheDir(const std::string& d) { cacheDir_ = d; }

    // --- girdi (AInputEvent) ---
    int32_t handleInput(AInputEvent* ev) {
        int32_t type = AInputEvent_getType(ev);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(ev);
            int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
            size_t count = AMotionEvent_getPointerCount(ev);

            for (size_t i = 0; i < count; ++i) {
                float x = AMotionEvent_getX(ev, i);
                float y = AMotionEvent_getY(ev, i);
                bool leftSide = x < width_ * 0.45f;

                if (leftSide) {
                    // joystick
                    float dx = x - joystickCenterX_;
                    float dy = y - joystickCenterY_;
                    float len = std::sqrt(dx*dx + dy*dy);
                    if (len > joystickRadius_) len = joystickRadius_;
                    if (len > 4.f) {
                        pending_.moveX = (dx / joystickRadius_);
                        pending_.moveZ = -(dy / joystickRadius_);
                        pending_.sprint = (len > joystickRadius_ * 0.9f);
                    } else {
                        pending_.moveX = 0; pending_.moveZ = 0;
                    }
                } else {
                    // bakış
                    if (masked == AMOTION_EVENT_ACTION_MOVE && count >= 2) {
                        pending_.lookDX += x - lastLookX_;
                        pending_.lookDY += y - lastLookY_;
                    }
                    if (masked == AMOTION_EVENT_ACTION_DOWN) {
                        tapStartX_ = x; tapStartY_ = y;
                        tapStartTime_ = clock_.nowSeconds();
                    }
                    lastLookX_ = x; lastLookY_ = y;
                }
            }

            if (masked == AMOTION_EVENT_ACTION_UP ||
                masked == AMOTION_EVENT_ACTION_POINTER_UP) {
                float x = AMotionEvent_getX(ev, 0);
                double dur = clock_.nowSeconds() - tapStartTime_;
                if (dur < 0.25 && x > width_ * 0.45f) {
                    pending_.breakBlock = true;   // kısa dokunma = kır
                    tapWasLong_ = false;
                }
                pending_.moveX = 0; pending_.moveZ = 0;
            }
            // uzun basma = yerleştir
            if (masked == AMOTION_EVENT_ACTION_DOWN && pending_.lookDX == 0.f) {
                longPressTimer_ = 0.f;
            }
            return 1;
        }
        if (type == AINPUT_EVENT_TYPE_KEY) {
            int32_t code = AKeyEvent_getKeyCode(ev);
            int32_t action = AKeyEvent_getAction(ev);
            if (action == AKEY_EVENT_ACTION_DOWN) {
                if (code == AKEYCODE_BACK) { shouldQuit_ = true; return 1; }
                if (code == AKEYCODE_VOLUME_UP) { pending_.cycleBlock = true; return 1; }
                pending_.jump = true;
            } else if (action == AKEY_EVENT_ACTION_UP) {
                pending_.jump = false;
            }
            return 1;
        }
        return 0;
    }

    // uzun basma tespiti (oyun döngüsü her karede çağırır)
    void tickLongPress(float dt, bool touchDown, float x, float y) {
        if (touchDown && x > width_ * 0.45f) {
            longPressTimer_ += dt;
            if (longPressTimer_ > 0.35f && !tapWasLong_) {
                pending_.placeBlock = true;
                tapWasLong_ = true;
            }
        } else {
            longPressTimer_ = 0.f;
        }
        (void)y;
    }

private:
    WindowConfig cfg_;
    Clock clock_;
    ANativeWindow* window_ = nullptr;
    int width_ = 1280, height_ = 720;
    std::atomic<bool> shouldQuit_{false};
    std::atomic<bool> paused_{false};
    bool surfaceReady_ = false;

    InputState pending_;
    float joystickCenterX_ = 0, joystickCenterY_ = 0, joystickRadius_ = 0;
    float lastLookX_ = 0, lastLookY_ = 0;
    float tapStartX_ = 0, tapStartY_ = 0;
    double tapStartTime_ = 0;
    float longPressTimer_ = 0.f;
    bool tapWasLong_ = false;
    std::string cacheDir_;
};

} // namespace craftya
