// Craftya — Android NativeActivity giriş noktası
// Sorumluluk: pencere/yüzey yaşam döngüsü, girdi, ana döngü.
// Tüm oyun mantığı engine/ içinde; bu dosya ince bir kabuk.
#include <android_native_app_glue.h>
#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <time.h>
#include <cstring>
#include <cmath>
#include <string>
#include <unordered_map>

#include "craftya/engine.hpp"
#include "craftya/atlas.hpp"
#include "gles_backend.hpp"
#include "ui_renderer.hpp"
#include "audio_android.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "Craftya", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Craftya", __VA_ARGS__)

using namespace craftya;

namespace {

struct AppState {
    android_app* app = nullptr;
    EngineConfig cfg;          // yüzey gelince motoru başlatmak için saklanır
    Engine engine;
    GLESBackend gl;
    UiRenderer ui;
    DrawList drawList;
    std::unordered_map<ChunkPos, GpuMesh, ChunkPosHash> gpuMeshes;
    bool engineReady = false;
    bool atlasUploaded = false;
    bool hasSurface = false;
    bool running = true;
    double lastTime = 0.0;
    float fpsAvg = 60.f;
    int width = 0, height = 0;
    float dpi = 1.f;
    // dokunmatik durum
    bool backPressed = false;
    // dünyanın ilk kez çizildiği an (yükleme ekranı için)
    double worldShownAt = 0.0;
    double bootAt = 0.0;
    // ses
    AndroidAudio audio;
    std::vector<int16_t> mixBuf;      // stereo interleaved karıştırma tamponu
    // titreşim
    int64_t lastVibrateAt = 0;
};

// ---- ses karıştırma köprüsü (AAudio callback'ten çağrılır) ----
AppState* g_audioState = nullptr;
void mixAudio(int16_t* out, int frames) {
    if (!g_audioState) return;
    AppState& st = *g_audioState;
    if (!st.engineReady) return;
    st.mixBuf.resize(static_cast<size_t>(frames) * 2, 0);
    st.engine.audio().mix(st.mixBuf.data(), frames);
}

double nowSeconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

// ---- titreşim (JNI, ek izin gerekmez: VIBRATE normalde otomatik) ----
void vibrate(android_app* app, int ms) {
    if (!app || !app->activity || !app->activity->vm || !app->activity->clazz) return;
    JNIEnv* env = nullptr;
    JavaVM* vm = app->activity->vm;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
    jobject activity = app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    if (cls) {
        jmethodID mid = env->GetMethodID(cls, "getSystemService",
                                        "(Ljava/lang/String;)Ljava/lang/Object;");
        jfieldID fid = env->GetStaticFieldID(cls, "VIBRATOR_SERVICE", "Ljava/lang/String;");
        if (mid && fid) {
            jstring svc = static_cast<jstring>(env->GetStaticObjectField(cls, fid));
            jobject vib = env->CallObjectMethod(activity, mid, svc);
            if (vib) {
                jclass vc = env->GetObjectClass(vib);
                jmethodID vm2 = env->GetMethodID(vc, "vibrate", "(J)V");
                if (vm2) env->CallVoidMethod(vib, vm2, static_cast<jlong>(ms));
                env->DeleteLocalRef(vc);
                env->DeleteLocalRef(vib);
            }
            if (svc) env->DeleteLocalRef(svc);
        }
        env->DeleteLocalRef(cls);
    }
    vm->DetachCurrentThread();
}

void handleCmd(android_app* app, int32_t cmd) {
    auto* st = static_cast<AppState*>(app->userData);
    if (!st) return;

    switch (cmd) {
        case APP_CMD_WINDOW_RESIZED:
            if (app->window && st->hasSurface) {
                int w = ANativeWindow_getWidth(app->window);
                int h = ANativeWindow_getHeight(app->window);
                st->width = w; st->height = h;
                st->gl.resize(w, h);
                st->engine.onSurfaceChanged(w, h, st->dpi);
            }
            break;
        case APP_CMD_INIT_WINDOW: {
            if (!app->window) return;
            // motoru YUZEY HAZIRKEN baslat (GL baglami gerekli)
            if (!st->engineReady) {
                st->engine.init(st->cfg);
                st->engineReady = true;
                // ses çıkışını başlat (AAudio yoksa sessiz devam eder)
                g_audioState = st;
                st->audio.start(&mixAudio, 48000);
                LOGI("motor hazir");
            }
            int w = ANativeWindow_getWidth(app->window);
            int h = ANativeWindow_getHeight(app->window);
            st->width = w;
            st->height = h;
            st->dpi = static_cast<float>(AConfiguration_getDensity(app->config)) / 160.f;
            if (st->dpi <= 0.f) st->dpi = 2.75f;

            if (!st->gl.valid() && !st->gl.initSurface(app->window, w, h)) {
                LOGE("GLES baslatilamadi: %s", st->gl.lastError().c_str());
                ANativeActivity_finish(app->activity);
                return;
            }
            st->gl.resize(w, h);
            st->hasSurface = true;
            st->engine.onSurfaceCreated(w, h, st->dpi);
            st->atlasUploaded = false;      // yeni bağlam → yeniden yükle
            st->gpuMeshes.clear();
            LOGI("yuzey hazir %dx%d dpi=%.2f", w, h, st->dpi);
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            st->hasSurface = false;
            for (auto& kv : st->gpuMeshes) st->gl.freeMesh(kv.second);
            st->gpuMeshes.clear();
            st->engine.onSurfaceLost();
            st->gl.destroySurface();
            break;
        }
        case APP_CMD_CONFIG_CHANGED: {
            if (app->window && st->hasSurface) {
                int w = ANativeWindow_getWidth(app->window);
                int h = ANativeWindow_getHeight(app->window);
                if (w != st->width || h != st->height) {
                    st->width = w; st->height = h;
                    st->gl.resize(w, h);
                    st->engine.onSurfaceChanged(w, h, st->dpi);
                }
            }
            break;
        }
        case APP_CMD_GAINED_FOCUS: st->engine.onResume(); break;
        case APP_CMD_LOST_FOCUS:   st->engine.onPause(); break;
        case APP_CMD_PAUSE:        st->engine.onPause(); break;
        case APP_CMD_RESUME:       st->engine.onResume(); break;
        case APP_CMD_DESTROY:      st->running = false; break;
        default: break;
    }
}

int32_t handleInput(android_app* app, AInputEvent* ev) {
    auto* st = static_cast<AppState*>(app->userData);
    if (!st) return 0;

    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(ev);
        int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
        int32_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                      >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        size_t count = AMotionEvent_getPointerCount(ev);
        for (size_t i = 0; i < count; ++i) {
            int id = AMotionEvent_getPointerId(ev, i);
            float x = AMotionEvent_getX(ev, i);
            float y = AMotionEvent_getY(ev, i);
            bool down = (masked == AMOTION_EVENT_ACTION_DOWN && i == 0) ||
                        (masked == AMOTION_EVENT_ACTION_POINTER_DOWN &&
                         static_cast<int32_t>(i) == idx);
            bool up = (masked == AMOTION_EVENT_ACTION_UP && i == 0) ||
                      (masked == AMOTION_EVENT_ACTION_POINTER_UP &&
                       static_cast<int32_t>(i) == idx) ||
                      masked == AMOTION_EVENT_ACTION_CANCEL;

            if (down) st->engine.onTouchDown(id, x, y);
            else if (up) st->engine.onTouchUp(id);
            else st->engine.onTouchMove(id, x, y);
        }
        return 1;
    }

    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        int32_t code = AKeyEvent_getKeyCode(ev);
        int32_t act = AKeyEvent_getAction(ev);
        if (code == AKEYCODE_BACK) {
            if (act == AKEY_EVENT_ACTION_UP) st->engine.onBackPressed();
            return 1;   // sistemin uygulamayı kapatmasını engelle
        }
        st->engine.onKey(code, act == AKEY_EVENT_ACTION_DOWN);
        return 1;
    }
    return 0;
}

// chunk mesh'i GPU'ya senkronize et
void syncMeshes(AppState& st) {
    if (!st.engineReady) return;
    Game& g = st.engine.game();

    // görünür chunk'ları yükle
    for (const auto& vc : g.visibleChunks()) {
        if (!vc.mesh) continue;
        auto it = st.gpuMeshes.find(vc.pos);
        if (it == st.gpuMeshes.end()) {
            GpuMesh m;
            st.gl.uploadMesh(m, *vc.mesh, false);
            st.gpuMeshes.emplace(vc.pos, m);
        }
    }

    // kaybolan chunk'ları serbest bırak (bellek sızıntısı yok)
    if (st.gpuMeshes.size() > 600) {
        std::vector<ChunkPos> dead;
        for (auto& kv : st.gpuMeshes) {
            bool used = false;
            for (const auto& vc : g.visibleChunks())
                if (vc.pos == kv.first) { used = true; break; }
            if (!used) dead.push_back(kv.first);
        }
        for (const auto& p : dead) {
            auto it = st.gpuMeshes.find(p);
            if (it != st.gpuMeshes.end()) {
                st.gl.freeMesh(it->second);
                st.gpuMeshes.erase(it);
            }
        }
    }
}

void renderFrame(AppState& st) {
    if (!st.hasSurface || !st.engineReady) return;

    // doku atlası: ilk karede üret ve GPU'ya yükle (cihazda üretilir, dosya yok)
    if (!st.atlasUploaded) {
        Atlas atlas;
        atlas.generateProcedural(20261124u);
        st.gl.uploadAtlas(atlas.image());
        st.atlasUploaded = true;
        LOGI("atlas yuklendi: %dx%d", atlas.image().width, atlas.image().height);
    }

    // DİKKAT: game() yalnızca dünya başladıktan sonra geçerlidir.
    // Ana menüde çağrılırsa nullptr dereference ile ÇÖKER.
    const bool inWorld = (st.engine.state() == EngineState::Playing);

    const float sky[3] = {0.53f, 0.72f, 0.98f};
    st.gl.beginFrame(sky, 60.f, 96.f);

    if (inWorld) {
        Game& g = st.engine.game();
        st.gl.setCamera(g.camera(), static_cast<float>(st.width) / static_cast<float>(st.height));
        st.gl.setSun(1.f, nullptr);
        syncMeshes(st);
        for (const auto& vc : g.visibleChunks()) {
            auto it = st.gpuMeshes.find(vc.pos);
            if (it == st.gpuMeshes.end() || !it->second.uploaded) continue;
            st.gl.drawMesh(it->second, Vec3f(static_cast<float>(vc.pos.x * CHUNK_W),
                                             static_cast<float>(vc.pos.y * CHUNK_H),
                                             static_cast<float>(vc.pos.z * CHUNK_W)));
        }
        if (st.worldShownAt == 0.0) {
            st.worldShownAt = nowSeconds();
            LOGI("dunya cizildi (acilistan %.1f sn)", st.worldShownAt - st.bootAt);
        }
    }

    // arayüz (menüler dahil — oyun olmasa da çizilir)
    st.drawList.clear();
    st.engine.render(&st.drawList);
    st.ui.render(st.gl, st.drawList, st.width, st.height);

    st.gl.endFrame();
}

void androidMain(android_app* app) {
    AppState st;
    st.app = app;
    app->userData = &st;
    app->onAppCmd = handleCmd;
    app->onInputEvent = handleInput;

    st.bootAt = nowSeconds();
    st.lastTime = st.bootAt;
    LOGI("Craftya aciliyor");

    // NOT: Eskiden motor burada, YÜZEY HAZIR OLMADAN başlatılıyor ve
    // startNewWorld() çağrılıyordu. Bu, menüyü atlayıp doğrudan oyuna
    // girmek demekti: ana menü/mods/ayarlar hiç görünmüyor, oyuncu
    // nerede olduğunu anlamıyordu. Artık motor ilk yüzeyle birlikte
    // başlatılıyor ve normal akış (ana menü) izleniyor.
    st.cfg.seed = 20261124u;
    st.cfg.savePath = "craftya_world.dat";
    st.cfg.settingsPath = "craftya_settings.json";

    while (st.running) {
        int events = 0;
        android_poll_source* source = nullptr;
        int timeout = st.hasSurface ? 0 : 20;
        while (ALooper_pollAll(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) { st.running = false; break; }
            timeout = 0;
        }
        if (!st.running) break;
        if (!st.hasSurface) continue;

        double t = nowSeconds();
        float dt = static_cast<float>(t - st.lastTime);
        st.lastTime = t;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0001f) dt = 1.f / 60.f;
        st.fpsAvg = st.fpsAvg * 0.95f + (1.f / dt) * 0.05f;

        st.engine.update(dt);
        // dokunsal geri bildirim (blok kırma/koyma)
        float vib = st.engine.consumeVibration();
        if (vib > 0.f && st.app && st.app->activity && st.app->activity->vm) {
            int64_t now = static_cast<int64_t>(t * 1000.0);
            if (now - st.lastVibrateAt > 60) {   // spam koruması
                st.lastVibrateAt = now;
                vibrate(st.app, static_cast<int>(vib * 1000.f));
            }
        }
        renderFrame(st);
    }

    st.audio.shutdown();
    g_audioState = nullptr;
    for (auto& kv : st.gpuMeshes) st.gl.freeMesh(kv.second);
    st.gpuMeshes.clear();
    st.engine.shutdown();
    st.gl.shutdown();
    LOGI("Craftya kapandi");
}

} // namespace

void android_main(android_app* app) {
    androidMain(app);
}
