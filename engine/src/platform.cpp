// Craftya Engine — platform yardımcıları
#include "craftya/platform.hpp"
#include <chrono>
#include <thread>

namespace craftya {

Clock::Clock() {
    last_ = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

float Clock::tick() {
    double now = nowSeconds();
    double dt = now - last_;
    last_ = now;
    if (dt < 0.0) dt = 0.0;
    if (dt > 0.25) dt = 0.25;    // takılma sonrası büyük sıçramayı kırp
    return static_cast<float>(dt);
}

double Clock::nowSeconds() const {
    return std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void frameSleep(float frameSeconds, int targetFps, float workSeconds) {
    (void)frameSeconds;
    if (targetFps <= 0) return;
    float target = 1.0f / static_cast<float>(targetFps);
    float remain = target - workSeconds;
    if (remain > 0.0005f) {
        std::this_thread::sleep_for(
            std::chrono::duration<double>(static_cast<double>(remain)));
    }
}

} // namespace craftya
