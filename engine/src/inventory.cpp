// Craftya Engine — item kayıt defteri, envanter, crafting
#include "craftya/inventory.hpp"
#include <algorithm>
#include <cstring>

namespace craftya {

// ---------------- ItemRegistry ----------------
void ItemRegistry::add(const ItemDef& def) {
    items_.push_back(def);
    byKey_[def.key] = def.id;
}

const ItemDef* ItemRegistry::get(uint16_t id) const {
    for (auto& it : items_) if (it.id == id) return &it;
    return nullptr;
}

const ItemDef* ItemRegistry::byKey(const std::string& key) const {
    auto it = byKey_.find(key);
    return it == byKey_.end() ? nullptr : get(it->second);
}

void ItemRegistry::registerDefaults() {
    // blok itemları (id 1..30 = blok id ile aynı)
    for (int b = 1; b < BLOCK_COUNT; ++b) {
        const BlockDef& bd = blockDef(static_cast<BlockId>(b));
        ItemDef d;
        d.id = static_cast<uint16_t>(b);
        d.key = bd.key;
        d.displayName = bd.name;
        d.kind = ItemKind::Block;
        d.blockId = b;
        d.texIndex = b * 3 + 1;   // yan yüz dokusu
        d.maxStack = 64;
        add(d);
    }
    // araçlar (id 100+)
    struct ToolSeed { uint16_t id; const char* key; const char* name; const char* type; int tier; float dmg; float dur; };
    const ToolSeed tools[] = {
        {100, "wood_pickaxe",   "Tahta Kazma",   "pickaxe", 1, 2.f, 60.f},
        {101, "stone_pickaxe",  "Taş Kazma",     "pickaxe", 2, 3.f, 132.f},
        {102, "iron_pickaxe",   "Demir Kazma",   "pickaxe", 3, 4.f, 250.f},
        {103, "diamond_pickaxe","Elmas Kazma",   "pickaxe", 4, 5.f, 1562.f},
        {110, "wood_axe",       "Tahta Balta",   "axe",     1, 3.f, 60.f},
        {111, "stone_axe",      "Taş Balta",     "axe",     2, 4.f, 132.f},
        {112, "iron_axe",       "Demir Balta",   "axe",     3, 6.f, 250.f},
        {120, "wood_shovel",    "Tahta Kürek",   "shovel",  1, 1.f, 60.f},
        {121, "stone_shovel",   "Taş Kürek",     "shovel",  2, 2.f, 132.f},
        {130, "wood_sword",     "Tahta Kılıç",   "sword",   1, 4.f, 60.f},
        {131, "stone_sword",    "Taş Kılıç",     "sword",   2, 5.f, 132.f},
        {132, "iron_sword",     "Demir Kılıç",   "sword",   3, 6.f, 250.f},
        {133, "diamond_sword",  "Elmas Kılıç",   "sword",   4, 8.f, 1562.f},
        {140, "torch_item",     "Meşale",        "",        0, 0.f, 0.f},
        {150, "coal",           "Kömür",         "",        0, 0.f, 0.f},
        {151, "iron_ingot",     "Demir Külçe",   "",        0, 0.f, 0.f},
        {152, "gold_ingot",     "Altın Külçe",   "",        0, 0.f, 0.f},
        {153, "diamond",        "Elmas",         "",        0, 0.f, 0.f},
        {154, "stick",          "Çubuk",         "",        0, 0.f, 0.f},
        {160, "apple",          "Elma",          "",        0, 0.f, 0.f},
    };
    for (auto& t : tools) {
        ItemDef d;
        d.id = t.id;
        d.key = t.key;
        d.displayName = t.name;
        d.toolType = t.type;
        d.toolTier = t.tier;
        d.damage = t.dmg;
        d.durability = t.dur;
        d.kind = (t.dmg > 0.f) ? ItemKind::Tool : ItemKind::Material;
        if (std::strcmp(t.key, "torch_item") == 0) { d.kind = ItemKind::Block; d.blockId = TORCH; }
        if (std::strcmp(t.key, "apple") == 0) d.kind = ItemKind::Food;
        d.texIndex = 300 + (t.id - 100);
        d.maxStack = (t.dmg > 0.f) ? 1 : 64;
        add(d);
    }
}

// ---------------- Inventory ----------------
int Inventory::add(uint16_t itemId, int count, float durability) {
    if (itemId == 0 || count <= 0) return 0;
    const ItemDef* def = nullptr;
    // maxStack bilgisi dışarıdan gelmiyor; varsayılan 64, araçlar 1
    int maxStack = 64;
    (void)def;
    if (itemId >= 100 && itemId < 140) maxStack = 1;   // araçlar

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
    // tahta kütüğü → tahta (şekilsiz)
    {
        Recipe r;
        r.key = "oak_planks_from_log";
        r.shapeless = true;
        r.pattern = {OAK_LOG,0,0, 0,0,0, 0,0,0};
        r.result = {OAK_PLANKS, 4, 0.f};
        add(r);
    }
    // çubuk
    {
        Recipe r;
        r.key = "stick";
        r.pattern = {OAK_PLANKS,0,0, OAK_PLANKS,0,0, 0,0,0};
        r.result = {154, 4, 0.f};
        add(r);
    }
    // tahta kazma
    {
        Recipe r;
        r.key = "wood_pickaxe";
        r.pattern = {OAK_PLANKS, OAK_PLANKS, OAK_PLANKS,
                     0,           154,         0,
                     0,           154,         0};
        r.result = {100, 1, 60.f};
        add(r);
    }
    // taş kazma
    {
        Recipe r;
        r.key = "stone_pickaxe";
        r.pattern = {COBBLE, COBBLE, COBBLE, 0, 154, 0, 0, 154, 0};
        r.result = {101, 1, 132.f};
        add(r);
    }
    // tahta kılıç
    {
        Recipe r;
        r.key = "wood_sword";
        r.pattern = {OAK_PLANKS,0,0, OAK_PLANKS,0,0, 154,0,0};
        r.result = {130, 1, 60.f};
        add(r);
    }
    // taş kılıç
    {
        Recipe r;
        r.key = "stone_sword";
        r.pattern = {COBBLE,0,0, COBBLE,0,0, 154,0,0};
        r.result = {131, 1, 132.f};
        add(r);
    }
    // tahta balta
    {
        Recipe r;
        r.key = "wood_axe";
        r.pattern = {OAK_PLANKS, OAK_PLANKS, 0, OAK_PLANKS, 154, 0, 0, 154, 0};
        r.result = {110, 1, 60.f};
        add(r);
    }
    // tezgah
    {
        Recipe r;
        r.key = "crafting_table";
        r.pattern = {OAK_PLANKS, OAK_PLANKS, 0, OAK_PLANKS, OAK_PLANKS, 0, 0,0,0};
        r.result = {CRAFTING_TABLE, 1, 0.f};
        add(r);
    }
    // fırın
    {
        Recipe r;
        r.key = "furnace";
        r.pattern = {COBBLE,COBBLE,COBBLE, COBBLE,0,COBBLE, COBBLE,COBBLE,COBBLE};
        r.result = {FURNACE, 1, 0.f};
        add(r);
    }
    // sandık
    {
        Recipe r;
        r.key = "chest";
        r.pattern = {OAK_PLANKS,OAK_PLANKS,OAK_PLANKS, OAK_PLANKS,0,OAK_PLANKS, OAK_PLANKS,OAK_PLANKS,OAK_PLANKS};
        r.result = {CHEST, 1, 0.f};
        add(r);
    }
    // meşale
    {
        Recipe r;
        r.key = "torch";
        r.pattern = {150,0,0, 154,0,0, 0,0,0};
        r.result = {140, 4, 0.f};
        add(r);
    }
    // tuğla
    {
        Recipe r;
        r.key = "bricks";
        r.pattern = {COBBLE,COBBLE,COBBLE, COBBLE,COBBLE,COBBLE, COBBLE,COBBLE,COBBLE};
        r.result = {4, 1, 0.f};
        add(r);
    }
    // cam (kum eritme yerine basit)
    {
        Recipe r;
        r.key = "glass_smelt";
        r.shapeless = true;
        r.pattern = {SAND, SAND, 0, 0,0,0, 0,0,0};
        r.result = {GLASS, 2, 0.f};
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
