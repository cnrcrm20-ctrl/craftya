// Craftya Engine — item kayıt defteri, envanter, crafting
#include "craftya/inventory.hpp"
#include <algorithm>
#include <cstring>

namespace craftya {

// ---------------- ItemRegistry ----------------
// ---------------- Inventory ----------------
int Inventory::add(uint16_t itemId, int count, float durability) {
    if (itemId == 0 || count <= 0) return 0;
    // maxStack: bloklar 64, alet/zırh 1, diğerleri kayıttan
    int maxStack = 64;
    {
        const ItemDef* def = itemDef(itemId);
        if (def) maxStack = def->maxStack;
    }

    int remaining = count;
    // 1) mevcut yığınlara ekle
    for (int i = 0; i < TOTAL && remaining > 0; ++i) {
        auto& s = slots_[i];
        if (s.itemId == itemId && s.count < maxStack) {
            int space = maxStack - s.count;
            int put = std::min(space, remaining);
            s.count = static_cast<uint8_t>(s.count + put);
            remaining -= put;
        }
    }
    // 2) boş slotlara koy
    for (int i = 0; i < TOTAL && remaining > 0; ++i) {
        auto& s = slots_[i];
        if (s.empty()) {
            int put = std::min(maxStack, remaining);
            s.itemId = itemId;
            s.count = static_cast<uint8_t>(put);
            s.durability = durability;
            remaining -= put;
        }
    }
    return remaining;
}

ItemStack Inventory::take(int index, int count) {
    ItemStack out;
    if (index < 0 || index >= TOTAL) return out;
    auto& s = slots_[index];
    if (s.empty()) return out;
    int n = std::min<int>(count, s.count);
    out = s;
    out.count = static_cast<uint8_t>(n);
    s.count = static_cast<uint8_t>(s.count - n);
    if (s.count == 0) s.clear();
    return out;
}

int Inventory::countOf(uint16_t itemId) const {
    int n = 0;
    for (auto& s : slots_) if (s.itemId == itemId) n += s.count;
    return n;
}

bool Inventory::removeItems(uint16_t itemId, int count) {
    if (countOf(itemId) < count) return false;
    int need = count;
    for (auto& s : slots_) {
        if (s.itemId != itemId) continue;
        int takeN = std::min<int>(need, s.count);
        s.count = static_cast<uint8_t>(s.count - takeN);
        need -= takeN;
        if (s.count == 0) s.clear();
        if (need == 0) break;
    }
    return need == 0;
}

int Inventory::selectedBlockId() const {
    const auto& s = slots_[selected_];
    if (s.empty()) return 0;
    if (s.itemId >= 1 && s.itemId < BLOCK_COUNT) return s.itemId;
    if (s.itemId == 140) return TORCH;
    return 0;
}

bool Inventory::full() const {
    for (auto& s : slots_) if (s.empty()) return false;
    return true;
}

// ---------------- CraftingRegistry ----------------
void CraftingRegistry::add(const Recipe& r) { recipes_.push_back(r); }

bool CraftingRegistry::gridEquals(const std::array<uint16_t,9>& a, const std::array<uint16_t,9>& b) {
    for (int i = 0; i < 9; ++i) {
        bool ae = a[i] == 0, be = b[i] == 0;
        if (ae != be) return false;
        if (!ae && a[i] != b[i]) return false;
    }
    return true;
}

std::array<uint16_t,9> CraftingRegistry::normalize(const std::array<uint16_t,9>& g) {
    // en küçük satır/sütuna kaydır (3x3 içinde konum bağımsız eşleşme)
    int minR = 3, minC = 3, maxR = -1, maxC = -1;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (g[r*3+c]) {
                minR = std::min(minR, r); maxR = std::max(maxR, r);
                minC = std::min(minC, c); maxC = std::max(maxC, c);
            }
    std::array<uint16_t,9> out{};
    if (maxR < 0) return out;
    for (int r = minR; r <= maxR; ++r)
        for (int c = minC; c <= maxC; ++c)
            out[(r-minR)*3 + (c-minC)] = g[r*3+c];
    return out;
}

const Recipe* CraftingRegistry::match(const std::array<ItemStack,9>& grid) const {
    std::array<uint16_t,9> ids{};
    for (int i = 0; i < 9; ++i) ids[i] = grid[i].count ? grid[i].itemId : 0;
    auto norm = normalize(ids);
    for (auto& r : recipes_) {
        auto rn = normalize(r.pattern);
        if (gridEquals(norm, rn)) return &r;
    }
    return nullptr;
}

void CraftingRegistry::registerDefaults() {
    recipes_.clear();
    const auto& defs = recipeRegistry();
    for (const auto& d : defs) {
        Recipe r;
        r.key = d.key;
        r.pattern = d.pattern;
        r.result = ItemStack{d.resultId, d.resultCount, 0.f};
        r.shapeless = d.shapeless;
        add(r);
    }
}

void CraftingGrid::consume(const Recipe& r) {
    // tarife giren her hücreden 1 azalt
    (void)r;
    for (int i = 0; i < 9; ++i) {
        if (cells[i].count > 0) {
            cells[i].count = static_cast<uint8_t>(cells[i].count - 1);
            if (cells[i].count == 0) cells[i].clear();
        }
    }
}

} // namespace craftya
