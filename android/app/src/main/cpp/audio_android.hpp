// Craftya — Android ses çıkışı (ince katman)
#pragma once
#include <cstdint>
#include <atomic>

namespace craftya {

class AndroidAudio {
public:
    // karıştırma fonksiyonu: stereo interleaved int16 üretir
    using MixFn = void (*)(int16_t* out, int frames);

    AndroidAudio();
    ~AndroidAudio();

    bool start(MixFn mix, int sampleRate = 48000);
    void shutdown();
    bool running() const { return running_.load(); }

    // AAudio callback'i buradan erişir (opak tutulur)
    struct Impl;

private:
    Impl* impl_ = nullptr;
    std::atomic<bool> running_{false};
};

} // namespace craftya
