// Craftya — GLES 3.0 gerçek GPU arka ucu (Android)
// IRenderBackend arayüzünün EGL/GLES3 uygulaması. Masaüstünde aynı kod
// EGL yerine GLX/EGL ile derlenebilir; GL çağrıları ortaktır.
#pragma once
#include "craftya/renderer.hpp"
#include <string>

namespace craftya {

class GLESBackend : public IRenderBackend {
public:
    GLESBackend();
    ~GLESBackend() override;

    // EGL bağlamı kur (Android: verilen ANativeWindow üstünde)
    bool initSurface(void* nativeWindow, int width, int height);
    void destroySurface();

    bool init(int width, int height) override;
    void shutdown() override;
    void resize(int width, int height) override;

    void beginFrame(const float skyColor[3], float fogStart, float fogEnd) override;
    void endFrame() override;

    void uploadAtlas(const Image& atlas) override;
    void uploadMesh(GpuMesh& mesh, const MeshData& data, bool transparent) override;
    void freeMesh(GpuMesh& mesh) override;
    void drawMesh(const GpuMesh& mesh, const Vec3f& offset) override;

    void setCamera(const Camera& cam, float aspect) override;
    void setSun(float sunFactor, const float sunDir[3]) override;
    void setShadowEnabled(bool on) override;
    const RenderStats& stats() const override { return stats_; }

    // --- 2B arayüz çizimi (DrawList → ekran) ---
    void beginUi(int widthPx, int heightPx);
    void drawUiQuad(float x, float y, float w, float h,
                    float r, float g, float b, float a,
                    int atlasTile = -1);
    void drawUiTextRun(float x, float y, float pxSize, float r, float g, float b,
                       const uint8_t* rows, int glyphW, int glyphH, int count);
    void endUi();

    bool valid() const { return program_ != 0; }
    const std::string& lastError() const { return error_; }

private:
    bool compileProgram();
    void createUiPipeline();

    unsigned int program_ = 0;
    unsigned int uiProgram_ = 0;
    unsigned int atlasTex_ = 0;
    unsigned int fontTex_ = 0;
    unsigned int quadVbo_ = 0;
    unsigned int uiQuadVbo_ = 0;
    int uiQuadCapacity_ = 0;
    int width_ = 0, height_ = 0;
    float viewProj_[16] = {0};
    float sunFactor_ = 1.f;
    float camPos_[3] = {0, 0, 0};
    float fogStart_ = 60.f, fogEnd_ = 96.f;
    float sky_[3] = {0.5f, 0.7f, 1.f};
    bool shadow_ = true;
    bool ready_ = false;
    void* display_ = nullptr;   // EGLDisplay
    void* surface_ = nullptr;   // EGLSurface
    void* context_ = nullptr;   // EGLContext
    RenderStats stats_;
    std::string error_;
};

} // namespace craftya
