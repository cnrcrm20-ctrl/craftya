// Craftya Engine — OTOMATİK ÜRETİLMİŞ eşya kayıt defteri (109 eşya)
// Bloklar (311) + canlılar (37) + eşyalar (109) = 457
#pragma once
#include <cstdint>
#include <array>

namespace craftya {

enum class ItemKind : uint8_t { Block, Tool, Armor, Material, Food, Special };

enum ItemId : uint16_t {
    ITEM_NONE = 0,
    WOOD_PICKAXE = 10001,
    WOOD_AXE = 10002,
    WOOD_SHOVEL = 10003,
    WOOD_SWORD = 10004,
    WOOD_HOE = 10005,
    STONE_PICKAXE = 10006,
    STONE_AXE = 10007,
    STONE_SHOVEL = 10008,
    STONE_SWORD = 10009,
    STONE_HOE = 10010,
    COPPER_PICKAXE = 10011,
    COPPER_AXE = 10012,
    COPPER_SHOVEL = 10013,
    COPPER_SWORD = 10014,
    COPPER_HOE = 10015,
    IRON_PICKAXE = 10016,
    IRON_AXE = 10017,
    IRON_SHOVEL = 10018,
    IRON_SWORD = 10019,
    IRON_HOE = 10020,
    DIAMOND_PICKAXE = 10021,
    DIAMOND_AXE = 10022,
    DIAMOND_SHOVEL = 10023,
    DIAMOND_SWORD = 10024,
    DIAMOND_HOE = 10025,
    EMERALD_PICKAXE = 10026,
    EMERALD_AXE = 10027,
    EMERALD_SHOVEL = 10028,
    EMERALD_SWORD = 10029,
    EMERALD_HOE = 10030,
    NETHERITE_PICKAXE = 10031,
    NETHERITE_AXE = 10032,
    NETHERITE_SHOVEL = 10033,
    NETHERITE_SWORD = 10034,
    NETHERITE_HOE = 10035,
    LEATHER_HELMET = 10036,
    LEATHER_CHESTPLATE = 10037,
    CHAINMAIL_HELMET = 10038,
    CHAINMAIL_CHESTPLATE = 10039,
    IRON_HELMET = 10040,
    IRON_CHESTPLATE = 10041,
    GOLD_HELMET = 10042,
    GOLD_CHESTPLATE = 10043,
    DIAMOND_HELMET = 10044,
    DIAMOND_CHESTPLATE = 10045,
    NETHERITE_HELMET = 10046,
    NETHERITE_CHESTPLATE = 10047,
    STICK = 10048,
    COAL = 10049,
    CHARCOAL = 10050,
    IRON_INGOT = 10051,
    GOLD_INGOT = 10052,
    COPPER_INGOT = 10053,
    NETHERITE_INGOT = 10054,
    DIAMOND = 10055,
    EMERALD = 10056,
    LAPIS_LAZULI = 10057,
    REDSTONE = 10058,
    AMETHYST_SHARD = 10059,
    FLINT = 10060,
    FEATHER = 10061,
    LEATHER = 10062,
    STRING = 10063,
    GUNPOWDER = 10064,
    BONE = 10065,
    BONE_MEAL = 10066,
    SLIME_BALL = 10067,
    CLAY_BALL = 10068,
    BRICK_ITEM = 10069,
    PAPER = 10070,
    BOOK = 10071,
    HONEYCOMB = 10072,
    HONEY_BOTTLE = 10073,
    SUGAR = 10074,
    WHEAT_SEEDS = 10075,
    WHEAT_ITEM = 10076,
    APPLE = 10077,
    GOLDEN_APPLE = 10078,
    BREAD = 10079,
    CARROT = 10080,
    GOLDEN_CARROT = 10081,
    POTATO = 10082,
    BAKED_POTATO = 10083,
    BEEF = 10084,
    COOKED_BEEF = 10085,
    PORKCHOP = 10086,
    COOKED_PORKCHOP = 10087,
    CHICKEN_RAW = 10088,
    CHICKEN_COOKED = 10089,
    MUTTON = 10090,
    COOKED_MUTTON = 10091,
    FISH_COOKED = 10092,
    EGG = 10093,
    MILK_BUCKET = 10094,
    BOW = 10095,
    CROSSBOW = 10096,
    ARROW = 10097,
    SHIELD = 10098,
    FISHING_ROD = 10099,
    FLINT_AND_STEEL = 10100,
    SHEARS = 10101,
    BUCKET = 10102,
    WATER_BUCKET = 10103,
    LAVA_BUCKET = 10104,
    TRIDENT = 10105,
    TOTEM = 10106,
    COMPASS = 10107,
    CLOCK = 10108,
    MAP = 10109,
    ITEM_COUNT = 10110
};
constexpr int kItemCount = 109;

struct ItemDef {
    uint16_t id;
    const char* key;
    const char* name;
    ItemKind kind;
    uint8_t maxStack;
    uint16_t durability;
    float damage;
    float armor;
    uint8_t toolTier;
    const char* toolType;   // pickaxe/axe/shovel/sword/hoe/""
    uint8_t iconStyle;      // 0=dilim 1=cevher 2=yuvarlak 3=sise 4=kure 5=yay 6=kalkan
    uint32_t color;
};

// ikon stili sabitleri
constexpr uint8_t kIcon_ingot = 0;
constexpr uint8_t kIcon_gem = 1;
constexpr uint8_t kIcon_round = 2;
constexpr uint8_t kIcon_bottle = 3;
constexpr uint8_t kIcon_ball = 4;
constexpr uint8_t kIcon_bow = 5;
constexpr uint8_t kIcon_shield = 6;
constexpr uint8_t kIcon_tool = 7;
constexpr uint8_t kIcon_food = 8;
constexpr uint8_t kIcon_book = 9;
constexpr uint8_t kIcon_rod = 10;
constexpr uint8_t kIcon_arrow = 11;
constexpr uint8_t kIcon_block = 12;

inline const std::array<ItemDef, kItemCount>& itemRegistry() {
    static const std::array<ItemDef, kItemCount> reg = {{
        {WOOD_PICKAXE, "wood_pickaxe", "Ahşap Kazma", ItemKind::Tool, 1, 60, 2.00f, 0.0f, 1, "pickaxe", 7, 0xB58C4E},
        {WOOD_AXE, "wood_axe", "Ahşap Balta", ItemKind::Tool, 1, 60, 3.00f, 0.0f, 1, "axe", 7, 0xB58C4E},
        {WOOD_SHOVEL, "wood_shovel", "Ahşap Kürek", ItemKind::Tool, 1, 60, 1.50f, 0.0f, 1, "shovel", 7, 0xB58C4E},
        {WOOD_SWORD, "wood_sword", "Ahşap Kılıç", ItemKind::Tool, 1, 60, 4.00f, 0.0f, 1, "sword", 7, 0xB58C4E},
        {WOOD_HOE, "wood_hoe", "Ahşap Çapa", ItemKind::Tool, 1, 60, 1.00f, 0.0f, 1, "hoe", 7, 0xB58C4E},
        {STONE_PICKAXE, "stone_pickaxe", "Taş Kazma", ItemKind::Tool, 1, 132, 3.50f, 0.0f, 2, "pickaxe", 7, 0x8A8A88},
        {STONE_AXE, "stone_axe", "Taş Balta", ItemKind::Tool, 1, 132, 4.50f, 0.0f, 2, "axe", 7, 0x8A8A88},
        {STONE_SHOVEL, "stone_shovel", "Taş Kürek", ItemKind::Tool, 1, 132, 3.00f, 0.0f, 2, "shovel", 7, 0x8A8A88},
        {STONE_SWORD, "stone_sword", "Taş Kılıç", ItemKind::Tool, 1, 132, 5.50f, 0.0f, 2, "sword", 7, 0x8A8A88},
        {STONE_HOE, "stone_hoe", "Taş Çapa", ItemKind::Tool, 1, 132, 2.50f, 0.0f, 2, "hoe", 7, 0x8A8A88},
        {COPPER_PICKAXE, "copper_pickaxe", "Bakır Kazma", ItemKind::Tool, 1, 190, 3.50f, 0.0f, 2, "pickaxe", 7, 0xE0794A},
        {COPPER_AXE, "copper_axe", "Bakır Balta", ItemKind::Tool, 1, 190, 4.50f, 0.0f, 2, "axe", 7, 0xE0794A},
        {COPPER_SHOVEL, "copper_shovel", "Bakır Kürek", ItemKind::Tool, 1, 190, 3.00f, 0.0f, 2, "shovel", 7, 0xE0794A},
        {COPPER_SWORD, "copper_sword", "Bakır Kılıç", ItemKind::Tool, 1, 190, 5.50f, 0.0f, 2, "sword", 7, 0xE0794A},
        {COPPER_HOE, "copper_hoe", "Bakır Çapa", ItemKind::Tool, 1, 190, 2.50f, 0.0f, 2, "hoe", 7, 0xE0794A},
        {IRON_PICKAXE, "iron_pickaxe", "Demir Kazma", ItemKind::Tool, 1, 250, 5.00f, 0.0f, 3, "pickaxe", 7, 0xD8D8D8},
        {IRON_AXE, "iron_axe", "Demir Balta", ItemKind::Tool, 1, 250, 6.00f, 0.0f, 3, "axe", 7, 0xD8D8D8},
        {IRON_SHOVEL, "iron_shovel", "Demir Kürek", ItemKind::Tool, 1, 250, 4.50f, 0.0f, 3, "shovel", 7, 0xD8D8D8},
        {IRON_SWORD, "iron_sword", "Demir Kılıç", ItemKind::Tool, 1, 250, 7.00f, 0.0f, 3, "sword", 7, 0xD8D8D8},
        {IRON_HOE, "iron_hoe", "Demir Çapa", ItemKind::Tool, 1, 250, 4.00f, 0.0f, 3, "hoe", 7, 0xD8D8D8},
        {DIAMOND_PICKAXE, "diamond_pickaxe", "Elmas Kazma", ItemKind::Tool, 1, 1562, 6.50f, 0.0f, 4, "pickaxe", 7, 0x4AE0D4},
        {DIAMOND_AXE, "diamond_axe", "Elmas Balta", ItemKind::Tool, 1, 1562, 7.50f, 0.0f, 4, "axe", 7, 0x4AE0D4},
        {DIAMOND_SHOVEL, "diamond_shovel", "Elmas Kürek", ItemKind::Tool, 1, 1562, 6.00f, 0.0f, 4, "shovel", 7, 0x4AE0D4},
        {DIAMOND_SWORD, "diamond_sword", "Elmas Kılıç", ItemKind::Tool, 1, 1562, 8.50f, 0.0f, 4, "sword", 7, 0x4AE0D4},
        {DIAMOND_HOE, "diamond_hoe", "Elmas Çapa", ItemKind::Tool, 1, 1562, 5.50f, 0.0f, 4, "hoe", 7, 0x4AE0D4},
        {EMERALD_PICKAXE, "emerald_pickaxe", "Zümrüt Kazma", ItemKind::Tool, 1, 1400, 6.50f, 0.0f, 4, "pickaxe", 7, 0x2FCB46},
        {EMERALD_AXE, "emerald_axe", "Zümrüt Balta", ItemKind::Tool, 1, 1400, 7.50f, 0.0f, 4, "axe", 7, 0x2FCB46},
        {EMERALD_SHOVEL, "emerald_shovel", "Zümrüt Kürek", ItemKind::Tool, 1, 1400, 6.00f, 0.0f, 4, "shovel", 7, 0x2FCB46},
        {EMERALD_SWORD, "emerald_sword", "Zümrüt Kılıç", ItemKind::Tool, 1, 1400, 8.50f, 0.0f, 4, "sword", 7, 0x2FCB46},
        {EMERALD_HOE, "emerald_hoe", "Zümrüt Çapa", ItemKind::Tool, 1, 1400, 5.50f, 0.0f, 4, "hoe", 7, 0x2FCB46},
        {NETHERITE_PICKAXE, "netherite_pickaxe", "Netherit Kazma", ItemKind::Tool, 1, 2031, 8.00f, 0.0f, 5, "pickaxe", 7, 0x45383E},
        {NETHERITE_AXE, "netherite_axe", "Netherit Balta", ItemKind::Tool, 1, 2031, 9.00f, 0.0f, 5, "axe", 7, 0x45383E},
        {NETHERITE_SHOVEL, "netherite_shovel", "Netherit Kürek", ItemKind::Tool, 1, 2031, 7.50f, 0.0f, 5, "shovel", 7, 0x45383E},
        {NETHERITE_SWORD, "netherite_sword", "Netherit Kılıç", ItemKind::Tool, 1, 2031, 10.00f, 0.0f, 5, "sword", 7, 0x45383E},
        {NETHERITE_HOE, "netherite_hoe", "Netherit Çapa", ItemKind::Tool, 1, 2031, 7.00f, 0.0f, 5, "hoe", 7, 0x45383E},
        {LEATHER_HELMET, "leather_helmet", "Deri Miğfer", ItemKind::Armor, 1, 80, 0.00f, 3.0f, 0, "", 0, 0x9A6A3A},
        {LEATHER_CHESTPLATE, "leather_chestplate", "Deri Göğüslük", ItemKind::Armor, 1, 128, 0.00f, 4.8f, 0, "", 0, 0x9A6A3A},
        {CHAINMAIL_HELMET, "chainmail_helmet", "Zincir Miğfer", ItemKind::Armor, 1, 80, 0.00f, 5.0f, 0, "", 0, 0x9A9AA0},
        {CHAINMAIL_CHESTPLATE, "chainmail_chestplate", "Zincir Göğüslük", ItemKind::Armor, 1, 128, 0.00f, 8.0f, 0, "", 0, 0x9A9AA0},
        {IRON_HELMET, "iron_helmet", "Demir Miğfer", ItemKind::Armor, 1, 80, 0.00f, 7.0f, 0, "", 0, 0xD8D8D8},
        {IRON_CHESTPLATE, "iron_chestplate", "Demir Göğüslük", ItemKind::Armor, 1, 128, 0.00f, 11.2f, 0, "", 0, 0xD8D8D8},
        {GOLD_HELMET, "gold_helmet", "Altın Miğfer", ItemKind::Armor, 1, 80, 0.00f, 6.0f, 0, "", 0, 0xF9D849},
        {GOLD_CHESTPLATE, "gold_chestplate", "Altın Göğüslük", ItemKind::Armor, 1, 128, 0.00f, 9.6f, 0, "", 0, 0xF9D849},
        {DIAMOND_HELMET, "diamond_helmet", "Elmas Miğfer", ItemKind::Armor, 1, 80, 0.00f, 9.0f, 0, "", 0, 0x4AE0D4},
        {DIAMOND_CHESTPLATE, "diamond_chestplate", "Elmas Göğüslük", ItemKind::Armor, 1, 128, 0.00f, 14.4f, 0, "", 0, 0x4AE0D4},
        {NETHERITE_HELMET, "netherite_helmet", "Netherit Miğfer", ItemKind::Armor, 1, 80, 0.00f, 10.0f, 0, "", 0, 0x45383E},
        {NETHERITE_CHESTPLATE, "netherite_chestplate", "Netherit Göğüslük", ItemKind::Armor, 1, 128, 0.00f, 16.0f, 0, "", 0, 0x45383E},
        {STICK, "stick", "Çubuk", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 10, 0xA5763F},
        {COAL, "coal", "Kömür", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0x2B2B2B},
        {CHARCOAL, "charcoal", "Odun Kömürü", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {IRON_INGOT, "iron_ingot", "Demir Külçe", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 0, 0xD8D8D8},
        {GOLD_INGOT, "gold_ingot", "Altın Külçe", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 0, 0xF9D849},
        {COPPER_INGOT, "copper_ingot", "Bakır Külçe", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 0, 0xE0794A},
        {NETHERITE_INGOT, "netherite_ingot", "Netherit Külçe", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 0, 0x45383E},
        {DIAMOND, "diamond", "Elmas", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 1, 0x4AE0D4},
        {EMERALD, "emerald", "Zümrüt", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 1, 0x2FCB46},
        {LAPIS_LAZULI, "lapis_lazuli", "Lapis", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 1, 0x2A4FC4},
        {REDSTONE, "redstone", "Kızıltaş", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xA81B1B},
        {AMETHYST_SHARD, "amethyst_shard", "Ametist Parçası", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 1, 0xB0B0B0},
        {FLINT, "flint", "Çakmak Taşı", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {FEATHER, "feather", "Tüy", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 6, 0xB0B0B0},
        {LEATHER, "leather", "Deri", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 5, 0xB0B0B0},
        {STRING, "string", "İp", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 6, 0xB0B0B0},
        {GUNPOWDER, "gunpowder", "Barut", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {BONE, "bone", "Kemik", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 6, 0xB0B0B0},
        {BONE_MEAL, "bone_meal", "Kemik Tozu", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {SLIME_BALL, "slime_ball", "Balçık Topu", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {CLAY_BALL, "clay_ball", "Kil Topu", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {BRICK_ITEM, "brick_item", "Tuğla", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 0, 0xB0B0B0},
        {PAPER, "paper", "Kağıt", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 9, 0xB0B0B0},
        {BOOK, "book", "Kitap", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 9, 0xB0B0B0},
        {HONEYCOMB, "honeycomb", "Bal Peteği", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {HONEY_BOTTLE, "honey_bottle", "Bal Şişesi", ItemKind::Food, 1, 0, 0.00f, 0.0f, 0, "", 3, 0xB0B0B0},
        {SUGAR, "sugar", "Şeker", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {WHEAT_SEEDS, "wheat_seeds", "Buğday Tohumu", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {WHEAT_ITEM, "wheat_item", "Buğday", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {APPLE, "apple", "Elma", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xD02A2A},
        {GOLDEN_APPLE, "golden_apple", "Altın Elma", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {BREAD, "bread", "Ekmek", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xC89A4A},
        {CARROT, "carrot", "Havuç", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {GOLDEN_CARROT, "golden_carrot", "Altın Havuç", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {POTATO, "potato", "Patates", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {BAKED_POTATO, "baked_potato", "Fırınlanmış Patates", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {BEEF, "beef", "Çiğ Et", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {COOKED_BEEF, "cooked_beef", "Pişmiş Et", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {PORKCHOP, "porkchop", "Çiğ Domuz Eti", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {COOKED_PORKCHOP, "cooked_porkchop", "Pişmiş Domuz Eti", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {CHICKEN_RAW, "chicken_raw", "Çiğ Tavuk", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {CHICKEN_COOKED, "chicken_cooked", "Pişmiş Tavuk", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {MUTTON, "mutton", "Koyun Eti", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {COOKED_MUTTON, "cooked_mutton", "Pişmiş Koyun Eti", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {FISH_COOKED, "fish_cooked", "Pişmiş Balık", ItemKind::Food, 64, 0, 0.00f, 0.0f, 0, "", 8, 0xB0B0B0},
        {EGG, "egg", "Yumurta", ItemKind::Material, 64, 0, 0.00f, 0.0f, 0, "", 2, 0xB0B0B0},
        {MILK_BUCKET, "milk_bucket", "Süt Kovası", ItemKind::Material, 1, 0, 0.00f, 0.0f, 0, "", 3, 0xB0B0B0},
        {BOW, "bow", "Yay", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 5, 0xA5763F},
        {CROSSBOW, "crossbow", "Arbalet", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 5, 0xB0B0B0},
        {ARROW, "arrow", "Ok", ItemKind::Special, 64, 0, 0.00f, 0.0f, 0, "", 11, 0xC8C8C8},
        {SHIELD, "shield", "Kalkan", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 6, 0xB0B0B0},
        {FISHING_ROD, "fishing_rod", "Olta", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 10, 0xB0B0B0},
        {FLINT_AND_STEEL, "flint_and_steel", "Çakmak", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 4, 0xB0B0B0},
        {SHEARS, "shears", "Makas", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 7, 0xB0B0B0},
        {BUCKET, "bucket", "Kova", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 3, 0xB0B0B0},
        {WATER_BUCKET, "water_bucket", "Su Kovası", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 3, 0x3A6AD0},
        {LAVA_BUCKET, "lava_bucket", "Lav Kovası", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 3, 0xE05A10},
        {TRIDENT, "trident", "Mızrak", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 7, 0xB0B0B0},
        {TOTEM, "totem", "Totem", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 2, 0xE8C850},
        {COMPASS, "compass", "Pusula", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 2, 0xB0B0B0},
        {CLOCK, "clock", "Saat", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 2, 0xB0B0B0},
        {MAP, "map", "Harita", ItemKind::Special, 1, 0, 0.00f, 0.0f, 0, "", 9, 0xB0B0B0},
    }};
    return reg;
}

inline const ItemDef* itemDef(uint16_t id) {
    if (id < 10001 || id >= ITEM_COUNT) return nullptr;
    return &itemRegistry()[id - 10001];
}

} // namespace craftya
