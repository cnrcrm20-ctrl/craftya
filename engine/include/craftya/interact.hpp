// Craftya Engine — etkileşimli blok davranışları + birleşik içerik kayıt defteri
#pragma once
#include "craftya/core.hpp"
#include "craftya/items_gen.hpp"
#include "craftya/mobs_gen.hpp"
#include "craftya/inventory.hpp"
#include <string>
#include <vector>
#include <functional>

namespace craftya {

// blok durumu (kapı açık/kapalı, fırın yanıyor, sandık dolu...)
struct BlockState {
    uint8_t flags = 0;       // 1=açık 2=yanıyor 3=çalışıyor 4=güçlü
    float timer = 0.f;
    uint8_t storageId = 255; // sandık/fırın içeriği indeksi

    bool isOpen() const { return flags & 1; }
    bool isLit() const { return flags & 2; }
    bool isPowered() const { return flags & 4; }
    void toggleOpen() { flags ^= 1; }
    void setLit(bool v) { if (v) flags |= 2; else flags &= ~2; }
};

// etkileşim sonucu
struct InteractResult {
    bool handled = false;
    std::string message;       // oyuncuya gösterilecek
    bool playSound = false;
    uint8_t soundId = 0;
    bool consumedItem = false;
    float cooldown = 0.f;
};

// blok konteyneri: sandık/fırın/fıçı içeriği
struct Container {
    static constexpr int kSlots = 27;
    ItemStack slots[kSlots];
    bool dirty = false;

    void clear() { for (auto& s : slots) s.clear(); dirty = true; }
    int add(const ItemStack& stack) {
        for (auto& s : slots) {
            if (s.empty()) { s = stack; dirty = true; return 0; }
            if (s.canMergeWith(stack)) {
                int space = 64 - s.count;
                int move = std::min<int>(space, stack.count);
                s.count = static_cast<uint8_t>(s.count + move);
                dirty = true;
                return stack.count - move;
            }
        }
        return stack.count;
    }
};

// ---------------- etkileşim yöneticisi ----------------
class InteractionManager {
public:
    static constexpr int kMaxContainers = 256;

    // bloğa sağ tık / dokunma
    InteractResult activate(BlockId block, BlockState& state, const ItemStack& held);

    // kırma: doğru aletle hız çarpanı
    float breakMultiplier(BlockId block, const ItemDef* tool) const;

    // blok kırıldığında düşen eşyalar
    std::vector<ItemStack> dropsFor(BlockId block, const ItemDef* tool) const;

    // konteyner yönetimi
    Container& container(uint8_t id);
    uint8_t allocateContainer();

    // redstone benzeri basit mantık
    bool isRedstoneSource(BlockId b) const;
    void tick(float dt);

    const std::string& lastMessage() const { return lastMessage_; }

private:
    Container containers_[kMaxContainers];
    uint8_t nextContainer_ = 0;
    std::string lastMessage_;
    float globalTimer_ = 0.f;
};

// ---------------- birleşik içerik kayıt defteri ----------------
// Blok (311) + Canlı (37) + Eşya (109) = 457
class ContentRegistry {
public:
    struct Entry {
        enum class Type : uint8_t { Block, Mob, Item } type;
        uint16_t id;
        std::string key;
        std::string name;
        uint32_t color;
    };

    static const ContentRegistry& instance();

    const std::vector<Entry>& all() const { return entries_; }
    size_t blockCount() const { return blockCount_; }
    size_t mobCount() const { return mobCount_; }
    size_t itemCount() const { return itemCount_; }
    size_t total() const { return entries_.size(); }

    const Entry* find(const std::string& key) const;
    const Entry* findBlock(BlockId id) const;
    const Entry* findItem(uint16_t id) const;
    const Entry* findMob(uint16_t id) const;

    // arama (envanter/yaratıcı menü)
    std::vector<const Entry*> search(const std::string& query, size_t limit = 50) const;

private:
    ContentRegistry();
    std::vector<Entry> entries_;
    size_t blockCount_ = 0, mobCount_ = 0, itemCount_ = 0;
};

} // namespace craftya
