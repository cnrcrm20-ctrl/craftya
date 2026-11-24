// Craftya Engine — item, envanter, hotbar, crafting
#pragma once
#include "craftya/core.hpp"
#include "craftya/items_gen.hpp"
#include "craftya/recipes_gen.hpp"
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>

namespace craftya {

// ItemDef / ItemKind / ItemId artık items_gen.hpp'den geliyor (109 eşya).
// Uyumluluk için alan adları:
inline std::string itemName(uint16_t id) {
    const ItemDef* d = itemDef(id);
    return d ? std::string(d->name) : std::string("?");
}
inline int itemMaxStack(uint16_t id) {
    const ItemDef* d = itemDef(id);
    return d ? d->maxStack : 64;
}
// blok item'ı mı? (id 1..310 blok kimliğidir)
inline bool isBlockItem(uint16_t id) { return id > 0 && id < BLOCK_COUNT; }

// basit anahtar arama (109 eşya için lineer tarama yeterli: ~109 karşılaştırma)
class ItemRegistry {
public:
    void registerDefaults() {}
    const ItemDef* get(uint16_t id) const { return itemDef(id); }
    const ItemDef* byKey(const std::string& key) const {
        const auto& reg = itemRegistry();
        for (const auto& d : reg) if (key == d.key) return &d;
        return nullptr;
    }
    const ItemDef* blockAsItem(BlockId) const { return nullptr; }
    static const ItemDef* blockInfo(BlockId b) { (void)b; return nullptr; }
    size_t count() const { return static_cast<size_t>(kItemCount); }
};

// ---------- yığın ----------
struct ItemStack {
    uint16_t itemId = 0;   // 0 = boş
    uint8_t count = 0;
    float durability = 0.f;

    bool empty() const { return itemId == 0 || count == 0; }
    void clear() { itemId = 0; count = 0; durability = 0.f; }
    bool canMergeWith(const ItemStack& o) const {
        return !empty() && o.itemId == itemId && count < 64;
    }
};

// ---------- envanter (36 slot + 9 hotbar) ----------
class Inventory {
public:
    static constexpr int HOTBAR = 9;
    static constexpr int MAIN = 27;
    static constexpr int TOTAL = HOTBAR + MAIN;

    Inventory() { clear(); }

    void clear() {
        for (auto& s : slots_) s.clear();
        selected_ = 0;
    }

    ItemStack& at(int index) { return slots_[clampIndex(index)]; }
    const ItemStack& at(int index) const { return slots_[clampIndex(index)]; }

    ItemStack& selectedSlot() { return slots_[selected_]; }
    int selectedIndex() const { return selected_; }
    void select(int i) { selected_ = clampIndex(i); }
    void cycle(int dir) {
        selected_ = (selected_ + dir + HOTBAR) % HOTBAR;
    }

    // ekle: kalan sayıyı döndürür (yer yoksa >0)
    int add(uint16_t itemId, int count, float durability = 0.f);
    // belirli slottan al
    ItemStack take(int index, int count = 1);
    // tüm envanterde var mı
    int countOf(uint16_t itemId) const;
    bool removeItems(uint16_t itemId, int count);

    // hotbar'daki bloğu döndür (yerleştirme için)
    int selectedBlockId() const;

    bool full() const;

private:
    static int clampIndex(int i) { return i < 0 ? 0 : (i >= TOTAL ? TOTAL - 1 : i); }
    std::array<ItemStack, TOTAL> slots_;
    int selected_ = 0;
};

// ---------- crafting ----------
struct Recipe {
    std::string key;
    // 3x3 ızgara, 0 = boş, diğerleri item id
    std::array<uint16_t, 9> pattern{};
    ItemStack result;
    bool shapeless = false;
};

class CraftingRegistry {
public:
    void registerDefaults();
    void add(const Recipe& r);

    // ızgaraya bakıp sonucu bul (şekilli: kaydırma/konum toleranslı)
    const Recipe* match(const std::array<ItemStack, 9>& grid) const;

    size_t count() const { return recipes_.size(); }

    // tarif listesi (arayüzde gösterilecek)
    const std::vector<Recipe>& all() const { return recipes_; }

private:
    std::vector<Recipe> recipes_;
    static bool gridEquals(const std::array<uint16_t, 9>& a, const std::array<uint16_t, 9>& b);
    static std::array<uint16_t, 9> normalize(const std::array<uint16_t, 9>& g);
};

// 3x3 crafting ızgarası (arayüz tarafı)
struct CraftingGrid {
    std::array<ItemStack, 9> cells;

    std::array<uint16_t, 9> ids() const {
        std::array<uint16_t, 9> out{};
        for (int i = 0; i < 9; ++i) out[i] = cells[i].count ? cells[i].itemId : 0;
        return out;
    }
    void consume(const Recipe& r);
    void clear() { for (auto& c : cells) c.clear(); }
};

// ---------- kırma süresi (araç + sertlik) ----------
class Mining {
public:
    // sertlik → saniye cinsinden kırma süresi (doku stiline göre doğru alet)
    static float breakTime(BlockId block, const ItemDef* tool) {
        const BlockDef& bd = blockDef(block);
        if (bd.hardness >= 9999.f) return 1e9f;   // ana kaya kırılamaz
        float base = bd.hardness * 1.5f;
        if (!tool || !tool->toolType[0]) return base;

        // blok dokusuna göre istenen alet türü
        const char* want = "";
        switch (bd.style) {
            case kStyle_stone: case kStyle_cobble: case kStyle_brick:
            case kStyle_ore: case kStyle_smooth: case kStyle_metal:
            case kStyle_speckle: case kStyle_gem:
                want = "pickaxe"; break;
            case kStyle_log: case kStyle_plank: case kStyle_books: case kStyle_door:
                want = "axe"; break;
            case kStyle_noise: case kStyle_grass: case kStyle_grain:
                want = "shovel"; break;
            case kStyle_cross: case kStyle_leaves: case kStyle_wool:
                want = "sword"; break;
            default: want = ""; break;
        }
        if (want[0] && std::strcmp(tool->toolType, want) == 0) {
            float tierBonus = 1.f + static_cast<float>(tool->toolTier) * 0.6f;
            base /= tierBonus;
        }
        return base < 0.05f ? 0.05f : base;
    }

    // araç dayanıklılık kaybı (kırılan her blok için)
    static void wearTool(ItemStack& stack, float amount = 1.f) {
        if (stack.empty()) return;
        stack.durability -= amount;
        if (stack.durability <= 0.f) stack.clear();
    }
};

} // namespace craftya
