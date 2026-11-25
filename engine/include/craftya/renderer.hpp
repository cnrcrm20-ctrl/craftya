// Craftya Engine — renderer arayüzü + GLES 3.0 arka ucu
// Platformdan bağımsız çizim komutları; GLES katmanı Android/masaüstü için ortak.
#pragma once
#include "craftya/core.hpp"
#include "craftya/mesher.hpp"
#include "craftya/atlas.hpp"
#include "craftya/render.hpp"
#include <vector>
#include <cstdint>

namespace craftya {

// GPU tarafında tutulan chunk mesh'i
struct GpuMesh {
    uint32_t vbo = 0;         // vertex buffer
    uint32_t ibo = 0;         // index buffer
    uint32_t indexCount = 0;
    uint32_t quadCount = 0;
    bool uploaded = false;
    bool transparent = false;
};

// ---- DÖRTGEN → İNDEKS ----
// Mesh verisi dörtgen (4 köşe) olarak saklanır. GPU'ya SIRALI indeks
// (0,1,2,3,4,...) yüklenirse üçgenler dörtgen sınırlarını aşar ve dünya
// ekranda çapraz şeritler halinde dağılır (gerçek sahada görülen hata).
// Bu yardımcı tek doğru kaynağı sağlar: her dörtgen için (0,1,2)+(0,2,3).
inline std::vector<uint32_t> quadIndices(size_t vertexCount) {
    const size_t quads = vertexCount / 4;
    std::vector<uint32_t> idx(quads * 6);
    for (size_t q = 0; q < quads; ++q) {
        const uint32_t b = static_cast<uint32_t>(q * 4);
        idx[q * 6 + 0] = b + 0;
        idx[q * 6 + 1] = b + 1;
        idx[q * 6 + 2] = b + 2;
        idx[q * 6 + 3] = b + 0;
        idx[q * 6 + 4] = b + 2;
        idx[q * 6 + 5] = b + 3;
    }
    return idx;
}

// Render istatistikleri
struct RenderStats {
    int drawCalls = 0;
    int triangles = 0;
    int vertices = 0;
    size_t textureBytes = 0;
    size_t bufferBytes = 0;
    float gpuMs = 0.f;
};

// Çizim hattı arayüzü (GLES, Vulkan veya null backend uygular)
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool init(int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual void resize(int width, int height) = 0;

    virtual void beginFrame(const float skyColor[3], float fogStart, float fogEnd) = 0;
    virtual void endFrame() = 0;

    // doku atlası yükle
    virtual void uploadAtlas(const Image& atlas) = 0;

    // mesh yükleme / güncelleme
    virtual void uploadMesh(GpuMesh& mesh, const MeshData& data, bool transparent) = 0;
    virtual void freeMesh(GpuMesh& mesh) = 0;

    // chunk mesh çiz (dünya koordinatı offseti ile)
    virtual void drawMesh(const GpuMesh& mesh, const Vec3f& offset) = 0;

    // kamera / ışık
    virtual void setCamera(const Camera& cam, float aspect) = 0;
    virtual void setSun(float sunFactor, const float sunDir[3]) = 0;
    virtual void setShadowEnabled(bool on) = 0;

    virtual const RenderStats& stats() const = 0;
};

// --- Null backend: testlerde ve sunucuda çizim yapmadan çalıştırmak için ---
class NullRenderBackend : public IRenderBackend {
public:
    bool init(int w, int h) override { width_ = w; height_ = h; return true; }
    void shutdown() override {}
    void resize(int w, int h) override { width_ = w; height_ = h; }
    void beginFrame(const float*, float, float) override { stats_ = {}; }
    void endFrame() override {}
    void uploadAtlas(const Image& img) override { stats_.textureBytes = img.pixels.size(); }
    void uploadMesh(GpuMesh& m, const MeshData& d, bool tr) override {
        m.uploaded = true; m.transparent = tr;
        // GPU yoluyla AYNI kural: dörtgen başına 6 indeks
        m.indexCount = static_cast<uint32_t>(quadIndices(
            d.opaque.size() + d.transparent.size()).size());
        m.quadCount = d.quadCount;
        stats_.bufferBytes += d.bytes();
    }
    void freeMesh(GpuMesh& m) override { m.uploaded = false; m.indexCount = 0; }
    void drawMesh(const GpuMesh& m, const Vec3f&) override {
        if (!m.uploaded) return;
        stats_.drawCalls++;
        stats_.vertices += static_cast<int>(m.indexCount);
        stats_.triangles += static_cast<int>(m.indexCount / 2);
    }
    void setCamera(const Camera&, float) override {}
    void setSun(float, const float[3]) override {}
    void setShadowEnabled(bool on) override { shadow_ = on; }
    const RenderStats& stats() const override { return stats_; }
private:
    int width_ = 0, height_ = 0;
    bool shadow_ = true;
    RenderStats stats_;
};

// GLES 3.0 shader kaynakları (gerçek GPU arka ucu bunları kullanır)
struct ShaderSources {
    static const char* chunkVS();
    static const char* chunkFS();
    static const char* skyVS();
    static const char* skyFS();
    static const char* guiVS();
    static const char* guiFS();
};

} // namespace craftya
