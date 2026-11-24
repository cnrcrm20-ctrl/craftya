// Craftya Engine — platform arayüzü (masaüstü ve Android aynı API'yi kullanır)
#pragma once
#include "craftya/game.hpp"
#include <string>
#include <functional>

namespace craftya {

// Platform katmanının sağlaması gerekenler:
// 1. Pencere + yüzey (wgpu/Vulkan veya GLES)
// 2. Girdi (dokunmatik / klavye-fare)
// 3. Zaman (yüksek çözünürlüklü saat)
// 4. Ses (AAudio / WASAPI)
// 5. Dosya (asset, cache, ayar)
//
// Bu arayüz saf sanal; renderer arka ucu (wgpu) ayrı bir katmanda.

struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string title = "Craftya";
    bool fullscreen = false;
    bool vsync = true;
    bool landscape = true;      // Android'de yatay kilit
    int targetFps = 60;         // 0 = sınırsız
};

class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual bool init(const WindowConfig& cfg) = 0;
    virtual void shutdown() = 0;

    // ana döngü: her karede çağrılır, false dönerse çıkılır
    virtual bool pumpEvents(InputState& in) = 0;
    virtual float deltaSeconds() = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual float aspect() const = 0;

    // dosya sistemi (Android'de APK içi vs. farkı burada gizlenir)
    virtual bool readFile(const std::string& path, std::vector<uint8_t>& out) = 0;
    virtual bool writeFile(const std::string& path, const std::vector<uint8_t>& data) = 0;
    virtual std::string assetRoot() const = 0;
    virtual std::string cacheRoot() const = 0;

    // yaşam döngüsü olayları (Android'de zorunlu)
    virtual bool isPaused() const = 0;
    virtual void requestQuit() = 0;

    // bildirim / izin (güncelleme, pil muafiyeti)
    virtual void toast(const std::string& msg) { (void)msg; }
};

// Masaüstü için saat
class Clock {
public:
    Clock();
    float tick();          // son çağrıdan bu yana saniye
    double nowSeconds() const;
private:
    double last_ = 0.0;
};

// 60 FPS hedefi için uyku (pil tasarrufu + ısınma kontrolü)
void frameSleep(float frameSeconds, int targetFps, float workSeconds);

} // namespace craftya
