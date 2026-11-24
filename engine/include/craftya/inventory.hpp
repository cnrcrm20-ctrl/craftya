// Craftya Engine — item, envanter, hotbar, crafting
#pragma once
#include "craftya/core.hpp"
#include <array>
#include <vector>
#include <string>
#include <unordered_map>

namespace craftya {

// ---------- item ----------
enum class ItemKind : uint8_t { Block, Tool, Material, Food, Special };

struct ItemDef {
    uint16_t id = 0;
    std::string key;
    std::string displayName;
    ItemKind kind = ItemKind::Material;
    int blockId = -1;        // ItemKind::Block ise hangi blok
    int texIndex = 0;        // envanter ikonu (atlas indeksi)
    int maxStack = 64;
    float damage = 0.f;      // araç/silah hasarı
    float durability = 0.f;  // 0 = dayanıklılık yok
    std::string toolType;    // "pickaxe", "axe", "shovel", "sword"
    int toolTier = 0;        // 0=el, 1=tahta, 2=taş, 3=demir, 4=elmas
};

class ItemRegistry {
public:
    void registerDefaults();
    void add(const ItemDef& def);
    const ItemDef* get(uint16_t id) const;
    const ItemDef* byKey(const std::string& key) const;
    size_t count() const { return items_.size(); }

private:
    std::vector<ItemDef> items_;
    std::unordered_map<std::string, uint16_t> byKey_;
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
    // sertlik → saniye cinsinden kırma süresi
    static float breakTime(BlockId block, const ItemDef* tool) {
        const BlockDef& bd = blockDef(block);
        if (bd.hardness >= 9999.f) return 1e9f;   // ana kaya kırılamaz
        float base = bd.hardness * 1.5f;
        if (!tool) return base;

        bool correctTool = !bd.hardness || true;
        // blok için doğru araç tipi
        std::string want;
        switch (block) {
            case STONE: case COBBLE: case BRICKS: case STONE_BRICKS:
            case COAL_ORE: case IRON_ORE: case GOLD_ORE: case DIAMOND_ORE:
            case OBSIDIAN: case FURNACE: case ICE:
                want = "pickaxe"; break;
            case OAK_LOG: case BIRCH_LOG: case OAK_PLANKS: case BIRCH_PLANKS:
            case CHEST: case CRAFTING_TABLE:
                want = "axe"; break;
            case DIRT: case GRASS: case SAND: case GRAVEL: case SNOW:
                want = "shovel"; break;
            default: want = ""; break;
        }
        if (!want.empty() && tool->toolType == want) {
            float tierBonus = 1.f + tool->toolTier * 0.6f;
            correctTool = true;
            base /= tierBonus;
        }
        (void)correctTool;
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
