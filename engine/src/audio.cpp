// Craftya Engine — ses üretimi ve miks
#include "craftya/audio.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace craftya {

namespace {
constexpr float PI = 3.14159265358979f;

uint32_t hashU32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
float randFloat(uint32_t& state) {
    state = hashU32(state + 0x9e3779b9U);
    return (state & 0xFFFFFF) / 16777216.0f;
}
}

void SfxGenerator::envelope(std::vector<int16_t>& s, float attack, float decay, int sr) {
    int a = static_cast<int>(attack * sr);
    int d = static_cast<int>(decay * sr);
    int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i) {
        float env = 1.f;
        if (i < a && a > 0) env = static_cast<float>(i) / a;
        else if (i > n - d && d > 0) env = static_cast<float>(n - i) / d;
        s[i] = static_cast<int16_t>(s[i] * env);
    }
}

AudioBuffer SfxGenerator::noiseBurst(float dur, float freqLow, float freqHigh,
                                     float gain, int sr, uint64_t seed) {
    AudioBuffer out;
    out.sampleRate = sr;
    out.durationSec = dur;
    int n = static_cast<int>(dur * sr);
    out.samples.resize(n);

    uint32_t st = static_cast<uint32_t>(seed);
    // tek kutuplu düşük geçiren basit filtre
    float lp = 0.f;
    float alpha = 0.25f;
    for (int i = 0; i < n; ++i) {
        float r = randFloat(st) * 2.f - 1.f;
        lp += alpha * (r - lp);
        // frekans bandı: yüksek frekansları hafifçe kırp
        float t = static_cast<float>(i) / static_cast<float>(n);
        float band = freqLow + (freqHigh - freqLow) * (1.f - t);
        float v = lp * (band / 20000.f + 0.2f);
        out.samples[i] = static_cast<int16_t>(std::clamp(v * gain * 32000.f, -32000.f, 32000.f));
    }
    envelope(out.samples, 0.002f, dur * 0.6f, sr);
    return out;
}

AudioBuffer SfxGenerator::tone(float dur, float f0, float f1, float gain, int sr, bool square) {
    AudioBuffer out;
    out.sampleRate = sr;
    out.durationSec = dur;
    int n = static_cast<int>(dur * sr);
    out.samples.resize(n);
    float phase = 0.f;
    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(n);
        float f = f0 + (f1 - f0) * t;
        phase += 2.f * PI * f / static_cast<float>(sr);
        float v = std::sin(phase);
        if (square) v = v > 0 ? 1.f : -1.f;
        out.samples[i] = static_cast<int16_t>(std::clamp(v * gain * 30000.f, -30000.f, 30000.f));
    }
    envelope(out.samples, 0.005f, dur * 0.5f, sr);
    return out;
}

AudioBuffer SfxGenerator::generate(SfxId id, int sr) {
    switch (id) {
        case SfxId::Step:      return noiseBurst(0.09f, 800.f, 2500.f, 0.35f, sr, 11);
        case SfxId::Break:     return noiseBurst(0.22f, 400.f, 5000.f, 0.55f, sr, 22);
        case SfxId::Place:     return noiseBurst(0.14f, 300.f, 1800.f, 0.5f, sr, 33);
        case SfxId::Hit:       return noiseBurst(0.10f, 900.f, 4000.f, 0.45f, sr, 44);
        case SfxId::Jump:      return tone(0.13f, 380.f, 620.f, 0.30f, sr);
        case SfxId::Land:      return noiseBurst(0.12f, 200.f, 1200.f, 0.45f, sr, 55);
        case SfxId::UiClick:   return tone(0.05f, 1100.f, 900.f, 0.28f, sr);
        case SfxId::Craft:     return tone(0.18f, 520.f, 780.f, 0.32f, sr);
        case SfxId::WaterSplash: return noiseBurst(0.30f, 150.f, 2500.f, 0.5f, sr, 66);
        case SfxId::ZombieGrowl: return tone(0.55f, 140.f, 80.f, 0.35f, sr, true);
        case SfxId::ArrowShoot: return noiseBurst(0.16f, 1200.f, 3500.f, 0.35f, sr, 77);
        case SfxId::Hurt:      return tone(0.20f, 300.f, 160.f, 0.40f, sr);
        case SfxId::Eat:       return noiseBurst(0.25f, 500.f, 2200.f, 0.4f, sr, 88);
        case SfxId::DoorOpen:  return tone(0.35f, 200.f, 320.f, 0.3f, sr, true);
        default:               return tone(0.1f, 440.f, 440.f, 0.2f, sr);
    }
}

// ---------------- AudioMixer ----------------
int AudioMixer::play(const AudioBuffer& buf, float volume, bool loop) {
    if (buf.empty()) return -1;
    AudioChannel ch;
    ch.buffer = buf;
    ch.volume = std::clamp(volume, 0.f, 1.f);
    ch.loop = loop;
    ch.playing = true;
    ch.position = 0;

    for (size_t i = 0; i < channels_.size(); ++i) {
        if (!channels_[i].playing) {
            channels_[i] = ch;
            return static_cast<int>(i);
        }
    }
    if (channels_.size() >= 32) return -1;   // kanal tavanı
    channels_.push_back(ch);
    return static_cast<int>(channels_.size() - 1);
}

int AudioMixer::playAt(const AudioBuffer& buf, const Vec3f& pos,
                       const Vec3f& listenerPos, const Vec3f& listenerFwd,
                       float volume) {
    Vec3f toSound = pos - listenerPos;
    float dist = toSound.length();
    // 32 blok ötesinde duyulmaz
    if (dist > 32.f) return -1;
    float atten = 1.f - (dist / 32.f);
    atten *= atten;

    // pan: sağ vektörüne izdüşüm
    Vec3f right = listenerFwd.cross(Vec3f{0, 1, 0}).normalized();
    float pan = 0.f;
    if (dist > 0.01f) pan = std::clamp((toSound.x * right.x + toSound.z * right.z) / dist, -1.f, 1.f);

    int ch = play(buf, volume * atten, false);
    if (ch >= 0) {
        channels_[ch].pan = pan * 0.8f;
        channels_[ch].worldPos = pos;
    }
    return ch;
}

void AudioMixer::stop(int channel) {
    if (channel >= 0 && channel < static_cast<int>(channels_.size()))
        channels_[channel].playing = false;
}

void AudioMixer::stopAll() {
    for (auto& c : channels_) c.playing = false;
}

void AudioMixer::setListener(const Vec3f& pos, const Vec3f& forward) {
    listenerPos_ = pos;
    listenerFwd_ = forward.normalized();
}

void AudioMixer::mix(int16_t* out, int frames) {
    std::fill(out, out + frames * 2, static_cast<int16_t>(0));

    for (auto& ch : channels_) {
        if (!ch.playing || ch.buffer.empty()) continue;
        int n = static_cast<int>(ch.buffer.samples.size());
        // pan kanunları (constant power)
        float pan = std::clamp(ch.pan, -1.f, 1.f);
        float ang = (pan + 1.f) * 0.25f * PI;
        float gl = std::cos(ang);
        float gr = std::sin(ang);
        float gain = ch.volume * master_ * sfx_;

        for (int i = 0; i < frames; ++i) {
            if (ch.position >= n) {
                if (ch.loop) ch.position = 0;
                else { ch.playing = false; break; }
            }
            float s = static_cast<float>(ch.buffer.samples[ch.position]) / 32768.f;
            float l = out[i*2]     / 32768.f + s * gl * gain;
            float r = out[i*2 + 1] / 32768.f + s * gr * gain;
            out[i*2]     = static_cast<int16_t>(std::clamp(l, -1.f, 1.f) * 32767.f);
            out[i*2 + 1] = static_cast<int16_t>(std::clamp(r, -1.f, 1.f) * 32767.f);
            ++ch.position;
        }
    }
}

int AudioMixer::activeChannels() const {
    int n = 0;
    for (auto& c : channels_) if (c.playing) ++n;
    return n;
}

void AudioMixer::loadProceduralSfx() {
    sfxLib_.clear();
    sfxLib_.resize(static_cast<size_t>(SfxId::COUNT));
    for (int i = 0; i < static_cast<int>(SfxId::COUNT); ++i)
        sfxLib_[i] = SfxGenerator::generate(static_cast<SfxId>(i), sampleRate_);
}

const AudioBuffer& AudioMixer::sfx(SfxId id) const {
    static AudioBuffer empty;
    size_t i = static_cast<size_t>(id);
    if (i >= sfxLib_.size()) return empty;
    return sfxLib_[i];
}

// ---------------- ambient müzik ----------------
AudioBuffer AmbientMusic::generate(int sr, float dur, uint64_t seed) {
    AudioBuffer out;
    out.sampleRate = sr;
    out.durationSec = dur;
    int n = static_cast<int>(dur * sr);
    out.samples.resize(n, 0);

    // pentatonik pad: 3 katman sinüs + yavaş LFO
    const float freqs[3] = {110.f, 165.f, 220.f};   // A2, E3, A3
    const float phases[3] = {0.f, 1.2f, 2.5f};
    uint32_t st = static_cast<uint32_t>(seed);

    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sr);
        float lfo = std::sin(2.f * PI * 0.06f * t) * 0.5f + 0.5f;
        float v = 0.f;
        for (int k = 0; k < 3; ++k) {
            v += std::sin(2.f * PI * freqs[k] * t + phases[k]) * (0.35f + 0.25f * lfo);
        }
        // hafif gürültü dokusu (rüzgâr)
        v += (randFloat(st) - 0.5f) * 0.04f;
        v *= 0.18f;
        out.samples[i] = static_cast<int16_t>(std::clamp(v, -1.f, 1.f) * 30000.f);
    }
    return out;
}

} // namespace craftya
