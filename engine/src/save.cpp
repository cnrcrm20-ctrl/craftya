// Craftya Engine — kayıt/yükleme + ayarlar
#include "craftya/save.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace craftya {

namespace {
template <typename T> void push(std::vector<uint8_t>& v, T x) {
    uint8_t* p = reinterpret_cast<uint8_t*>(&x);
    v.insert(v.end(), p, p + sizeof(T));
}
template <typename T> T read(const std::vector<uint8_t>& v, size_t& pos) {
    T x{};
    if (pos + sizeof(T) <= v.size()) std::memcpy(&x, v.data() + pos, sizeof(T));
    pos += sizeof(T);
    return x;
}
}

// ---------------- RLE ----------------
std::vector<uint8_t> WorldSave::rleCompress(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> out;
    out.reserve(in.size() / 2 + 2);
    size_t i = 0;
    while (i < in.size()) {
        uint8_t val = in[i];
        size_t run = 1;
        while (i + run < in.size() && in[i + run] == val && run < 255) ++run;
        // 3+ tekrar varsa RLE, yoksa literal
        if (run >= 3) {
            out.push_back(255);        // RLE işareti
            out.push_back(val);
            out.push_back(static_cast<uint8_t>(run));
            i += run;
        } else {
            size_t lit = 0;
            while (i + lit < in.size() && lit < 254) {
                if (i + lit + 2 < in.size() && in[i+lit] == in[i+lit+1] && in[i+lit] == in[i+lit+2]) break;
                ++lit;
            }
            out.push_back(static_cast<uint8_t>(lit));
            out.insert(out.end(), in.begin() + i, in.begin() + i + lit);
            i += lit;
        }
    }
    return out;
}

std::vector<uint8_t> WorldSave::rleDecompress(const std::vector<uint8_t>& in, size_t expected) {
    std::vector<uint8_t> out;
    out.reserve(expected);
    size_t i = 0;
    while (i < in.size() && out.size() < expected) {
        uint8_t tag = in[i++];
        if (tag == 255) {
            if (i + 1 >= in.size()) break;
            uint8_t val = in[i++];
            uint8_t run = in[i++];
            for (int k = 0; k < run && out.size() < expected; ++k) out.push_back(val);
        } else {
            for (int k = 0; k < tag && i < in.size() && out.size() < expected; ++k)
                out.push_back(in[i++]);
        }
    }
    while (out.size() < expected) out.push_back(0);
    return out;
}

// ---------------- serileştirme ----------------
std::vector<uint8_t> WorldSave::serialize(const World& world, uint32_t seed,
                                          const Vec3f& playerPos, float yaw, float pitch,
                                          const Inventory& inv,
                                          const std::vector<BlockEdit>& edits,
                                          const std::string& name) {
    std::vector<uint8_t> out;
    out.reserve(8 + 2 + 4 + 20 + 2 + name.size() + Inventory::TOTAL * 7 + 8 + edits.size() * 13);
    out.insert(out.end(), kMagic, kMagic + 8);
    push<uint16_t>(out, kSaveVersion);
    push<uint32_t>(out, seed);
    push<float>(out, playerPos.x); push<float>(out, playerPos.y); push<float>(out, playerPos.z);
    push<float>(out, yaw); push<float>(out, pitch);

    // isim
    push<uint16_t>(out, static_cast<uint16_t>(name.size()));
    out.insert(out.end(), name.begin(), name.end());

    // envanter: 45 yuva
    for (int i = 0; i < Inventory::TOTAL; ++i) {
        const ItemStack& s = inv.at(i);
        push<uint16_t>(out, s.itemId);
        push<uint8_t>(out, s.count);
        push<float>(out, s.durability);
    }
    push<uint8_t>(out, static_cast<uint8_t>(inv.selectedIndex()));

    // blok düzenlemeleri (delta — dünyanın tamamı yerine)
    (void)world;
    push<uint32_t>(out, static_cast<uint32_t>(edits.size()));
    for (const auto& e : edits) {
        push<int32_t>(out, e.x); push<int32_t>(out, e.y); push<int32_t>(out, e.z);
        push<uint8_t>(out, e.block);
    }
    return out;
}

WorldSave::Loaded WorldSave::deserialize(const std::vector<uint8_t>& blob) {
    Loaded L;
    if (blob.size() < 8 + 2 + 4 + 20) return L;
    if (std::memcmp(blob.data(), kMagic, 8) != 0) return L;

    size_t pos = 8;
    uint16_t ver = read<uint16_t>(blob, pos);
    if (ver != kSaveVersion) return L;
    L.seed = read<uint32_t>(blob, pos);
    L.playerPos.x = read<float>(blob, pos);
    L.playerPos.y = read<float>(blob, pos);
    L.playerPos.z = read<float>(blob, pos);
    L.yaw = read<float>(blob, pos);
    L.pitch = read<float>(blob, pos);

    uint16_t nameLen = read<uint16_t>(blob, pos);
    if (pos + nameLen > blob.size()) return L;
    L.name.assign(reinterpret_cast<const char*>(blob.data() + pos), nameLen);
    pos += nameLen;

    for (int i = 0; i < Inventory::TOTAL; ++i) {
        uint16_t id = read<uint16_t>(blob, pos);
        uint8_t cnt = read<uint8_t>(blob, pos);
        float dur = read<float>(blob, pos);
        L.inv.at(i) = ItemStack{id, cnt, dur};
    }
    uint8_t sel = read<uint8_t>(blob, pos);
    L.inv.select(sel % Inventory::HOTBAR);

    uint32_t nEdits = read<uint32_t>(blob, pos);
    if (nEdits > 4000000) return L;   // bozuk dosya koruması
    L.edits.reserve(nEdits);
    for (uint32_t i = 0; i < nEdits; ++i) {
        BlockEdit e;
        e.x = read<int32_t>(blob, pos);
        e.y = read<int32_t>(blob, pos);
        e.z = read<int32_t>(blob, pos);
        e.block = read<uint8_t>(blob, pos);
        L.edits.push_back(e);
    }
    L.valid = true;
    return L;
}

bool WorldSave::applyEdits(World& world, const std::vector<BlockEdit>& edits) {
    for (const auto& e : edits) world.setBlock(e.x, e.y, e.z, static_cast<BlockId>(e.block));
    return true;
}

bool WorldSave::saveToFile(const std::string& path, const std::vector<uint8_t>& blob) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
    return f.good();
}

std::vector<uint8_t> WorldSave::loadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    if (sz <= 0) return {};
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

// ---------------- ayarlar ----------------
GameSettings GameSettings::loadDefaults() { return GameSettings{}; }

std::string GameSettings::toJson() const {
    std::ostringstream o;
    o << "{"
      << "\"renderDistance\":" << renderDistance << ","
      << "\"chunkProcessing\":" << chunkProcessing << ","
      << "\"shadowMapSize\":" << shadowMapSize << ","
      << "\"shadowsEnabled\":" << (shadowsEnabled ? "true" : "false") << ","
      << "\"shadowDistance\":" << shadowDistance << ","
      << "\"fogEnabled\":" << (fogEnabled ? "true" : "false") << ","
      << "\"fov\":" << fov << ","
      << "\"mouseSensitivity\":" << mouseSensitivity << ","
      << "\"fpsMode\":" << fpsMode << ","
      << "\"resolutionScale\":" << resolutionScale << ","
      << "\"masterVolume\":" << masterVolume << ","
      << "\"musicVolume\":" << musicVolume << ","
      << "\"sfxVolume\":" << sfxVolume << ","
      << "\"invertY\":" << (invertY ? "true" : "false") << ","
      << "\"showCrosshair\":" << (showCrosshair ? "true" : "false") << ","
      << "\"showFps\":" << (showFps ? "true" : "false")
      << "}";
    return o.str();
}

namespace {
float num(const std::string& j, const std::string& key, float def) {
    auto k = j.find("\"" + key + "\"");
    if (k == std::string::npos) return def;
    auto c = j.find(':', k);
    if (c == std::string::npos) return def;
    return std::strtof(j.c_str() + c + 1, nullptr);
}
bool boolean(const std::string& j, const std::string& key, bool def) {
    auto k = j.find("\"" + key + "\"");
    if (k == std::string::npos) return def;
    auto c = j.find(':', k);
    if (c == std::string::npos) return def;
    return j.compare(c + 1, 5, "true") == 0 || (j.find("true", c) == c + 1);
}
}

GameSettings GameSettings::fromJson(const std::string& j) {
    GameSettings s;
    s.renderDistance = static_cast<int>(num(j, "renderDistance", s.renderDistance));
    s.chunkProcessing = static_cast<int>(num(j, "chunkProcessing", s.chunkProcessing));
    s.shadowMapSize = static_cast<int>(num(j, "shadowMapSize", s.shadowMapSize));
    s.shadowsEnabled = boolean(j, "shadowsEnabled", s.shadowsEnabled);
    s.shadowDistance = num(j, "shadowDistance", s.shadowDistance);
    s.fogEnabled = boolean(j, "fogEnabled", s.fogEnabled);
    s.fov = num(j, "fov", s.fov);
    s.mouseSensitivity = num(j, "mouseSensitivity", s.mouseSensitivity);
    s.fpsMode = static_cast<int>(num(j, "fpsMode", static_cast<float>(s.fpsMode)));
    s.resolutionScale = num(j, "resolutionScale", s.resolutionScale);
    s.masterVolume = num(j, "masterVolume", s.masterVolume);
    s.musicVolume = num(j, "musicVolume", s.musicVolume);
    s.sfxVolume = num(j, "sfxVolume", s.sfxVolume);
    s.invertY = boolean(j, "invertY", s.invertY);
    s.showCrosshair = boolean(j, "showCrosshair", s.showCrosshair);
    s.showFps = boolean(j, "showFps", s.showFps);
    // güvenlik kırpma
    s.renderDistance = std::max(2, std::min(12, s.renderDistance));
    s.fov = std::max(50.f, std::min(110.f, s.fov));
    s.resolutionScale = std::max(0.5f, std::min(1.5f, s.resolutionScale));
    return s;
}

} // namespace craftya
