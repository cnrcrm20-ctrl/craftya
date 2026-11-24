// Craftya Engine — etkileşimli bloklar + birleşik içerik kayıt defteri
#include "craftya/interact.hpp"
#include <algorithm>
#include <cstring>
#include <cctype>

namespace craftya {

InteractResult InteractionManager::activate(BlockId block, BlockState& state,
                                            const ItemStack& held) {
    InteractResult r;
    const BlockDef& d = blockDef(block);
    lastMessage_.clear();

    switch (d.interact) {
        case Interact::Door:
        case Interact::Trapdoor:
            state.toggleOpen();
            r.handled = true;
            r.playSound = true;
            r.soundId = 13;   // DoorOpen
            r.message = state.isOpen() ? "Kapi acildi" : "Kapi kapandi";
            break;

        case Interact::Chest:
        case Interact::Barrel:
        case Interact::Hopper:
        case Interact::Dispenser:
        case Interact::Dropper: {
            state.toggleOpen();
            r.handled = true;
            r.playSound = true;
            r.soundId = 6;
            r.message = std::string(d.name) + (state.isOpen() ? " acildi (27 yuva)" : " kapandi");
            break;
        }

        case Interact::CraftingTable:
            r.handled = true;
            r.message = "Uretim izgarasi acildi";
            break;

        case Interact::Furnace:
        case Interact::BlastFurnace:
        case Interact::Smoker: {
            // yakıt varsa yak
            bool hasFuel = held.itemId == COAL || held.itemId == CHARCOAL;
            if (hasFuel && !state.isLit()) {
                state.setLit(true);
                state.timer = 8.f;
                r.consumedItem = true;
                r.message = "Firin yakildi";
            } else if (state.isLit()) {
                r.message = "Firin yaniyor";
            } else {
                r.message = "Yakit gerekli (komur)";
            }
            r.handled = true;
            break;
        }

        case Interact::Lever:
        case Interact::Button:
        case Interact::PressurePlate:
            if (state.isPowered()) state.flags &= ~4;
            else state.flags |= 4;
            r.handled = true;
            r.playSound = true;
            r.soundId = 6;
            r.message = state.isPowered() ? "Sinyal acik" : "Sinyal kapali";
            r.cooldown = 0.25f;
            break;

        case Interact::Ladder:
            r.handled = true;
            r.message = "Tirman";
            break;

        case Interact::Lantern:
        case Interact::Campfire:
        case Interact::Torch:
            state.setLit(!state.isLit());
            r.handled = true;
            r.message = state.isLit() ? "Yakildi" : "Sonduruldu";
            break;

        case Interact::Anvil:
            r.handled = true;
            r.message = "Ors: tamir / isim";
            break;
        case Interact::Grindstone:
            r.handled = true;
            r.message = "Bilegi tasi: tamir";
            break;
        case Interact::Stonecutter:
            r.handled = true;
            r.message = "Tas kesici acildi";
            break;
        case Interact::Smithing:
            r.handled = true;
            r.message = "Demirci tezgahi";
            break;
        case Interact::Enchanting:
            r.handled = true;
            r.message = "Buyu masasi (lapis gerekli)";
            break;
        case Interact::Brewing:
            r.handled = true;
            r.message = "Iksir standi";
            break;
        case Interact::Cauldron:
            r.handled = true;
            r.message = "Kazan (su ile doldur)";
            break;
        case Interact::Beacon:
            r.handled = true;
            r.message = "Fener kulesi (demir blok tabani gerekli)";
            break;
        case Interact::NoteBlock:
            r.handled = true;
            r.playSound = true;
            r.soundId = 6;
            r.message = "Nota caldi";
            break;
        case Interact::Jukebox:
            r.handled = true;
            r.message = "Muzik kutusu";
            break;
        case Interact::Lectern:
            r.handled = true;
            r.message = "Kursu: kitap koy";
            break;
        case Interact::Composter:
            r.handled = true;
            r.message = "Kompost: bitki at";
            break;
        case Interact::Bell:
            r.handled = true;
            r.playSound = true;
            r.soundId = 6;
            r.message = "Can caldi";
            break;
        case Interact::Bed:
            r.handled = true;
            r.message = "Uyudun — sabah oldu";
            break;
        case Interact::TNT:
            r.handled = true;
            state.setLit(true);
            state.timer = 3.f;
            r.message = "TNT ateslendi!";
            r.cooldown = 1.f;
            break;
        case Interact::Piston:
            state.toggleOpen();
            r.handled = true;
            r.message = state.isOpen() ? "Piston uzadi" : "Piston geri cekildi";
            break;
        case Interact::Observer:
            r.handled = true;
            r.message = "Gozlemci: degisim bekliyor";
            break;
        case Interact::Anchor:
            r.handled = true;
            r.message = "Capa: parlayan tas gerekli";
            break;
        case Interact::Sensor:
            r.handled = true;
            r.message = "Sculk sensoru titresimi algilar";
            break;
        case Interact::Crafter:
            r.handled = true;
            r.message = "Uretici acildi";
            break;
        case Interact::Pot:
            r.handled = true;
            r.message = "Comlek kirildi";
            break;
        case Interact::Structure:
        case Interact::Command:
        case Interact::Jigsaw:
            r.handled = true;
            r.message = "Yonetici blogu — sadece yaratici";
            break;
        case Interact::None:
        default:
            r.handled = false;
            break;
    }

    lastMessage_ = r.message;
    return r;
}

float InteractionManager::breakMultiplier(BlockId block, const ItemDef* tool) const {
    const BlockDef& d = blockDef(block);
    if (!tool) return 1.f;
    // alet türü blokla eşleşiyor mu (basit sezgisel)
    uint8_t st = d.style;
    bool good = false;
    if (std::strcmp(tool->toolType, "pickaxe") == 0)
        good = (st == kStyle_stone || st == kStyle_cobble || st == kStyle_brick ||
                st == kStyle_ore || st == kStyle_smooth || st == kStyle_metal ||
                st == kStyle_speckle || st == kStyle_gem);
    else if (std::strcmp(tool->toolType, "axe") == 0)
        good = (st == kStyle_log || st == kStyle_plank || st == kStyle_books || st == kStyle_door);
    else if (std::strcmp(tool->toolType, "shovel") == 0)
        good = (st == kStyle_noise || st == kStyle_grass || st == kStyle_grain || st == kStyle_speckle);
    else if (std::strcmp(tool->toolType, "sword") == 0)
        good = (st == kStyle_cross || st == kStyle_leaves || st == kStyle_wool);
    else if (std::strcmp(tool->toolType, "hoe") == 0)
        good = (st == kStyle_leaves || st == kStyle_grass || st == kStyle_noise);
    if (!good) return 1.f;
    return 1.f + static_cast<float>(tool->toolTier) * 0.6f;
}

std::vector<ItemStack> InteractionManager::dropsFor(BlockId block, const ItemDef* tool) const {
    std::vector<ItemStack> out;
    const BlockDef& d = blockDef(block);
    if (block == AIR || block == WATER || block == LAVA) return out;

    // cevherler: alet tier'ı yetersizse düşmez
    if (d.style == kStyle_ore) {
        uint8_t need = (d.key[0] == 'd' && std::strncmp(d.key, "deepslate", 9) == 0) ? 2 : 1;
        uint8_t tier = tool ? tool->toolTier : 0;
        if (tier < need && block != COAL_ORE) return out;
    }
    // taş benzeri: alet yoksa düşmez
    if (d.hardness >= 1.4f && d.style == kStyle_stone && (!tool || tool->toolTier == 0)) {
        if (block != STONE) return out;
    }

    // blok → düşen eşya eşlemesi (blok id aralığı 1..310)
    if (static_cast<int>(block) > 0 && static_cast<int>(block) < BLOCK_COUNT) {
        ItemStack s;
        s.itemId = static_cast<uint16_t>(block);
        s.count = 1;
        out.push_back(s);
    }
    return out;
}

Container& InteractionManager::container(uint8_t id) {
    return containers_[id % kMaxContainers];
}

uint8_t InteractionManager::allocateContainer() {
    uint8_t id = nextContainer_;
    nextContainer_ = static_cast<uint8_t>((nextContainer_ + 1) % kMaxContainers);
    containers_[id].clear();
    return id;
}

bool InteractionManager::isRedstoneSource(BlockId b) const {
    switch (b) {
        case LEVER: case STONE_BUTTON: case OAK_BUTTON:
        case STONE_PRESSURE_PLATE: case OAK_PRESSURE_PLATE:
        case OBSERVER: case SCULK_SENSOR: case TARGET:
            return true;
        default: return false;
    }
}

void InteractionManager::tick(float dt) {
    globalTimer_ += dt;
    for (auto& c : containers_) {
        for (auto& s : c.slots) {
            if (!s.empty() && s.durability < 0.f) s.clear();
        }
    }
}

// ---------------- ContentRegistry ----------------
ContentRegistry::ContentRegistry() {
    entries_.reserve(600);
    // bloklar
    for (int i = 1; i < BLOCK_COUNT; ++i) {
        const BlockDef& d = blockDef(static_cast<BlockId>(i));
        entries_.push_back({Entry::Type::Block, static_cast<uint16_t>(i), d.key, d.name, d.color});
    }
    blockCount_ = entries_.size();
    // canlılar (balık kombinasyonları HARİÇ)
    for (int i = 1; i < MOB_COUNT; ++i) {
        const MobDef& m = mobDef(static_cast<uint16_t>(i));
        entries_.push_back({Entry::Type::Mob, static_cast<uint16_t>(i), m.key, m.name, m.bodyColor});
    }
    mobCount_ = entries_.size() - blockCount_;
    // eşyalar
    const auto& items = itemRegistry();
    for (int i = 0; i < kItemCount; ++i) {
        const ItemDef& it = items[i];
        entries_.push_back({Entry::Type::Item, it.id, it.key, it.name, it.color});
    }
    itemCount_ = entries_.size() - blockCount_ - mobCount_;
}

const ContentRegistry& ContentRegistry::instance() {
    static ContentRegistry reg;
    return reg;
}

const ContentRegistry::Entry* ContentRegistry::find(const std::string& key) const {
    for (const auto& e : entries_) if (key == e.key) return &e;
    return nullptr;
}

const ContentRegistry::Entry* ContentRegistry::findBlock(BlockId id) const {
    if (id <= 0 || id >= BLOCK_COUNT) return nullptr;
    return &entries_[static_cast<size_t>(id) - 1];
}

const ContentRegistry::Entry* ContentRegistry::findItem(uint16_t id) const {
    if (id < 10001 || id >= ITEM_COUNT) return nullptr;
    return &entries_[blockCount_ + mobCount_ + (id - 10001)];
}

const ContentRegistry::Entry* ContentRegistry::findMob(uint16_t id) const {
    if (id <= 0 || id >= MOB_COUNT) return nullptr;
    return &entries_[blockCount_ + id - 1];
}

std::vector<const ContentRegistry::Entry*> ContentRegistry::search(const std::string& q,
                                                                   size_t limit) const {
    std::vector<const Entry*> out;
    std::string lower;
    lower.reserve(q.size());
    for (char c : q) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    for (const auto& e : entries_) {
        std::string name;
        name.reserve(e.name.size());
        for (char c : e.name) name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if (name.find(lower) != std::string::npos || e.key.find(lower) != std::string::npos) {
            out.push_back(&e);
            if (out.size() >= limit) break;
        }
    }
    return out;
}

} // namespace craftya
