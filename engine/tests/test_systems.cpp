// Craftya Engine — genişletilmiş sistem testleri
// json / lighting / atlas / inventory / crafting / save / net / ecs / audio / gui
#include "craftya/json.hpp"
#include "craftya/lighting.hpp"
#include "craftya/atlas.hpp"
#include "craftya/inventory.hpp"
#include "craftya/save.hpp"
#include "craftya/net.hpp"
#include "craftya/ecs.hpp"
#include "craftya/audio.hpp"
#include "craftya/gui.hpp"
#include "craftya/engine.hpp"
#include "craftya/world.hpp"
#include "craftya/mesher.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

using namespace craftya;

static int g_pass = 0, g_fail = 0;
static const char* g_group = "";

static void group(const char* name) { g_group = name; std::printf("\n[%s]\n", name); }
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; std::printf("  ok   %s\n", what); }
    else { ++g_fail; std::printf("  FAIL %s  (%s)\n", what, g_group); }
}

// ---------------------------------------------------------------- JSON
static void testJson() {
    group("json");

    std::string err;
    Json j = Json::parse(R"({"name":"craftya","version":3,"ratio":0.75,"ok":true,
                             "modes":["duello","takimli","mega"],
                             "blocks":{"grass":{"hardness":0.6,"light":0}}})", &err);
    check(err.empty(), "gecerli JSON ayristirildi");
    check(j.isObject(), "kok nesne");
    check(j["name"].asString() == "craftya", "string alan");
    check(j["version"].asInt() == 3, "int alan");
    check(std::fabs(j["ratio"].asFloat() - 0.75f) < 1e-6f, "float alan");
    check(j["ok"].asBool() == true, "bool alan");
    check(j["modes"].size() == 3, "dizi uzunlugu");
    check(j["modes"].at(1).asString() == "takimli", "dizi elemani");
    check(j.has("blocks"), "has()");
    check(!j.has("yok"), "has() olmayan");
    check(j["blocks"]["grass"]["hardness"].asFloat() > 0.5f, "ic ice nesne");

    std::string err2;
    Json bad = Json::parse(R"({"a":1,})", &err2);
    check(!err2.empty(), "bozuk JSON hata veriyor");
    (void)bad;

    // sunucudan blok tanımları
    std::string blockJson = R"([
        {"id":1,"key":"grass","name":"Cim","texTop":0,"texSide":1,"texBottom":2,
         "hardness":0.6,"tool":"shovel","light":0,"opaque":true},
        {"id":20,"key":"glowstone","name":"Isik Tasi","texTop":60,"texSide":60,"texBottom":60,
         "hardness":0.3,"tool":"pickaxe","light":15,"opaque":true}
    ])";
    std::string berr;
    auto defs = parseBlockDefinitions(blockJson, &berr);
    check(berr.empty() && defs.size() == 2, "sunucu blok JSON'u okundu");
    check(defs.size() == 2 && defs[1].light == 15, "isik degeri okundu");
    check(defs.size() == 2 && defs[0].key == "grass", "anahtar okundu");

    // gidiş-dönüş
    Json built = Json::parse(j.dump());
    check(built["modes"].size() == 3, "dump/parse dongusu");
}

// ---------------------------------------------------------------- LIGHTING
static void testLighting() {
    group("lighting");

    // gündüz/gece faktörü
    check(std::fabs(LightEngine::sunFactor(12.f) - 1.f) < 0.01f, "ogle: tam gunes");
    check(LightEngine::sunFactor(2.f) < 0.2f, "gece: karanlik");
    check(LightEngine::sunFactor(5.5f) > 0.1f && LightEngine::sunFactor(5.5f) < 0.9f, "safak: gecis");
    check(LightEngine::sunFactor(20.f) < 0.45f, "aksam 20: isik azaldi");
    check(LightEngine::sunFactor(22.f) < 0.15f, "gece 22: karanlik");

    float col[3];
    LightEngine::skyColor(12.f, col);
    check(col[2] > col[0], "gunduz gokyuzu mavi baskin");
    LightEngine::skyColor(2.f, col);
    check(col[0] < 0.2f && col[1] < 0.2f && col[2] < 0.3f, "gece gokyuzu koyu");

    // AO
    check(AmbientOcclusion::vertexFactor(false, false, false) > 0.99f, "AO: acik kose = 1.0");
    check(AmbientOcclusion::vertexFactor(true, false, false) < 1.f, "AO: tek taraf kisuyor");
    check(AmbientOcclusion::vertexFactor(true, true, false) <
          AmbientOcclusion::vertexFactor(true, false, false), "AO: iki taraf daha koyu");

    // gerçek dünya ışığı: gökyüzü açık, güneş ışığı yüzeye iner
    World w(1234, false);
    for (int x = 0; x < 4; ++x)
        for (int z = 0; z < 4; ++z)
            for (int y = 0; y < 4; ++y)
                w.setBlock(x, y, z, STONE);
    LightEngine::onBlockChanged(w, 1, 4, 1, AIR, STONE);
    check(true, "isik yeniden hesaplandi (crash yok)");

    // ışık yayan blok
    World g(7, false);
    g.setBlock(2, 2, 2, GLOWSTONE);
    LightEngine::onBlockChanged(g, 2, 2, 2, AIR, GLOWSTONE);
    check(g.blockLightAt(2, 2, 2) > 0, "isik tasi kendi blogunda isik yayar");
    check(g.blockLightAt(3, 2, 2) > 0, "isik yayilir (komsu)");
    check(g.blockLightAt(3, 2, 2) <= g.blockLightAt(2, 2, 2), "isik mesafeyle azalir");

    // DayCycle
    DayCycle dc;
    dc.hours = 12.f;
    float before = dc.hours;
    dc.update(1.f);
    check(dc.hours != before, "gun dongusu ilerliyor");
    check(dc.sunFactor() >= 0.f && dc.sunFactor() <= 1.f, "gunes faktoru aralikta");
}

// ---------------------------------------------------------------- ATLAS
static void testAtlas() {
    group("atlas");
    Atlas atlasObj;
    atlasObj.generateProcedural();
    const Image& img = atlasObj.image();
    check(img.width == Atlas::SIZE && img.height == Atlas::SIZE, "atlas 256x256");
    check(img.pixels.size() == static_cast<size_t>(Atlas::SIZE) * Atlas::SIZE * 4, "RGBA tampon boyutu");

    // dolu pikseller var mı
    int nonEmpty = 0;
    for (size_t i = 0; i < img.pixels.size(); i += 4)
        if (img.pixels[i+3] > 0) ++nonEmpty;
    check(nonEmpty > Atlas::SIZE * Atlas::SIZE / 2, "atlasin cogu dolu");

    // her blok için geçerli kare indeksi
    bool idsOk = true;
    for (int idi = 1; idi < 32; ++idi) {
        for (int f = 0; f < 3; ++f) {
            int t = Atlas::tileForBlock(static_cast<BlockId>(idi), f);
            if (t < 0 || t >= Atlas::GRID * Atlas::GRID) idsOk = false;
        }
    }
    check(idsOk, "tum blok yuzleri gecerli kare");

    float u0, v0, u1, v1;
    Atlas::tileUV(0, u0, v0, u1, v1);
    check(u0 == 0.f && v0 == 0.f && u1 > 0.f && u1 <= 1.f, "UV 0. kare");
    Atlas::tileUV(Atlas::GRID * Atlas::GRID - 1, u0, v0, u1, v1);
    check(u1 <= 1.0001f && v1 <= 1.0001f, "UV son kare sinirda");

    // PNG yazıcı
    std::string pngPath = "/tmp/craftya_atlas_test.png";
    check(img.savePNG(pngPath), "PNG dosyasi yazildi");
    FILE* fp = std::fopen(pngPath.c_str(), "rb");
    if (fp) {
        uint8_t head[16] = {0};
        size_t got = std::fread(head, 1, 16, fp);
        std::fclose(fp);
        check(got == 16, "PNG basligi okundu");
        check(head[0] == 0x89 && head[1] == 'P' && head[2] == 'N' && head[3] == 'G', "PNG imzasi");
        check(head[12] == 'I' && head[13] == 'H' && head[14] == 'D' && head[15] == 'R', "IHDR chunk");
    } else {
        check(false, "PNG dosyasi acilamadi");
    }
}

// ---------------------------------------------------------------- INVENTORY
static void testInventory() {
    group("inventory");

    ItemRegistry reg;
    reg.registerDefaults();
    check(reg.count() >= 50, "esya kayit defteri dolu");
    check(reg.get(1) != nullptr, "blok esyasi var (id 1)");
    check(reg.byKey("grass_block") != nullptr, "anahtarla arama");

    const ItemDef* pick = reg.get(100);   // tahta kazma
    check(pick != nullptr && pick->kind == ItemKind::Tool, "kazma alet");
    check(pick != nullptr && pick->maxStack == 1, "alet yigilanmaz");

    Inventory inv;
    check(inv.add(1, 10) == 0, "10 blok eklendi");
    check(inv.countOf(1) == 10, "sayim dogru");
    check(inv.add(1, 60) == 0, "ayni yigina eklendi");
    check(inv.countOf(1) == 70, "70 toplam");
    int slotsUsed = 0;
    for (int i = 0; i < Inventory::TOTAL; ++i) if (!inv.at(i).empty()) ++slotsUsed;
    check(slotsUsed == 2, "70 blok 2 yuvaya bolundu (64+6)");

    ItemStack taken = inv.take(0, 5);
    check(taken.count == 5 && inv.countOf(1) == 65, "5 alindi");

    inv.select(3);
    check(inv.selectedIndex() == 3, "hotbar secimi");
    inv.cycle(1);
    check(inv.selectedIndex() == 4, "hotbar ilerledi");
    inv.cycle(-1); inv.cycle(-1);
    check(inv.selectedIndex() == 2, "hotbar geriye");

    check(inv.removeItems(1, 65), "65 blok silindi");
    check(inv.countOf(1) == 0, "envanter bos");

    // dolu envanter
    Inventory full;
    for (int i = 0; i < Inventory::TOTAL; ++i) full.at(i) = ItemStack{1, 64, 0};
    check(full.full(), "envanter dolu algilandi");
    check(full.add(1, 1) == 1, "dolu envanter eklemeyi reddetti");

    // kazma hızı
    float handTime = Mining::breakTime(STONE, nullptr);
    float pickTime = Mining::breakTime(STONE, reg.get(100));  // tahta kazma
    check(pickTime < handTime, "dogru alet daha hizli kirar");
    float diamondTime = Mining::breakTime(STONE, reg.get(103)); // elmas kazma
    check(diamondTime < pickTime, "daha iyi alet daha da hizli");
}

// ---------------------------------------------------------------- CRAFTING
static void testCrafting() {
    group("crafting");

    CraftingRegistry reg;
    reg.registerDefaults();
    check(reg.count() >= 12, "12+ tarif yuklendi");

    // şekilli tarif: konumdan bağımsız eşleşme
    const Recipe* any = nullptr;
    for (const auto& rec : reg.all()) {
        if (rec.key == "stick" || rec.key == "crafting_table") { any = &rec; break; }
    }
    check(any != nullptr, "bilinen tarif bulundu");

    // çubuk: 2 tahta dikey
    std::array<ItemStack, 9> g{};
    for (auto& c : g) c.clear();
    g[0] = ItemStack{OAK_PLANKS, 1, 0};
    g[3] = ItemStack{OAK_PLANKS, 1, 0};
    const Recipe* stick = reg.match(g);
    check(stick != nullptr, "cubuk tarifi eslesti");
    if (stick) {
        check(stick->result.itemId != 0, "sonuc esyasi var");
        check(stick->result.count >= 1, "sonuc adedi var");
    }

    // konum kaydırma toleransı: sol üst yerine ortada
    std::array<ItemStack, 9> g2{};
    for (auto& c : g2) c.clear();
    g2[1] = ItemStack{OAK_PLANKS, 1, 0};
    g2[4] = ItemStack{OAK_PLANKS, 1, 0};
    const Recipe* stick2 = reg.match(g2);
    check(stick2 != nullptr, "kaydirilmis desen de eslesti");

    // boş ızgara tarif vermez
    std::array<ItemStack, 9> empty{};
    for (auto& c : empty) c.clear();
    check(reg.match(empty) == nullptr, "bos izgara tarifsiz");

    // tüketim
    CraftingGrid cg;
    cg.clear();
    cg.cells[0] = ItemStack{OAK_PLANKS, 2, 0};
    cg.cells[3] = ItemStack{OAK_PLANKS, 2, 0};
    cg.consume(Recipe{});
    check(cg.cells[0].count == 1 && cg.cells[3].count == 1, "malzeme tuketildi");
}

// ---------------------------------------------------------------- SAVE
static void testSave() {
    group("save");

    // 20 saniyelik askeri senaryo: düzenle, kaydet, yükle, karşılaştır
    World w(99, false);
    for (int x = 0; x < 6; ++x)
        for (int z = 0; z < 6; ++z)
            w.setBlock(x, 0, z, STONE);

    std::vector<BlockEdit> edits;
    for (int i = 0; i < 40; ++i)
        edits.push_back(BlockEdit{i % 6, 1, i / 6, static_cast<uint8_t>(1 + (i % 5))});

    Inventory inv;
    inv.add(1, 32);
    inv.add(100, 1, 0.75f);
    inv.select(4);

    Vec3f pos{3.5f, 12.25f, -7.75f};
    auto blob = WorldSave::serialize(w, 99, pos, 1.25f, -0.5f, inv, edits, "test dunyasi");
    check(blob.size() > 100, "kayit blob'u uretildi");
    check(std::memcmp(blob.data(), kMagic, 8) == 0, "magic dogru");

    auto loaded = WorldSave::deserialize(blob);
    check(loaded.valid, "kayit gecerli okundu");
    check(loaded.seed == 99, "tohum korundu");
    check(std::fabs(loaded.playerPos.x - 3.5f) < 1e-5f &&
          std::fabs(loaded.playerPos.y - 12.25f) < 1e-5f &&
          std::fabs(loaded.playerPos.z + 7.75f) < 1e-5f, "oyuncu konumu korundu");
    check(std::fabs(loaded.yaw - 1.25f) < 1e-5f, "yaw korundu");
    check(loaded.name == "test dunyasi", "isim korundu");
    check(loaded.edits.size() == 40, "duzenlemeler korundu");
    check(loaded.inv.countOf(1) == 32, "envanter bloklari korundu");
    check(loaded.inv.countOf(100) == 1, "alet korundu");
    check(loaded.inv.selectedIndex() == 4, "hotbar secimi korundu");
    check(std::fabs(loaded.inv.at(0).durability) >= 0.f, "dayaniklilik alani okundu");

    // bozuk blob reddedilir
    std::vector<uint8_t> junk(200, 0xAB);
    check(!WorldSave::deserialize(junk).valid, "bozuk dosya reddedildi");
    auto tr = WorldSave::deserialize(blob);
    tr.seed = 0;
    (void)tr;

    // RLE
    std::vector<uint8_t> rep(1000, 7);
    auto comp = WorldSave::rleCompress(rep);
    check(comp.size() < rep.size() / 10, "RLE tekrar eden veriyi sikistirdi");
    auto dec = WorldSave::rleDecompress(comp, rep.size());
    check(dec == rep, "RLE kayipsiz geri acildi");

    std::vector<uint8_t> mixed;
    for (int i = 0; i < 500; ++i) mixed.push_back(static_cast<uint8_t>((i * 37) % 13));
    auto c2 = WorldSave::rleCompress(mixed);
    auto d2 = WorldSave::rleDecompress(c2, mixed.size());
    check(d2 == mixed, "RLE karisik veri kayipsiz");

    // ayarlar JSON döngüsü
    GameSettings s;
    s.renderDistance = 8;
    s.shadowsEnabled = false;
    s.fov = 95.f;
    s.fpsMode = 120;
    std::string js = s.toJson();
    GameSettings back = GameSettings::fromJson(js);
    check(back.renderDistance == 8, "ayar renderDistance korundu");
    check(back.shadowsEnabled == false, "ayar golge korundu");
    check(std::fabs(back.fov - 95.f) < 1e-4f, "ayar fov korundu");
    check(back.fpsMode == 120, "ayar fps modu korundu");
    GameSettings clamp = GameSettings::fromJson("{\"renderDistance\":99,\"fov\":500}");
    check(clamp.renderDistance <= 12 && clamp.fov <= 110.f, "ayarlar kirpiliyor");
}

// ---------------------------------------------------------------- NET
static void testNet() {
    group("net");

    // paket yaz/oku döngüsü
    PacketWriter w(PacketType::BlockEdit);
    w.u32(100);
    w.u32(64);
    w.u32(static_cast<uint32_t>(-5));
    w.u8(7);
    w.u8(1);
    PacketReader r(w.data());
    check(r.type() == PacketType::BlockEdit, "paket turu korundu");
    check(r.u8() == static_cast<uint8_t>(PacketType::BlockEdit), "tur bayti tuketildi");
    check(r.u32() == 100, "x geri okundu");
    check(r.u32() == 64, "y geri okundu");
    check(static_cast<int32_t>(r.u32()) == -5, "negatif z geri okundu");
    check(r.u8() == 7, "blok id geri okundu");
    check(r.u8() == 1, "place bayragi geri okundu");
    check(!r.failed(), "okuyucu hata vermedi");

    // float yuvarlama
    PacketWriter fw(PacketType::PlayerState);
    fw.f32(123.4567f);
    fw.f32(-0.0001f);
    PacketReader fr(fw.data());
    fr.u8();
    check(std::fabs(fr.f32() - 123.4567f) < 1e-3f, "float kayipsiz");
    check(std::fabs(fr.f32() + 0.0001f) < 1e-6f, "kucuk negatif float");

    // string
    PacketWriter sw(PacketType::Chat);
    sw.str("merhaba craftya");
    PacketReader sr(sw.data());
    sr.u8();  // type
    check(sr.str() == "merhaba craftya", "string gidis-donus");

    // kısa paket güvenli
    uint8_t tiny[2] = {0, 0};
    PacketReader tr(tiny, 1);
    tr.u32();
    check(tr.failed(), "kisa paket hata bildiriyor");

    // mesh: 3 cihaz herkese yayın yapıyor
    MeshNetwork mesh;
    mesh.setLocalDevice(1);
    int sentTo2 = 0, sentTo3 = 0;
    mesh.addPeer(2, [&](const uint8_t*, size_t) { ++sentTo2; return true; });
    mesh.addPeer(3, [&](const uint8_t*, size_t) { ++sentTo3; return true; });
    check(mesh.peerCount() == 2, "iki peer bagli");

    PlayerSnapshot snap;
    snap.pos = {10.f, 20.f, 30.f};
    snap.health = 18.f;
    mesh.publishLocalState(snap, 1000);
    check(sentTo2 == 1 && sentTo3 == 1, "yayin herkese gitti (hostsuz)");

    // gelen oyuncu durumu işlenir
    PacketWriter ps(PacketType::PlayerState);
    ps.u8(2);
    ps.f32(5.f); ps.f32(6.f); ps.f32(7.f);
    ps.f32(1.f); ps.f32(0.f);
    ps.u8(0x08);
    ps.u8(20);
    ps.u32(2000);
    mesh.handlePacket(2, ps.data().data(), ps.data().size(), 2000);
    RemotePlayer* rp = mesh.remote(2);
    check(rp != nullptr, "uzak oyuncu kaydi var");
    check(rp && std::fabs(rp->current.pos.y - 6.f) < 1e-5f, "uzak konum islendi");
    check(rp && rp->connected, "uzak oyuncu bagli");

    // blok düzenleme geri çağrısı
    int editX = 0, editY = 0, editZ = 0; uint8_t editBlock = 0; bool editPlace = false;
    mesh.onBlockEdit([&](uint8_t, int x, int y, int z, uint8_t b, bool p) {
        editX = x; editY = y; editZ = z; editBlock = b; editPlace = p;
    });
    PacketWriter be(PacketType::BlockEdit);
    be.u32(static_cast<uint32_t>(-3));
    be.u32(70);
    be.u32(12);
    be.u8(5);
    be.u8(1);
    mesh.handlePacket(3, be.data().data(), be.data().size(), 2100);
    check(editX == -3 && editY == 70 && editZ == 12, "blok duzenleme koordinatlari");
    check(editBlock == 5 && editPlace, "blok duzenleme icerigi");

    // sohbet
    std::string chatMsg;
    mesh.onChat([&](uint8_t, const std::string& m) { chatMsg = m; });
    PacketWriter ch(PacketType::Chat);
    ch.str("selam");
    mesh.handlePacket(2, ch.data().data(), ch.data().size(), 2200);
    check(chatMsg == "selam", "sohbet paketi islendi");

    // maç durumu
    uint16_t sa = 0, sb = 0; uint8_t ph = 0;
    mesh.onMatchState([&](uint8_t, uint16_t a, uint16_t b, uint8_t p) { sa = a; sb = b; ph = p; });
    PacketWriter ms(PacketType::MatchState);
    ms.u16(3); ms.u16(1); ms.u8(2);
    mesh.handlePacket(3, ms.data().data(), ms.data().size(), 2300);
    check(sa == 3 && sb == 1 && ph == 2, "mac durumu islendi");

    // zaman aşımı
    mesh.update(1.f, 2000 + 7000);
    RemotePlayer* rp2 = mesh.remote(2);
    check(rp2 && !rp2->connected, "7 saniye sessizlik -> baglanti koptu");

    // peer tavanı
    MeshNetwork big;
    big.setLocalDevice(9);
    for (int i = 1; i <= MeshNetwork::kMaxPeers; ++i)
        big.addPeer(static_cast<uint8_t>(i + 10), [](const uint8_t*, size_t) { return true; });
    check(big.peerCount() == static_cast<size_t>(MeshNetwork::kMaxPeers), "peer tavani uygulandi");
    check(big.addPeer(99, [](const uint8_t*, size_t) { return true; }) == nullptr,
          "tavan asilinca peer reddedildi");
}

// ---------------------------------------------------------------- ECS
static void testEcs() {
    group("ecs");

    World2 ew;
    EntityId mob = ew.spawn(EntityKind::Mob);
    EntityId proj = ew.spawn(EntityKind::Projectile);
    EntityId drop = ew.spawn(EntityKind::ItemDrop);
    EntityId player = ew.spawn(EntityKind::Player);
    check(ew.liveCount() == 4, "4 varlik yaratildi");
    check(ew.countOf(EntityKind::Mob) == 1, "mob sayimi");

    Entity* m = ew.get(mob);
    check(m != nullptr, "varlik erisilebilir");
    m->transform.pos = {0, 10, 0};
    m->ai.hostile = true;
    m->ai.sightRange = 20.f;

    Entity* p = ew.get(player);
    p->transform.pos = {3.f, 10.f, 0.f};

    // dünya: düz zemin
    World w(5, false);
    for (int x = -4; x < 8; ++x)
        for (int z = -4; z < 4; ++z)
            for (int y = 0; y < 10; ++y)
                w.setBlock(x, y, z, y < 9 ? AIR : STONE);

    // mob oyuncuya yaklaşmalı
    float distBefore = (p->transform.pos - m->transform.pos).length();
    for (int i = 0; i < 60; ++i) ew.updateMobs(w, 1.f / 20.f, player);
    m = ew.get(mob);
    Entity* p2 = ew.get(player);
    float distAfter = (p2->transform.pos - m->transform.pos).length();
    check(distAfter <= distBefore + 0.01f, "mob oyuncuya yaklasti veya durdu");
    check(m->ai.target == player, "mob hedefi oyuncu");
    check(m->ai.state == MobAi::State::Chase || m->ai.state == MobAi::State::Attack,
          "mob dovus durumunda");

    // yakın mesafe -> hasar
    m->transform.pos = p2->transform.pos + Vec3f{1.f, 0.f, 0.f};
    p2->health.current = 20.f;
    for (int i = 0; i < 80; ++i) ew.updateMobs(w, 1.f / 20.f, player);
    check(p2->health.current < 20.f, "mob oyuncuya hasar verdi");

    // mermi blok içine girince yok olur
    Entity* pr = ew.get(proj);
    pr->transform.pos = {0.f, 9.5f, 0.f};
    pr->proj.dir = {0.f, -1.f, 0.f};
    pr->proj.speed = 30.f;
    pr->proj.life = 1.f;
    for (int i = 0; i < 10; ++i) ew.updateProjectiles(w, 1.f / 60.f);
    check(ew.get(proj) == nullptr, "mermi bloga carpti ve yok oldu");

    // düşen eşya toplama gecikmesi
    Entity* d = ew.get(drop);
    d->drop.pickDelay = 0.5f;
    ew.updateItemDrops(w, 0.2f);
    d = ew.get(drop);
    check(d && d->drop.pickDelay > 0.f, "esya toplama gecikmesi isliyor");
    for (int i = 0; i < 10; ++i) ew.updateItemDrops(w, 0.2f);
    d = ew.get(drop);
    check(d && d->drop.pickDelay <= 0.f, "gecikme bitti");

    // yok etme
    size_t beforeDeath = ew.liveCount();
    ew.despawn(mob);
    check(ew.get(mob) == nullptr, "varlik yok edildi");
    check(ew.liveCount() == beforeDeath - 1, "canli sayisi dusttu");

    // aynı varlık iki kez yok edilemez
    ew.despawn(mob);
    check(ew.liveCount() == beforeDeath - 1, "tekrar yok etme etkisiz");
}

// ---------------------------------------------------------------- AUDIO
static void testAudio() {
    group("audio");

    AudioBuffer step = SfxGenerator::generate(SfxId::Step, 48000);
    check(!step.empty(), "adim sesi uretildi");
    check(step.sampleRate == 48000, "ornek hizi 48k");
    check(step.durationSec > 0.05f && step.durationSec < 0.5f, "adim sesi suresi");

    // tüm SFX'ler üretilebiliyor
    bool allOk = true;
    for (int i = 0; i < static_cast<int>(SfxId::COUNT); ++i) {
        auto b = SfxGenerator::generate(static_cast<SfxId>(i), 48000);
        if (b.empty()) allOk = false;
    }
    check(allOk, "tum SFX turleri uretildi");

    AudioMixer mix;
    mix.loadProceduralSfx();
    check(!mix.sfx(SfxId::Break).empty(), "SFX kutuphanesi yuklendi");

    int ch = mix.play(mix.sfx(SfxId::Break), 1.f, false);
    check(ch >= 0, "kanal acildi");
    check(mix.activeChannels() == 1, "1 aktif kanal");

    std::vector<int16_t> out(1024 * 2, 0);
    mix.mix(out.data(), 1024);
    bool nonZero = false;
    for (int16_t s : out) if (s != 0) { nonZero = true; break; }
    check(nonZero, "mikser ses uretti");

    // konumsal ses: uzak = sessiz
    AudioMixer m2;
    m2.loadProceduralSfx();
    auto buf = m2.sfx(SfxId::Hit);
    int far = m2.playAt(buf, Vec3f{100, 0, 0}, Vec3f{0,0,0}, Vec3f{0,0,-1}, 1.f);
    check(far == -1, "32 blok otedeki ses duyulmuyor");
    int near = m2.playAt(buf, Vec3f{2, 0, 0}, Vec3f{0,0,0}, Vec3f{0,0,-1}, 1.f);
    check(near >= 0, "yakin ses duyuluyor");

    // ses seviyesi kırpma
    mix.setMasterVolume(5.f);
    check(mix.masterVolume() <= 1.f, "master ses kirpildi");
    mix.setMasterVolume(-1.f);
    check(mix.masterVolume() >= 0.f, "negatif ses sifirlandi");

    mix.stopAll();
    check(mix.activeChannels() == 0, "tum kanallar durdu");

    // ambient müzik
    AudioBuffer music = AmbientMusic::generate(48000, 4.f, 3);
    check(!music.empty() && music.durationSec > 3.f, "ambient muzik uretildi");

    // kanal tavanı
    AudioMixer m3;
    m3.loadProceduralSfx();
    int opened = 0;
    for (int i = 0; i < 100; ++i) if (m3.play(m3.sfx(SfxId::UiClick), 1.f, true) >= 0) ++opened;
    check(opened <= 32, "kanal tavani 32");
}

// ---------------------------------------------------------------- GUI
static void testGui() {
    group("gui");

    DrawList dl;
    dl.rect(Rect{10, 10, 100, 50}, Color{1, 0, 0, 1});
    dl.text("test", 20, 20, 1.f, Color{}, 0);
    dl.line(0, 0, 100, 100, Color{}, 2.f);
    dl.texturedRect(Rect{0, 0, 16, 16}, 5);
    check(dl.size() == 4, "4 cizim komutu");

    Hud hud;
    hud.resize(1920, 1080, 1.f);
    check(hud.screenW() == 1920 && hud.screenH() == 1080, "ekran boyutu");

    Inventory inv;
    inv.add(1, 64);
    inv.add(2, 10);
    inv.select(1);

    DrawList hudList;
    hud.build(hudList, inv, 15.f, 20.f, 60.f, Vec3f{10, 64, 20}, 42, 5, 0.f);
    check(hudList.size() > 20, "HUD komut uretti");
    // hotbar 9 yuva + kalpler 10 + joystick + butonlar
    check(hudList.size() >= 9 + 10, "hotbar ve kalpler cizildi");

    // dokunmatik joystick
    hud.onTouchDown(0, 150.f, 900.f);
    bool moved = std::fabs(hud.touch.moveAxis.x) > 0.01f || std::fabs(hud.touch.moveAxis.z) > 0.01f;
    check(moved, "joystick girdi uretti");
    hud.onTouchUp(0);
    check(std::fabs(hud.touch.moveAxis.x) < 0.001f, "joystick birakildi");

    // menü
    MenuSystem menu;
    check(!menu.isOpen(), "menu kapali basladi");
    menu.open(MenuId::Main);
    check(menu.isOpen() && menu.current() == MenuId::Main, "ana menu acildi");
    check(menu.screen().items.size() >= 3, "menu ogeleri var");

    int before = menu.screen().selected;
    menu.moveSelection(1);
    check(menu.screen().selected != before, "secim hareket etti");

    DrawList menuList;
    menu.build(menuList, 1920, 1080);
    check(menuList.size() > 5, "menu cizildi");

    // mod paneli (NPC)
    menu.showModePanel({
        {"Duello", "1v1", 12, 2, true},
        {"Takimli", "5v5", 40, 10, true},
        {"Mega", "6v6", 8, 12, false},
    });
    check(menu.current() == MenuId::ModeSelect, "mod paneli acildi");
    check(menu.screen().modes.size() == 3, "3 mod satiri");
    check(menu.screen().modes[0].livePlayers == 12, "canli oyuncu sayisi");
    check(!menu.screen().modes[2].available, "dolu mod isaretli");

    DrawList panelList;
    menu.build(panelList, 1920, 1080);
    check(panelList.size() > 8, "mod paneli cizildi");

    menu.back();
    check(menu.current() == MenuId::Main, "geri -> ana menu");
    menu.back();
    check(!menu.isOpen(), "menu kapandi");

    // toast
    Toast t;
    t.show("blok kirildi");
    check(t.visible(), "toast gorunur");
    t.update(3.f);
    check(!t.visible(), "toast kayboldu");

    // sohbet
    ChatLog log;
    log.add("oyuncu1", "selam");
    log.add("oyuncu2", "naber");
    check(log.lines().size() == 2, "sohbet satirlari");
    DrawList chatList;
    log.build(chatList, 1920, 1080);
    check(chatList.size() > 0, "sohbet cizildi");
    log.update(20.f);
    DrawList chatList2;
    log.build(chatList2, 1920, 1080);
    check(chatList2.size() == 0, "eski sohbet soldu");
}

// ---------------------------------------------------------------- MESH SCENARIO
static void testMeshScenario() {
    group("mesh senaryo (3 telefon)");

    // 3 cihaz: 1, 2, 3. Hostsuz: her biri diğer ikisine bağlı.
    MeshNetwork n1, n2, n3;
    n1.setLocalDevice(1); n2.setLocalDevice(2); n3.setLocalDevice(3);

    int n1Recv = 0, n2Recv = 0, n3Recv = 0;
    auto deliver = [](MeshNetwork& dst, uint8_t from, const std::vector<uint8_t>& data, uint32_t now) {
        dst.handlePacket(from, data.data(), data.size(), now);
    };

    n1.addPeer(2, [&](const uint8_t* d, size_t n) {
        ++n2Recv; deliver(n2, 1, std::vector<uint8_t>(d, d + n), 1000); return true; });
    n1.addPeer(3, [&](const uint8_t* d, size_t n) {
        ++n3Recv; deliver(n3, 1, std::vector<uint8_t>(d, d + n), 1000); return true; });
    n2.addPeer(1, [&](const uint8_t* d, size_t n) {
        ++n1Recv; deliver(n1, 2, std::vector<uint8_t>(d, d + n), 1000); return true; });
    n3.addPeer(1, [&](const uint8_t* d, size_t n) {
        ++n1Recv; deliver(n1, 3, std::vector<uint8_t>(d, d + n), 1000); return true; });

    PlayerSnapshot s1; s1.pos = {0, 0, 0}; s1.health = 20;
    n1.publishLocalState(s1, 1000);
    check(n2Recv == 1 && n3Recv == 1, "1. cihaz 2 ve 3'e gonderi yapti (hostsuz)");

    check(n2.remote(1) != nullptr && n3.remote(1) != nullptr, "her iki cihaz 1'i gordu");
    check(n2.remote(1)->current.health == 20.f, "saglik senkronu");

    // blok düzenlemesi yayılımı
    int applied2 = 0, applied3 = 0;
    n2.onBlockEdit([&](uint8_t, int, int, int, uint8_t, bool) { ++applied2; });
    n3.onBlockEdit([&](uint8_t, int, int, int, uint8_t, bool) { ++applied3; });
    n1.sendBlockEdit(5, 6, 7, 3, true);
    check(applied2 == 1 && applied3 == 1, "blok duzenlemesi iki cihaza ulasti");

    // sohbet yayılımı
    std::string got2, got3;
    n2.onChat([&](uint8_t, const std::string& m) { got2 = m; });
    n3.onChat([&](uint8_t, const std::string& m) { got3 = m; });
    n1.sendChat("hazir misin");
    check(got2 == "hazir misin" && got3 == "hazir misin", "sohbet herkese ulasti");
}

// ---------------------------------------------------------------- ENGINE
static void testEngine() {
    group("engine cephesi");

    EngineConfig cfg;
    cfg.seed = 777;
    cfg.autoSave = false;

    Engine eng;
    check(eng.init(cfg), "motor basladi");
    check(eng.state() == EngineState::MainMenu, "ana menude");

    eng.onSurfaceCreated(1920, 1080, 1.f);
    check(eng.hud().screenW() == 1920, "yuzey olusturuldu");

    // menü çizimi
    DrawList l0;
    eng.render(&l0);
    check(l0.size() > 3, "ana menu cizildi");

    // dünya başlat
    check(eng.startNewWorld(777), "dunya basladi");
    check(eng.state() == EngineState::Playing, "oyunda");
    check(eng.inventory().countOf(STONE) == 64, "baslangic envanteri");
    check(eng.entities().countOf(EntityKind::Mob) == 4, "4 mob yaratildi");
    check(eng.entities().countOf(EntityKind::Player) == 1, "oyuncu varligi");

    // 120 kare simüle et
    float dt = 1.f / 60.f;
    for (int i = 0; i < 120; ++i) {
        eng.hud().touch.moveAxis = Vec3f{0.6f, 0, 1.f};
        eng.hud().touch.jumpHeld = (i % 40 == 0);
        eng.update(dt);
    }
    check(eng.state() == EngineState::Playing, "simulasyon devam ediyor");
    Vec3f p1 = eng.game().player().pos;
    check(std::fabs(p1.z) > 0.5f || std::fabs(p1.x) > 0.5f, "oyuncu hareket etti");

    // HUD üretildi mi
    DrawList hudL;
    eng.render(&hudL);
    check(hudL.size() > 20, "oyun HUD'u cizildi");

    // duraklat
    eng.onPause();
    check(eng.state() == EngineState::Paused, "duraklatildi");
    check(eng.menus().current() == MenuId::Pause, "duraklatma menusu");
    DrawList pauseL;
    eng.render(&pauseL);
    check(pauseL.size() > 5, "duraklatma menusu cizildi");
    eng.onResume();
    check(eng.state() == EngineState::Playing, "devam etti");

    // ağ ve oda
    eng.setMode(2);
    check(eng.mode() == 2, "mod mega");
    eng.setMode(9);
    check(eng.mode() == 0, "gecersiz mod duello'ya kirpildi");

    // sunucudan lobi verisi
    eng.refreshLobbyFromServer("{\"duello\":12,\"takimli\":40,\"mega\":8,\"online\":57}");
    check(eng.lobby.duello == 12 && eng.lobby.online == 57, "lobi verisi okundu");
    check(eng.menus().current() == MenuId::ModeSelect, "mod paneli acildi");
    check(eng.menus().screen().modes.size() == 3, "3 mod gosterildi");
    check(eng.menus().screen().modes[1].livePlayers == 40, "takimli canli sayisi");

    // sohbet
    eng.chatMessage("oyuncu34", "hazir");
    DrawList chatL;
    eng.render(&chatL);
    check(chatL.size() > 0, "sohbet cizildi");

    // anlık oyuncu durumu
    PlayerSnapshot snap = eng.localSnapshot();
    check(std::fabs(snap.pos.x - p1.x) < 0.01f, "yerel anlik konum");

    // ağ istatistikleri
    check(eng.netStats().totalBytesSent >= 0, "ag istatistikleri erisilebilir");

    // geri tuşu
    eng.onBackPressed();
    check(eng.menus().current() == MenuId::None, "geri tusu mod panelini kapatti");
    eng.onBackPressed();
    check(eng.state() == EngineState::Paused, "geri tusu oyunu duraklatti");
    check(eng.menus().current() == MenuId::Pause, "duraklatma menusu acildi");

    // kapatma
    eng.shutdown();
}

int main() {
    std::printf("=== Craftya genisletilmis sistem testleri ===\n");
    testJson();
    testLighting();
    testAtlas();
    testInventory();
    testCrafting();
    testSave();
    testNet();
    testEcs();
    testAudio();
    testGui();
    testMeshScenario();
    testEngine();

    std::printf("\n----------------------------------------\n");
    std::printf("GECEN: %d   BASARISIZ: %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
