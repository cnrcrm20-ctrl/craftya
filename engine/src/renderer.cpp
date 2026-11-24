// Craftya Engine — renderer shader kaynakları (GLES 3.0 ES / masaüstü GL)
#include "craftya/renderer.hpp"

namespace craftya {

// Chunk vertex shader: konum, normal, UV, ışık
const char* ShaderSources::chunkVS() {
    return R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in float aLight;

uniform mat4 uViewProj;
uniform vec3 uChunkOffset;

out vec3 vNormal;
out vec2 vUV;
out float vLight;
out vec3 vWorldPos;

void main() {
    vec3 world = aPos + uChunkOffset;
    vNormal = aNormal;
    vUV = aUV;
    vLight = aLight;
    vWorldPos = world;
    gl_Position = uViewProj * vec4(world, 1.0);
}
)GLSL";
}

const char* ShaderSources::chunkFS() {
    return R"GLSL(#version 300 es
precision mediump float;

in vec3 vNormal;
in vec2 vUV;
in float vLight;
in vec3 vWorldPos;

uniform sampler2D uAtlas;
uniform float uSunFactor;      // 0..1 (gece/gündüz)
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec3 uCameraPos;

out vec4 fragColor;

void main() {
    vec4 tex = texture(uAtlas, vUV);
    if (tex.a < 0.05) discard;

    // yön bazlı gölgeleme (vertex aLight) x güneş
    float sun = mix(0.12, 1.0, uSunFactor);
    vec3 lit = tex.rgb * vLight * sun;

    // mesafe sisi
    float dist = length(vWorldPos - uCameraPos);
    float fog = clamp((dist - uFogStart) / max(0.001, (uFogEnd - uFogStart)), 0.0, 1.0);
    vec3 color = mix(lit, uFogColor, fog);

    fragColor = vec4(color, tex.a);
}
)GLSL";
}

const char* ShaderSources::skyVS() {
    return R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.999, 1.0);
}
)GLSL";
}

const char* ShaderSources::skyFS() {
    return R"GLSL(#version 300 es
precision mediump float;
in vec2 vUV;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
out vec4 fragColor;
void main() {
    vec3 c = mix(uBottomColor, uTopColor, vUV.y);
    fragColor = vec4(c, 1.0);
}
)GLSL";
}

const char* ShaderSources::guiVS() {
    return R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform vec2 uScreenSize;
out vec2 vUV;
void main() {
    vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;
    vUV = aUV;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";
}

const char* ShaderSources::guiFS() {
    return R"GLSL(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec4 uColor;
uniform bool uUseTex;
out vec4 fragColor;
void main() {
    vec4 c = uColor;
    if (uUseTex) c *= texture(uTex, vUV);
    fragColor = c;
}
)GLSL";
}

} // namespace craftya
