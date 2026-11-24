// Craftya Engine — hostsuz P2P mesh ağ katmanı
// Tasarım: sunucu yok, host telefon yok. Her cihaz her cihaza doğrudan paket atar.
// Bu katman WebRTC DataChannel / UDP / TCP üstünde çalışacak şekilde soyutlanmıştır.
#pragma once
#include "craftya/core.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <functional>
#include <cstring>

namespace craftya {

// ---------------- paket türleri ----------------
enum class PacketType : uint8_t {
    Hello = 1,        // el sıkışma: sürüm + oda + oyuncu adı
    HelloAck,         // karşılık
    PlayerState,      // 20 Hz konum/hareket
    PlayerInput,      // tuş girdisi (opsiyonel, gecikme düşürme)
    BlockEdit,        // blok kırma/koyma
    EntitySpawn,
    EntityState,
    EntityDespawn,
    Chat,
    Ping,
    Pong,
    SyncRequest,      // geç katılan: dünya özeti iste
    SyncResponse,     // dünya özeti (blok delta listesi)
    MatchState,       // maç skoru/faz
    Disconnect
};

struct PacketHeader {
    uint8_t type = 0;
    uint8_t channel = 0;      // 0=güvenilir-sıralı, 1=güvenilmez-hızlı
    uint16_t seq = 0;
    uint32_t ack = 0;         // en son görülen seq
    uint32_t senderTick = 0;
    uint8_t senderId = 0;
};

// paket yazıcı (little-endian, sıkıştırma yok — basit ve hızlı)
class PacketWriter {
public:
    explicit PacketWriter(PacketType t) { buf_.reserve(64); u8(static_cast<uint8_t>(t)); }
    void u8(uint8_t v) { buf_.push_back(v); }
    void u16(uint16_t v) { buf_.push_back(v & 0xFF); buf_.push_back((v >> 8) & 0xFF); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) buf_.push_back((v >> (i*8)) & 0xFF); }
    void i16(int16_t v) { u16(static_cast<uint16_t>(v)); }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void f32(float v) { uint32_t b; std::memcpy(&b, &v, 4); u32(b); }
    void str(const std::string& s) { u16(static_cast<uint16_t>(s.size())); buf_.insert(buf_.end(), s.begin(), s.end()); }
    void bytes(const uint8_t* p, size_t n) { buf_.insert(buf_.end(), p, p + n); }

    std::vector<uint8_t>& data() { return buf_; }
    size_t size() const { return buf_.size(); }

private:
    std::vector<uint8_t> buf_;
};

class PacketReader {
public:
    PacketReader(const uint8_t* d, size_t n) : d_(d), n_(n) {}
    explicit PacketReader(const std::vector<uint8_t>& v) : d_(v.data()), n_(v.size()) {}

    bool ok() const { return n_ > 0; }
    bool exhausted() const { return pos_ >= n_; }
    PacketType type() const { return static_cast<PacketType>(d_[0]); }

    uint8_t u8() { if (pos_ + 1 > n_) { bad_ = true; return 0; } return d_[pos_++]; }
    uint16_t u16() { if (pos_ + 2 > n_) { bad_ = true; return 0; }
        uint16_t v = d_[pos_] | (d_[pos_+1] << 8); pos_ += 2; return v; }
    uint32_t u32() { if (pos_ + 4 > n_) { bad_ = true; return 0; }
        uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(d_[pos_+i]) << (i*8);
        pos_ += 4; return v; }
    float f32() { uint32_t b = u32(); float f; std::memcpy(&f, &b, 4); return f; }
    std::string str() { uint16_t len = u16(); if (pos_ + len > n_) { bad_ = true; return {}; }
        std::string s(reinterpret_cast<const char*>(d_ + pos_), len); pos_ += len; return s; }

    bool failed() const { return bad_; }

private:
    const uint8_t* d_ = nullptr;
    size_t n_ = 0, pos_ = 0;
    bool bad_ = false;
};

// ---------------- uzak oyuncu durumu ----------------
struct PlayerSnapshot {
    Vec3f pos{0, 0, 0};
    float yaw = 0.f, pitch = 0.f;
    uint8_t flags = 0;        // 1=sprint 2=sneak 4=swim 8=onGround 16=alive
    float health = 20.f;
    uint32_t tick = 0;
};

struct RemotePlayer {
    uint8_t deviceId = 0;
    std::string name;
    std::string publicKey;    // kimlik doğrulama (imza ile)
    PlayerSnapshot current;
    PlayerSnapshot prev;      // interpolasyon için
    float interpTime = 0.f;
    uint32_t lastRecvMs = 0;
    float rttMs = 0.f;
    uint8_t packetLossPct = 0;
    bool connected = false;
    bool isBot = false;
    // istemci-taraflı tahmin: uzak oyunun yerel simülasyonu
    Vec3f predictedPos{0, 0, 0};
    Vec3f predictionError{0, 0, 0};
};

// ---------------- mesh peer ----------------
class PeerLink {
public:
    // soyut gönderim: WebRTC DC veya UDP soketi
    using SendFn = std::function<bool(const uint8_t* data, size_t len)>;

    PeerLink() = default;
    PeerLink(uint8_t deviceId, SendFn send) : deviceId_(deviceId), send_(std::move(send)) {}

    uint8_t deviceId() const { return deviceId_; }
    void setSend(SendFn fn) { send_ = std::move(fn); }

    bool send(PacketWriter& w) {
        if (!send_) return false;
        stats_.bytesSent += w.size();
        stats_.packetsSent++;
        return send_(w.data().data(), w.size());
    }

    struct Stats {
        uint64_t bytesSent = 0, bytesRecv = 0;
        uint32_t packetsSent = 0, packetsRecv = 0, lost = 0;
        float rttMs = 0.f;
    };
    const Stats& stats() const { return stats_; }
    Stats& stats() { return stats_; }

private:
    uint8_t deviceId_ = 0;
    SendFn send_;
    Stats stats_;
};

// ---------------- mesh yöneticisi ----------------
class MeshNetwork {
public:
    static constexpr int kMaxPeers = 16;         // Mega modu 12 + yedek
    static constexpr float kSnapshotHz = 20.f;
    static constexpr float kTimeoutSec = 5.f;

    void setLocalDevice(uint8_t id) { localId_ = id; }
    uint8_t localDevice() const { return localId_; }

    // peer ekle/çıkar
    PeerLink* addPeer(uint8_t deviceId, PeerLink::SendFn send);
    void removePeer(uint8_t deviceId);
    PeerLink* peer(uint8_t deviceId);
    size_t peerCount() const { return peers_.size(); }
    const std::map<uint8_t, PeerLink>& peers() const { return peers_; }

    // tüm peerlere yayın (hostsuz: herkes herkese)
    void broadcast(PacketWriter& w, uint8_t exceptDevice = 0xFF);
    // tek peer'e
    bool sendTo(uint8_t deviceId, PacketWriter& w);

    // gelen paket işleme — tür bazlı geri çağrılar
    using PlayerStateCb = std::function<void(uint8_t deviceId, const PlayerSnapshot&)>;
    using BlockEditCb = std::function<void(uint8_t deviceId, int x, int y, int z, uint8_t block, bool place)>;
    using ChatCb = std::function<void(uint8_t deviceId, const std::string&)>;
    using MatchStateCb = std::function<void(uint8_t from, uint16_t scoreA, uint16_t scoreB, uint8_t phase)>;

    void onPlayerState(PlayerStateCb cb) { playerStateCb_ = std::move(cb); }
    void onBlockEdit(BlockEditCb cb) { blockEditCb_ = std::move(cb); }
    void onChat(ChatCb cb) { chatCb_ = std::move(cb); }
    void onMatchState(MatchStateCb cb) { matchStateCb_ = std::move(cb); }

    void handlePacket(uint8_t fromDevice, const uint8_t* data, size_t len, uint32_t nowMs);

    // her karede: zaman aşımı, interpolasyon, ping
    void update(float dt, uint32_t nowMs);

    // yerel durumu 20 Hz yayınla
    void publishLocalState(const PlayerSnapshot& snap, uint32_t nowMs);

    // uzak oyuncular (interpole edilmiş konumla)
    const std::vector<RemotePlayer>& remotePlayers() const { return remotes_; }
    RemotePlayer* remote(uint8_t deviceId);

    void sendBlockEdit(int x, int y, int z, uint8_t block, bool place);
    void sendChat(const std::string& msg);
    void sendMatchState(uint16_t a, uint16_t b, uint8_t phase);

    struct NetStats {
        uint64_t totalBytesSent = 0, totalBytesRecv = 0;
        uint32_t totalPackets = 0, dropped = 0;
        float avgRttMs = 0.f;
        float estimatedKbps = 0.f;
    };
    const NetStats& stats() const { return netStats_; }
    void resetStatsTotals();

private:
    uint8_t localId_ = 1;
    std::map<uint8_t, PeerLink> peers_;
    std::vector<RemotePlayer> remotes_;
    uint16_t nextSeq_ = 1;
    float publishAccum_ = 0.f;
    float pingAccum_ = 0.f;
    uint32_t pingTimestamp_ = 0;
    NetStats netStats_;

    PlayerStateCb playerStateCb_;
    BlockEditCb blockEditCb_;
    ChatCb chatCb_;
    MatchStateCb matchStateCb_;
};

// ---------------- basit oda/oturum ----------------
struct RoomInfo {
    uint32_t roomId = 0;
    uint8_t mode = 0;            // 0=düello 1=takımlı 2=mega
    uint8_t expectedPlayers = 2;
    uint8_t joinedPlayers = 0;
    uint64_t createdAtMs = 0;
    uint32_t seed = 0;
};

} // namespace craftya
