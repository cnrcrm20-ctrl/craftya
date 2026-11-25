// Craftya — Android ses çıkışı (AAudio / OpenSL ES yerine basit kuyruk)
//
// Motor tarafında AudioMixer karıştırılmış PCM üretir; burada o PCM'i
// cihazın çıkışına yazan küçük bir oynatıcı var. Üçüncü parti bağımlılık
// YOK — yalnızca <aaudio/AAudio.h> (Android 8+, API 26) kullanılır.
// API 24/25 için sessizce devre dışı kalır; oyun çalışmaya devam eder.
#include "audio_android.hpp"

#if __ANDROID_API__ >= 26
#include <aaudio/AAudio.h>
#endif

#include <android/log.h>
#include <cstring>
#include <atomic>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "CraftyaAudio", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "CraftyaAudio", __VA_ARGS__)

namespace craftya {

struct AndroidAudio::Impl {
    void* stream = nullptr;
    AndroidAudio::MixFn mix = nullptr;
    int sampleRate = 48000;
};

AndroidAudio::AndroidAudio() : impl_(new Impl()) {}
AndroidAudio::~AndroidAudio() { shutdown(); delete impl_; }

#if __ANDROID_API__ >= 26
static aaudio_data_callback_result_t audioCb(AAudioStream* /*s*/, void* user,
                                             void* audioData, int32_t numFrames) {
    auto* impl = static_cast<AndroidAudio::Impl*>(user);
    if (!impl || !impl->mix) return AAUDIO_CALLBACK_RESULT_CONTINUE;
    int16_t* out = static_cast<int16_t*>(audioData);
    // stereo interleaved
    std::memset(out, 0, static_cast<size_t>(numFrames) * 2 * sizeof(int16_t));
    impl->mix(out, numFrames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
#endif

bool AndroidAudio::start(MixFn mixFn, int sampleRate) {
    if (running_.load()) return true;
    impl_->mix = mixFn;
    impl_->sampleRate = sampleRate > 0 ? sampleRate : 48000;

#if __ANDROID_API__ >= 26
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, impl_->sampleRate);
    AAudioStreamBuilder_setDataCallback(b, audioCb, impl_);

    AAudioStream* s = nullptr;
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &s);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK || !s) { LOGE("AAudio acilamadi: %d", (int)r); return false; }
    if (AAudioStream_requestStart(s) != AAUDIO_OK) { AAudioStream_close(s); return false; }
    impl_->stream = s;
    running_.store(true);
    LOGI("ses cikisi basladi (%d Hz)", impl_->sampleRate);
    return true;
#else
    LOGI("AAudio yok (API < 26) — ses kapali");
    return false;
#endif
}

void AndroidAudio::shutdown() {
    if (!running_.exchange(false)) return;
#if __ANDROID_API__ >= 26
    auto* s = static_cast<AAudioStream*>(impl_->stream);
    if (s) {
        AAudioStream_requestStop(s);
        AAudioStream_close(s);
    }
    impl_->stream = nullptr;
#endif
}

} // namespace craftya
