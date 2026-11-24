// Craftya Engine — kamera, frustum culling, chunk akışı, oyun döngüsü
#pragma once
#include "craftya/core.hpp"
#include "craftya/world.hpp"
#include "craftya/mesher.hpp"
#include "craftya/physics.hpp"
#include <vector>
#include <unordered_set>

namespace craftya {

// ---------- matris (sütun-öncelikli, OpenGL/WGPU uyumlu) ----------
struct Mat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    static Mat4 identity() { return Mat4{}; }

    static Mat4 perspective(float fovYRad, float aspect, float nearZ, float farZ) {
        Mat4 r;
        float f = 1.0f / std::tan(fovYRad * 0.5f);
        for (int i = 0; i < 16; ++i) r.m[i] = 0.f;
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = farZ / (nearZ - farZ);
        r.m[11] = -1.0f;
        r.m[14] = (nearZ * farZ) / (nearZ - farZ);
        return r;
    }

    static Mat4 lookAt(const Vec3f& eye, const Vec3f& center, const Vec3f& up) {
        Vec3f f = (center - eye).normalized();
        Vec3f s = f.cross(up).normalized();
        Vec3f u = s.cross(f);
        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -s.dot(eye);
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -u.dot(eye);
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] =  f.dot(eye);
        r.m[3] = 0;    r.m[7] = 0;    r.m[11] = 0;    r.m[15] = 1;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float sum = 0;
                for (int k = 0; k < 4; ++k) sum += m[k * 4 + row] * o.m[c * 4 + k];
                r.m[c * 4 + row] = sum;
            }
        return r;
    }
};

// ---------- frustum ----------
struct Plane { float a, b, c, d; };

struct Frustum {
    Plane planes[6];

    static Frustum fromCameraMatrix(const Mat4& m) {
        Frustum f;
        auto set = [&](int i, float a, float b, float c, float d) {
            float len = std::sqrt(a*a + b*b + c*c);
            if (len < 1e-8f) len = 1.f;
            f.planes[i] = {a/len, b/len, c/len, d/len};
        };
        set(0, m.m[3]+m.m[0], m.m[7]+m.m[4], m.m[11]+m.m[8],  m.m[15]+m.m[12]); // sol
        set(1, m.m[3]-m.m[0], m.m[7]-m.m[4], m.m[11]-m.m[8],  m.m[15]-m.m[12]); // sağ
        set(2, m.m[3]+m.m[1], m.m[7]+m.m[5], m.m[11]+m.m[9],  m.m[15]+m.m[13]); // alt
        set(3, m.m[3]-m.m[1], m.m[7]-m.m[5], m.m[11]-m.m[9],  m.m[15]-m.m[13]); // üst
        set(4, m.m[3]+m.m[2], m.m[7]+m.m[6], m.m[11]+m.m[10], m.m[15]+m.m[14]); // yakın
        set(5, m.m[3]-m.m[2], m.m[7]-m.m[6], m.m[11]-m.m[10], m.m[15]-m.m[14]); // uzak
        return f;
    }

    // AABB frustum içinde mi (kısmi test)
    bool boxVisible(const Vec3f& mn, const Vec3f& mx) const {
        for (int i = 0; i < 6; ++i) {
            const Plane& p = planes[i];
            float px = p.a > 0 ? mx.x : mn.x;
            float py = p.b > 0 ? mx.y : mn.y;
            float pz = p.c > 0 ? mx.z : mn.z;
            if (p.a * px + p.b * py + p.c * pz + p.d < 0) return false;
        }
        return true;
    }
};

// ---------- kamera ----------
struct Camera {
    Vec3f pos{0, 45, 0};
    float yaw = 0.f;     // radyan, sağa +
    float pitch = 0.f;   // radyan, yukarı +
    float fovDeg = 70.f;
    float nearZ = 0.1f;
    float farZ = 1000.f;

    Vec3f forward() const {
        float cp = std::cos(pitch), sp = std::sin(pitch);
        return {std::sin(yaw) * cp, sp, -std::cos(yaw) * cp};
    }
    Vec3f right() const { return {std::cos(yaw), 0.f, std::sin(yaw)}; }
    Vec3f up() const { return right().cross(forward()); }

    Mat4 view() const { return Mat4::lookAt(pos, pos + forward(), {0, 1, 0}); }
    Mat4 proj(float aspect) const {
        return Mat4::perspective(fovDeg * 3.14159265f / 180.f, aspect, nearZ, farZ);
    }
};

// ---------- chunk akışı (görüş 6 / işlem 6.5) ----------
class ChunkStreamer {
public:
    // kameranın etrafındaki gerekli chunk listesini günceller (kamera yönü öncelikli)
    std::vector<ChunkPos> update(const Camera& cam, int viewChunks = VIEW_CHUNKS);

    // yüklü mesh'leri serbest bırakılacaklar listesine ekler
    std::vector<ChunkPos> unloadList(const Camera& cam, int viewChunks = VIEW_CHUNKS) const;

    static float chunkRadiusWorld() { return VIEW_CHUNKS * CHUNK_W; }
};

// ---------- performans sayacı ----------
struct FrameStats {
    float fps = 0.f;
    float frameMs = 0.f;
    float meshMs = 0.f;
    int visibleChunks = 0;
    int loadedChunks = 0;
    int drawCalls = 0;
    size_t vramBytes = 0;
};

} // namespace craftya
