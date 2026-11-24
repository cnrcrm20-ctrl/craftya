// Craftya Engine — ses motoru: mikser, konumsal ses, prosedürel SFX
// Platformdan bağımsız; AAudio/OpenSL/WASAPI arka uçları bunu besler.
#pragma once
#include "craftya/core.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace craftya {

enum class SfxId : uint16_t {
    Step = 0, Break, Place, Hit, Jump, Land, UiClick, Craft, WaterSplash,
    ZombieGrowl, ArrowShoot, Hurt, Eat, DoorOpen, COUNT
};

// 16-bit mono PCM tampon
struct AudioBuffer {
    std::vector<int16_t> samples;
    int sampleRate = 48000;
    float durationSec = 0.f;
    bool empty() const { return samples.empty(); }
};

// prosedürel ses üreticisi (asset dosyası olmadan da ses çıkar)
class SfxGenerator {
public:
    static AudioBuffer generate(SfxId id, int sampleRate = 48000);

private:
    static void envelope(std::vector<int16_t>& s, float attack, float decay, int sr);
    static AudioBuffer noiseBurst(float dur, float freqLow, float freqHigh,
                                  float gain, int sr, uint64_t seed);
    static AudioBuffer tone(float dur, float freqStart, float freqEnd,
                            float gain, int sr, bool square = false);
};

// Ses kanalı
struct AudioChannel {
    AudioBuffer buffer;
    float volume = 1.f;
    float pan = 0.f;            // -1 sol, +1 sağ
    bool loop = false;
    bool playing = false;
    int position = 0;           // örnek indeksi
    Vec3f worldPos;             // 3B konum (konumsal ses)
    float fadeOut = 0.f;
};

// Mikser
class AudioMixer {
public:
    explicit AudioMixer(int sampleRate = 48000) : sampleRate_(sampleRate) {}

    void setMasterVolume(float v) { master_ = clamp01(v); }
    void setMusicVolume(float v) { music_ = clamp01(v); }
    void setSfxVolume(float v) { sfx_ = clamp01(v); }

    float masterVolume() const { return master_; }
    float musicVolume() const { return music_; }
    float sfxVolume() const { return sfx_; }

    // sesi başlat, kanal indeksi döner (-1 = kanal yok)
    int play(const AudioBuffer& buf, float volume = 1.f, bool loop = false);
    // konumsal ses: dinleyiciye mesafeye göre ses/pan hesaplanır
    int playAt(const AudioBuffer& buf, const Vec3f& pos,
               const Vec3f& listenerPos, const Vec3f& listenerForward,
               float volume = 1.f);

    void stop(int channel);
    void stopAll();
    void setListener(const Vec3f& pos, const Vec3f& forward);

    // her karede çağrılır: karıştırılmış örnekleri üretir
    // out: interleaved stereo (L,R,L,R,...)
    void mix(int16_t* out, int frames);

    int activeChannels() const;
    int sampleRate() const { return sampleRate_; }

    // önceden üretilmiş SFX kütüphanesi
    void loadProceduralSfx();
    const AudioBuffer& sfx(SfxId id) const;

private:
    static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
    int sampleRate_ = 48000;
    float master_ = 1.f, music_ = 0.6f, sfx_ = 0.9f;
    Vec3f listenerPos_{0, 0, 0};
    Vec3f listenerFwd_{0, 0, -1};
    std::vector<AudioChannel> channels_;
    std::vector<AudioBuffer> sfxLib_;
};

// Müzik: basit prosedürel ambient döngü (lobi için)
class AmbientMusic {
public:
    static AudioBuffer generate(int sampleRate, float durationSec, uint64_t seed = 7);
};

} // namespace craftya
