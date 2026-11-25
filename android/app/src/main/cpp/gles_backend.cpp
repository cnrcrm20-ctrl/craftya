// Craftya — GLES 3.0 / EGL gerçek GPU arka ucu
#include "gles_backend.hpp"

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/log.h>
#include <cstring>
#include <cmath>
#include <cstdio>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "CraftyaGL", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "CraftyaGL", __VA_ARGS__)

namespace craftya {

namespace {

unsigned int compile(GLenum type, const char* src, std::string& err) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetShaderInfoLog(s, sizeof(log), &n, log);
        err += log;
        LOGE("shader derleme hatasi: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

unsigned int link(const char* vs, const char* fs, std::string& err) {
    unsigned int v = compile(GL_VERTEX_SHADER, vs, err);
    if (!v) return 0;
    unsigned int f = compile(GL_FRAGMENT_SHADER, fs, err);
    if (!f) { glDeleteShader(v); return 0; }
    unsigned int p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetProgramInfoLog(p, sizeof(log), &n, log);
        err += log;
        LOGE("shader link hatasi: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// 6 köşeli birim kare (0,0)-(1,1)
const float kQuad[8] = {0, 0, 1, 0, 0, 1, 1, 1};

} // namespace

GLESBackend::GLESBackend() = default;

GLESBackend::~GLESBackend() { GLESBackend::shutdown(); }

bool GLESBackend::initSurface(void* nativeWindow, int width, int height) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) { error_ = "EGL display yok"; return false; }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(display, &major, &minor)) { error_ = "eglInitialize basarisiz"; return false; }
    LOGI("EGL %d.%d", major, minor);

    const EGLint attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint num = 0;
    if (!eglChooseConfig(display, attrs, &cfg, 1, &num) || num == 0) {
        error_ = "uygun EGLConfig yok (GLES3)";
        return false;
    }

    EGLSurface surface = eglCreateWindowSurface(display, cfg,
                                                static_cast<EGLNativeWindowType>(nativeWindow), nullptr);
    if (surface == EGL_NO_SURFACE) { error_ = "EGLSurface olusturulamadi"; return false; }

    const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext ctx = eglCreateContext(display, cfg, EGL_NO_CONTEXT, ctxAttrs);
    if (ctx == EGL_NO_CONTEXT) { error_ = "EGLContext olusturulamadi"; return false; }

    if (!eglMakeCurrent(display, surface, surface, ctx)) {
        error_ = "eglMakeCurrent basarisiz";
        return false;
    }

    // dikey eşitleme — pil/isinma icin 60 FPS'e kilit
    eglSwapInterval(display, 1);

    display_ = display;
    surface_ = surface;
    context_ = ctx;

    width_ = width > 0 ? width : 1280;
    height_ = height > 0 ? height : 720;
    ready_ = true;
    LOGI("GLES: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    return compileProgram();
}

void GLESBackend::destroySurface() {
    EGLDisplay d = static_cast<EGLDisplay>(display_);
    if (d != EGL_NO_DISPLAY && d) {
        eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_) eglDestroyContext(d, static_cast<EGLContext>(context_));
        if (surface_) eglDestroySurface(d, static_cast<EGLSurface>(surface_));
        eglTerminate(d);
    }
    display_ = surface_ = context_ = nullptr;
    ready_ = false;
}

bool GLESBackend::init(int w, int h) {
    width_ = w; height_ = h;
    if (!ready_) return false;
    return compileProgram();
}

void GLESBackend::shutdown() {
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    if (uiProgram_) { glDeleteProgram(uiProgram_); uiProgram_ = 0; }
    if (atlasTex_) { glDeleteTextures(1, &atlasTex_); atlasTex_ = 0; }
    if (fontTex_) { glDeleteTextures(1, &fontTex_); fontTex_ = 0; }
    if (quadVbo_) { glDeleteBuffers(1, &quadVbo_); quadVbo_ = 0; }
    if (uiQuadVbo_) { glDeleteBuffers(1, &uiQuadVbo_); uiQuadVbo_ = 0; }
    if (uiTextVbo_) { glDeleteBuffers(1, &uiTextVbo_); uiTextVbo_ = 0; }
    destroySurface();
}

bool GLESBackend::compileProgram() {
    program_ = link(ShaderSources::chunkVS(), ShaderSources::chunkFS(), error_);
    if (!program_) return false;

    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    createUiPipeline();
    return true;
}

void GLESBackend::createUiPipeline() {
    uiProgram_ = link(ShaderSources::guiVS(), ShaderSources::guiFS(), error_);
    if (!uiProgram_) return;
    // 6 vertex * (x,y,u,v) float = 6*4
    uiQuadCapacity_ = 6 * 4;
    glGenBuffers(1, &uiQuadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, uiQuadVbo_);
    glBufferData(GL_ARRAY_BUFFER, uiQuadCapacity_ * sizeof(float) * 256, nullptr, GL_DYNAMIC_DRAW);
    // toplu yazı tamponu: köşe başına x,y,u,v,r,g,b,a = 8 float
    glGenBuffers(1, &uiTextVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, uiTextVbo_);
    glBufferData(GL_ARRAY_BUFFER, 6 * 8 * sizeof(float) * 2048, nullptr, GL_DYNAMIC_DRAW);
}

// ---------------- TOPLU YAZI ----------------
void GLESBackend::beginUiText() {
    // ÖNEMLİ: glVertexAttribPointer bir kez kurulduğunda VAO durumunda kalıyor.
    // Toplu yazı 8 float'lık, diğer çizimler 4 float'lık düzen kullanıyor;
    // yazıdan sonra düzen geri kurulmazsa TÜM arayüz bozuk çizilir.
    // Bu yüzden flush sonunda düzeni burada yeniden kuruyoruz.
    textVerts_.clear();
}

void GLESBackend::addUiGlyph(float x, float y, float w, float h,
                             float u0, float v0, float u1, float v1,
                             float r, float g, float b, float a) {
    // 2 üçgen = 6 köşe; renk köşe başına (her glif farklı renkte olabilir)
    const float px[4] = {x, x + w, x, x + w};
    const float py[4] = {y, y, y + h, y + h};
    const float pu[4] = {u0, u1, u0, u1};
    const float pv[4] = {v0, v0, v1, v1};
    static const int order[6] = {0, 1, 2, 1, 3, 2};
    for (int k = 0; k < 6; ++k) {
        int i = order[k];
        textVerts_.push_back(px[i]);
        textVerts_.push_back(py[i]);
        textVerts_.push_back(pu[i]);
        textVerts_.push_back(pv[i]);
        textVerts_.push_back(r);
        textVerts_.push_back(g);
        textVerts_.push_back(b);
        textVerts_.push_back(a);
    }
}

void GLESBackend::flushUiText() {
    if (textVerts_.empty() || uiProgram_ == 0 || fontTex_ == 0) {
        textVerts_.clear(); return;
    }
    glUniform1i(glGetUniformLocation(uiProgram_, "uVertColor"), 1);
    glUniform1i(glGetUniformLocation(uiProgram_, "uUseGrad"), 0);
    glUniform1f(glGetUniformLocation(uiProgram_, "uCorner"), 0.f);
    glUniform1f(glGetUniformLocation(uiProgram_, "uRingInner"), 0.f);
    glUniform1i(glGetUniformLocation(uiProgram_, "uUseTex"), 1);
    glUniform1i(glGetUniformLocation(uiProgram_, "uTex"), 0);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glBindBuffer(GL_ARRAY_BUFFER, uiTextVbo_);
    glBufferData(GL_ARRAY_BUFFER, textVerts_.size() * sizeof(float),
                 textVerts_.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(4 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textVerts_.size() / 8));
    stats_.drawCalls++;
    textVerts_.clear();
    glDisableVertexAttribArray(2);
    glUniform1i(glGetUniformLocation(uiProgram_, "uVertColor"), 0);

    // ---- VAO durumunu normale döndür ----
    // 4 float'lık düzeni (x,y,u,v) yeniden kur. Bu satırlar olmadan yazıdan
    // sonraki paneller/kartlar yanlış öznitelik düzeniyle çizilip ekranı
    // bozuyordu.
    glBindBuffer(GL_ARRAY_BUFFER, uiQuadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
}

void GLESBackend::resize(int w, int h) {
    width_ = w; height_ = h;
    glViewport(0, 0, w, h);
}

void GLESBackend::beginFrame(const float skyColor[3], float fogStart, float fogEnd) {
    sky_[0] = skyColor[0]; sky_[1] = skyColor[1]; sky_[2] = skyColor[2];
    fogStart_ = fogStart; fogEnd_ = fogEnd;
    stats_ = {};
    glViewport(0, 0, width_, height_);
    glClearColor(sky_[0], sky_[1], sky_[2], 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLESBackend::endFrame() {
    EGLDisplay d = static_cast<EGLDisplay>(display_);
    if (d && surface_) eglSwapBuffers(d, static_cast<EGLSurface>(surface_));
}

void GLESBackend::uploadAtlas(const Image& atlas) {
    if (atlasTex_ == 0) glGenTextures(1, &atlasTex_);
    glBindTexture(GL_TEXTURE_2D, atlasTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas.width, atlas.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas.pixels.data());
    // piksel sanatı: yakinlastirma komşu piksel, kenar kırpma (doku sızması yok)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // mipmap: 16px sanatı bozar, bilinçli olarak kapalı
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    stats_.textureBytes = atlas.pixels.size();
}

void GLESBackend::uploadMesh(GpuMesh& mesh, const MeshData& data, bool transparent) {
    // opak + saydam ayrı VBO'lara gider; basitlik için tek tamponda tutuluyor
    std::vector<Vertex> verts;
    verts.reserve(data.opaque.size() + data.transparent.size());
    verts.insert(verts.end(), data.opaque.begin(), data.opaque.end());
    verts.insert(verts.end(), data.transparent.begin(), data.transparent.end());

    if (verts.empty()) { mesh.uploaded = false; mesh.indexCount = 0; return; }

    if (mesh.vbo == 0) glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)),
                 verts.data(), GL_STATIC_DRAW);

    // ---- KRİTİK: mesh verisi DÖRTGEN (4 köşe) olarak geliyor ----
    // Önceden indeks tamponu 0,1,2,3,4,... şeklinde SIRALI dolduruluyordu.
    // Bu durumda GPU'nun çizdiği üçgenler (0,1,2) ve (3,4,5) dörtgen
    // sınırlarını aşıyor; dünya ekranda çapraz şeritler/dilimler halinde
    // dağılıyordu. Dörtgen başına iki üçgen (0,1,2)+(0,2,3) üretilmeli.
    if (verts.size() % 4 != 0) {
        // beklenmeyen veri: güvenli tarafta kal, dörtgen varsayımıyla kırp
        verts.resize(verts.size() - (verts.size() % 4));
        if (verts.empty()) { mesh.uploaded = false; mesh.indexCount = 0; return; }
    }
    if (mesh.ibo == 0) glGenBuffers(1, &mesh.ibo);
    // tek doğru kaynak: quadIndices (renderer.hpp) — testle korunuyor
    const std::vector<uint32_t> idx = quadIndices(verts.size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned int)),
                 idx.data(), GL_STATIC_DRAW);

    mesh.indexCount = static_cast<uint32_t>(idx.size());
    mesh.quadCount = data.quadCount;
    mesh.transparent = transparent;
    mesh.uploaded = true;

    stats_.vertices += static_cast<int>(verts.size());
    stats_.bufferBytes += verts.size() * sizeof(Vertex);
}

void GLESBackend::freeMesh(GpuMesh& mesh) {
    if (mesh.vbo) { glDeleteBuffers(1, &mesh.vbo); mesh.vbo = 0; }
    if (mesh.ibo) { glDeleteBuffers(1, &mesh.ibo); mesh.ibo = 0; }
    mesh.uploaded = false;
    mesh.indexCount = 0;
}

void GLESBackend::setCamera(const Camera& cam, float aspect) {
    Mat4 vp = cam.proj(aspect) * cam.view();
    std::memcpy(viewProj_, vp.m, sizeof(viewProj_));
    camPos_[0] = cam.pos.x; camPos_[1] = cam.pos.y; camPos_[2] = cam.pos.z;
}

void GLESBackend::setSun(float f, const float[3]) { sunFactor_ = f; }
void GLESBackend::setShadowEnabled(bool on) { shadow_ = on; }

void GLESBackend::drawMesh(const GpuMesh& mesh, const Vec3f& offset) {
    if (!mesh.uploaded || mesh.indexCount == 0 || program_ == 0) return;

    glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uViewProj"), 1, GL_FALSE, viewProj_);
    glUniform3f(glGetUniformLocation(program_, "uChunkOffset"), offset.x, offset.y, offset.z);
    glUniform1f(glGetUniformLocation(program_, "uSunFactor"), sunFactor_);
    glUniform3f(glGetUniformLocation(program_, "uFogColor"), sky_[0], sky_[1], sky_[2]);
    glUniform1f(glGetUniformLocation(program_, "uFogStart"), fogStart_);
    glUniform1f(glGetUniformLocation(program_, "uFogEnd"), fogEnd_);
    glUniform3f(glGetUniformLocation(program_, "uCameraPos"), camPos_[0], camPos_[1], camPos_[2]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTex_);
    glUniform1i(glGetUniformLocation(program_, "uAtlas"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(24));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(32));

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount),
                   GL_UNSIGNED_INT, nullptr);

    stats_.drawCalls++;
    stats_.triangles += static_cast<int>(mesh.indexCount / 3);
}

// ---------------- 2B arayüz ----------------
void GLESBackend::beginUi(int w, int h) {
    if (uiProgram_ == 0) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, w, h);
    glUseProgram(uiProgram_);
    glUniform2f(glGetUniformLocation(uiProgram_, "uScreenSize"),
                static_cast<float>(w), static_cast<float>(h));
    glUniform1i(glGetUniformLocation(uiProgram_, "uTex"), 0);
    glUniform1i(glGetUniformLocation(uiProgram_, "uUseTex"), 0);
    glUniform1i(glGetUniformLocation(uiProgram_, "uVertColor"), 0);
    // 2. öznitelik yalnız toplu yazıda kullanılıyor; kapalıyken varsayılan
    // değer (0,0,0,1) okunur ve renk uniform'dan gelir.
    glDisableVertexAttribArray(2);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, uiQuadVbo_);
}

void GLESBackend::drawUiQuadStyled(float x, float y, float w, float h,
                                   float r, float g, float b, float a,
                                   float corner,
                                   const float* gradTop, const float* gradBottom,
                                   int atlasTile, float ringInner) {
    if (uiProgram_ == 0) return;
    float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
    if (atlasTile >= 0) Atlas::tileUV(atlasTile, u0, v0, u1, v1);

    const float v[] = {
        x,     y,     u0, v0,
        x + w, y,     u1, v0,
        x,     y + h, u0, v1,
        x + w, y,     u1, v0,
        x + w, y + h, u1, v1,
        x,     y + h, u0, v1
    };
    glUniform4f(glGetUniformLocation(uiProgram_, "uColor"), r, g, b, a);
    glUniform1i(glGetUniformLocation(uiProgram_, "uUseGrad"), gradTop ? 1 : 0);
    if (gradTop) {
        glUniform4f(glGetUniformLocation(uiProgram_, "uColor2"),
                    gradBottom[0], gradBottom[1], gradBottom[2], gradBottom[3]);
    }
    glUniform2f(glGetUniformLocation(uiProgram_, "uRectCenter"),
                x + w * 0.5f, y + h * 0.5f);
    glUniform2f(glGetUniformLocation(uiProgram_, "uHalfSize"), w * 0.5f, h * 0.5f);
    glUniform1f(glGetUniformLocation(uiProgram_, "uCorner"), corner);
    glUniform1f(glGetUniformLocation(uiProgram_, "uRingInner"), ringInner);
    if (atlasTile >= 0) {
        glBindTexture(GL_TEXTURE_2D, atlasTex_);
        glUniform1i(glGetUniformLocation(uiProgram_, "uUseTex"), 1);
    } else {
        glUniform1i(glGetUniformLocation(uiProgram_, "uUseTex"), 0);
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    stats_.drawCalls++;
}

void GLESBackend::drawUiQuad(float x, float y, float w, float h,
                             float r, float g, float b, float a,
                             int atlasTile) {
    // düz dörtgen: stil verisi olmadan (köşe 0, gradyan yok)
    drawUiQuadStyled(x, y, w, h, r, g, b, a, 0.f, nullptr, nullptr, atlasTile);
}

// ---- modern yazı: alfa dokulu glif atlası ----
void GLESBackend::uploadFontAtlas(const uint8_t* alpha, int w, int h) {
    if (!alpha || w <= 0 || h <= 0) return;
    if (fontTex_ == 0) glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    // tek kanal alfa → GL_ALPHA yerine RGBA'ya genişlet (uyumluluk)
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha[i];
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    // DOĞRUSAL filtre: bu, "blok piksel" görünümünü bitiren şey.
    // (atlas dokusu NEAREST kalıyor — blok sanatı için doğru.)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLESBackend::drawUiGlyph(float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              float r, float g, float b, float a) {
    if (uiProgram_ == 0 || fontTex_ == 0) return;
    const float v[] = {
        x,     y,     u0, v0,
        x + w, y,     u1, v0,
        x,     y + h, u0, v1,
        x + w, y,     u1, v0,
        x + w, y + h, u1, v1,
        x,     y + h, u0, v1
    };
    glUniform4f(glGetUniformLocation(uiProgram_, "uColor"), r, g, b, a);
    glUniform1i(glGetUniformLocation(uiProgram_, "uUseGrad"), 0);
    glUniform1f(glGetUniformLocation(uiProgram_, "uCorner"), 0.f);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glUniform1i(glGetUniformLocation(uiProgram_, "uUseTex"), 1);
    glUniform1i(glGetUniformLocation(uiProgram_, "uTex"), 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    stats_.drawCalls++;
}

void GLESBackend::endUi() {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

} // namespace craftya
