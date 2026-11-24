// Craftya Engine — hostsuz mesh ağı uygulaması
#include "craftya/net.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace craftya {

// ---------------- PeerLink ----------------
PeerLink* MeshNetwork::addPeer(uint8_t deviceId, PeerLink::SendFn send) {
    if (peers_.size() >= static_cast<size_t>(kMaxPeers) && peers_.find(deviceId) == peers_.end())
        return nullptr;
    peers_[deviceId] = PeerLink(deviceId, std::move(send));

    // uzak oyuncu kaydı
    bool found = false;
    for (auto& r : remotes_) if (r.deviceId == deviceId) { found = true; r.connected = true; }
    if (!found) {
        RemotePlayer rp;
        rp.deviceId = deviceId;
        rp.connected = true;
        remotes_.push_back(rp);
    }
    return &peers_[deviceId];
}

void MeshNetwork::removePeer(uint8_t deviceId) {
    peers_.erase(deviceId);
    for (auto& r : remotes_) if (r.deviceId == deviceId) r.connected = false;
}

PeerLink* MeshNetwork::peer(uint8_t deviceId) {
    auto it = peers_.find(deviceId);
    return it == peers_.end() ? nullptr : &it->second;
}

void MeshNetwork::broadcast(PacketWriter& w, uint8_t exceptDevice) {
    for (auto& kv : peers_) {
        if (kv.first == exceptDevice) continue;
        if (kv.second.send(w)) {
            netStats_.totalBytesSent += w.size();
            netStats_.totalPackets++;
        }
    }
}

bool MeshNetwork::sendTo(uint8_t deviceId, PacketWriter& w) {
    PeerLink* p = peer(deviceId);
    if (!p) return false;
    if (!p->send(w)) return false;
    netStats_.totalBytesSent += w.size();
    netStats_.totalPackets++;
    return true;
}

void MeshNetwork::publishLocalState(const PlayerSnapshot& snap, uint32_t nowMs) {
    PacketWriter w(PacketType::PlayerState);
    w.u8(localId_);
    w.f32(snap.pos.x); w.f32(snap.pos.y); w.f32(snap.pos.z);
    w.f32(snap.yaw); w.f32(snap.pitch);
    w.u8(snap.flags);
    w.u8(static_cast<uint8_t>(std::clamp(snap.health, 0.f, 20.f)));
    w.u32(nowMs);
    broadcast(w);
}

void MeshNetwork::handlePacket(uint8_t fromDevice, const uint8_t* data, size_t len, uint32_t nowMs) {
    if (len < 2) { netStats_.dropped++; return; }
    PacketReader r(data, len);
    uint8_t typeByte = r.u8();
    PacketType type = static_cast<PacketType>(typeByte);

    netStats_.totalBytesRecv += len;
    PeerLink* p = peer(fromDevice);
    if (p) { p->stats().bytesRecv += len; p->stats().packetsRecv++; }

    switch (type) {
        case PacketType::Hello: {
            std::string name = r.str();
            std::string key = r.str();
            if (!r.failed()) {
                for (auto& rp : remotes_) {
                    if (rp.deviceId == fromDevice) {
                        rp.name = name;
                        rp.publicKey = key;
                        rp.connected = true;
                        rp.lastRecvMs = nowMs;
                    }
                }
                // karşılık ver
                PacketWriter ack(PacketType::HelloAck);
                ack.u8(localId_);
                ack.u32(nowMs);
                sendTo(fromDevice, ack);
            }
            break;
        }
        case PacketType::HelloAck: {
            uint8_t id = r.u8();
            (void)id;
            for (auto& rp : remotes_) if (rp.deviceId == fromDevice) rp.lastRecvMs = nowMs;
            break;
        }
        case PacketType::PlayerState: {
            uint8_t id = r.u8();
            PlayerSnapshot s;
            s.pos.x = r.f32(); s.pos.y = r.f32(); s.pos.z = r.f32();
            s.yaw = r.f32(); s.pitch = r.f32();
            s.flags = r.u8();
            s.health = static_cast<float>(r.u8());
            s.tick = r.u32();
            if (r.failed()) { netStats_.dropped++; break; }

            for (auto& rp : remotes_) {
                if (rp.deviceId == id) {
                    rp.prev = rp.current;
                    rp.current = s;
                    rp.interpTime = 0.f;
                    rp.lastRecvMs = nowMs;
                    rp.connected = true;
                    // tahmin hatası
                    rp.predictionError = rp.predictedPos - s.pos;
                    if (playerStateCb_) playerStateCb_(id, s);
                }
            }
            break;
        }
        case PacketType::BlockEdit: {
            int32_t x = static_cast<int32_t>(r.u32());
            int32_t y = static_cast<int32_t>(r.u32());
            int32_t z = static_cast<int32_t>(r.u32());
            uint8_t blk = r.u8();
            bool place = r.u8() != 0;
            if (!r.failed() && blockEditCb_) blockEditCb_(fromDevice, x, y, z, blk, place);
            break;
        }
        case PacketType::Chat: {
            std::string msg = r.str();
            if (!r.failed() && chatCb_) chatCb_(fromDevice, msg);
            break;
        }
        case PacketType::MatchState: {
            uint16_t a = r.u16(), b = r.u16();
            uint8_t phase = r.u8();
            if (!r.failed() && matchStateCb_) matchStateCb_(fromDevice, a, b, phase);
            break;
        }
        case PacketType::Ping: {
            uint32_t ts = r.u32();
            PacketWriter pong(PacketType::Pong);
            pong.u32(ts);
            pong.u32(nowMs);
            sendTo(fromDevice, pong);
            break;
        }
        case PacketType::Pong: {
            uint32_t sent = r.u32();
            uint32_t now = r.u32();
            (void)now;
            if (p) {
                float rtt = static_cast<float>(nowMs - sent);
                p->stats().rttMs = rtt;
                for (auto& rp : remotes_) if (rp.deviceId == fromDevice) rp.rttMs = rtt;
            }
            break;
        }
        case PacketType::Disconnect: {
            removePeer(fromDevice);
            if (playerStateCb_) { /* üst katman oyuncuyu düşürür */ }
            break;
        }
        default: break;
    }
}

void MeshNetwork::update(float dt, uint32_t nowMs) {
    // zaman aşımı
    for (auto& rp : remotes_) {
        if (rp.connected && (nowMs - rp.lastRecvMs) > static_cast<uint32_t>(kTimeoutSec * 1000.f))
            rp.connected = false;
    }
    // interpolasyon ilerlet
    for (auto& rp : remotes_) rp.interpTime += dt;

    // ping 1 Hz
    pingAccum_ += dt;
    if (pingAccum_ >= 1.f) {
        pingAccum_ = 0.f;
        PacketWriter w(PacketType::Ping);
        w.u32(nowMs);
        broadcast(w);
        pingTimestamp_ = nowMs;
    }

    // ortalama RTT
    float sum = 0.f; int n = 0;
    for (auto& kv : peers_) if (kv.second.stats().rttMs > 0) { sum += kv.second.stats().rttMs; ++n; }
    netStats_.avgRttMs = n ? sum / n : 0.f;
    netStats_.estimatedKbps = static_cast<float>(netStats_.totalBytesSent) * 8.f / 1000.f /
                              std::max(1.f, static_cast<float>(nowMs) / 1000.f);
}

RemotePlayer* MeshNetwork::remote(uint8_t deviceId) {
    for (auto& r : remotes_) if (r.deviceId == deviceId) return &r;
    return nullptr;
}

void MeshNetwork::sendBlockEdit(int x, int y, int z, uint8_t block, bool place) {
    PacketWriter w(PacketType::BlockEdit);
    w.u32(static_cast<uint32_t>(x));
    w.u32(static_cast<uint32_t>(y));
    w.u32(static_cast<uint32_t>(z));
    w.u8(block);
    w.u8(place ? 1 : 0);
    broadcast(w);
}

void MeshNetwork::sendChat(const std::string& msg) {
    PacketWriter w(PacketType::Chat);
    w.str(msg);
    broadcast(w);
}

void MeshNetwork::sendMatchState(uint16_t a, uint16_t b, uint8_t phase) {
    PacketWriter w(PacketType::MatchState);
    w.u16(a); w.u16(b); w.u8(phase);
    broadcast(w);
}

void MeshNetwork::resetStatsTotals() {
    netStats_ = NetStats{};
    for (auto& kv : peers_) kv.second.stats() = PeerLink::Stats{};
}

} // namespace craftya
