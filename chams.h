#ifndef ZEFF_CHAMS
#define ZEFF_CHAMS

#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <fstream>
#include <chrono>
#include <cmath>
#include <mutex>
#include <algorithm>

#include "imgui.h"

// ---------------------------------------------------------------------------
// Конфиг
// ---------------------------------------------------------------------------
static void*       g_handle       = nullptr;
static std::string g_shaderName   = "";

static std::atomic<int>   g_chamsMode{0};
static std::atomic<bool>  g_enableRainbow{false};
static std::atomic<bool>  g_dumpShaders{false};
static std::atomic<float> g_lineWidth{10.0f};

static std::mutex  g_colorMtx;
static float g_r = 1.f, g_g = 1.f, g_b = 1.f;
static ImVec4 g_wallColor    = {1.f, 0.f, 0.f, 1.f}; // RED default
static ImVec4 g_visibleColor = {0.f, 1.f, 0.f, 1.f}; // GREEN default

static std::mutex            g_wlMtx;
static std::vector<GLsizei>  g_whitelistCounts;
static int g_tolerance = 6;

static std::mutex                    g_dumpMtx;
static std::unordered_set<std::string> g_seenUniforms;

static const char* kChamsModes[] = {
    "OFF",
    "Default",
    "Shading",
    "Wireframe",
    "Outline",
    "Visibility Check Outline",
    "Visibility Check Shading",
    "Visibility Check Shading + Outline"
};
static const int kChamsModeCount = 8;

// ---------------------------------------------------------------------------
// Публичный API
// ---------------------------------------------------------------------------
inline void setShader(const char* s)     { if (s) g_shaderName = s; }
inline const char* getShader()           { return g_shaderName.c_str(); }

inline void SetR(float v) { std::lock_guard<std::mutex> l(g_colorMtx); g_r = std::clamp(v,0.f,1.f); }
inline void SetG(float v) { std::lock_guard<std::mutex> l(g_colorMtx); g_g = std::clamp(v,0.f,1.f); }
inline void SetB(float v) { std::lock_guard<std::mutex> l(g_colorMtx); g_b = std::clamp(v,0.f,1.f); }
inline void SetWallColor(ImVec4 c)    { std::lock_guard<std::mutex> l(g_colorMtx); g_wallColor    = c; }
inline void SetVisibleColor(ImVec4 c) { std::lock_guard<std::mutex> l(g_colorMtx); g_visibleColor = c; }

inline void SetChamsMode(int m)  { g_chamsMode  = std::clamp(m,0,7); }
inline int  GetChamsMode()       { return g_chamsMode.load(); }
inline void SetRainbow(bool on)  { g_enableRainbow = on; }
inline void SetDumpShaders(bool on) { g_dumpShaders = on; }

inline void SetWhitelistCounts(const std::vector<GLsizei>& v) {
    std::lock_guard<std::mutex> l(g_wlMtx);
    g_whitelistCounts = v;
}

// ---------------------------------------------------------------------------
// Rainbow HSV
// ---------------------------------------------------------------------------
static void updateRainbow() {
    if (!g_enableRainbow) return;
    static auto start = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - start).count();
    float h = std::fmod(t * 60.f, 360.f);
    float c = 1.f;
    float x = c * (1.f - std::fabs(std::fmod(h/60.f, 2.f) - 1.f));
    float rr=0,gg=0,bb=0;
    if      (h< 60){rr=c;gg=x;}
    else if (h<120){rr=x;gg=c;}
    else if (h<180){gg=c;bb=x;}
    else if (h<240){gg=x;bb=c;}
    else if (h<300){rr=x;bb=c;}
    else           {rr=c;bb=x;}
    std::lock_guard<std::mutex> l(g_colorMtx);
    g_r=rr; g_g=gg; g_b=bb;
}

// ---------------------------------------------------------------------------
// GL State save/restore
// ---------------------------------------------------------------------------
struct GLStateSnapshot {
    GLboolean blend=GL_FALSE, depthTest=GL_FALSE;
    GLint blendSrcRGB=GL_ONE, blendDstRGB=GL_ZERO;
    GLint blendSrcAlpha=GL_ONE, blendDstAlpha=GL_ZERO;
    GLfloat depthRange[2]={0.f,1.f};
    GLfloat lineWidth=1.f;
    GLfloat blendColor[4]={0.f,0.f,0.f,0.f};
};

static void saveState(GLStateSnapshot& s) {
    glGetBooleanv(GL_BLEND,           &s.blend);
    glGetBooleanv(GL_DEPTH_TEST,      &s.depthTest);
    glGetIntegerv(GL_BLEND_SRC_RGB,   &s.blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB,   &s.blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &s.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &s.blendDstAlpha);
    glGetFloatv(GL_DEPTH_RANGE,       s.depthRange);
    glGetFloatv(GL_LINE_WIDTH,        &s.lineWidth);
    glGetFloatv(GL_BLEND_COLOR,       s.blendColor);
}

static void restoreState(const GLStateSnapshot& s) {
    glBlendFuncSeparate(s.blendSrcRGB, s.blendDstRGB,
                        s.blendSrcAlpha, s.blendDstAlpha);
    glBlendColor(s.blendColor[0],s.blendColor[1],
                 s.blendColor[2],s.blendColor[3]);
    glLineWidth(s.lineWidth);
    glDepthRangef(s.depthRange[0], s.depthRange[1]);
    s.blend     ? glEnable(GL_BLEND)      : glDisable(GL_BLEND);
    s.depthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------------------
// Конвертер triangle-индексов → GL_LINES (ФИКС wireframe)
// ---------------------------------------------------------------------------
static std::vector<GLushort> g_lineIndexBuf;

static void buildLineIndices(GLsizei count, GLenum type, const void* indices) {
    g_lineIndexBuf.clear();
    g_lineIndexBuf.reserve(count * 2);
    if (type == GL_UNSIGNED_SHORT) {
        const GLushort* idx = static_cast<const GLushort*>(indices);
        for (GLsizei i = 0; i+2 < count; i+=3) {
            GLushort a=idx[i],b=idx[i+1],c=idx[i+2];
            g_lineIndexBuf.push_back(a); g_lineIndexBuf.push_back(b);
            g_lineIndexBuf.push_back(b); g_lineIndexBuf.push_back(c);
            g_lineIndexBuf.push_back(c); g_lineIndexBuf.push_back(a);
        }
    } else if (type == GL_UNSIGNED_BYTE) {
        const GLubyte* idx = static_cast<const GLubyte*>(indices);
        for (GLsizei i = 0; i+2 < count; i+=3) {
            GLushort a=idx[i],b=idx[i+1],c=idx[i+2];
            g_lineIndexBuf.push_back(a); g_lineIndexBuf.push_back(b);
            g_lineIndexBuf.push_back(b); g_lineIndexBuf.push_back(c);
            g_lineIndexBuf.push_back(c); g_lineIndexBuf.push_back(a);
        }
    }
}

// forward decl
static void (*old_glDrawElements)(GLenum,GLsizei,GLenum,const void*) = nullptr;
static GLint (*old_glGetUniformLocation)(GLuint, const GLchar*) = nullptr;

static inline void drawWireframe(GLsizei count, GLenum type,
                                  const void* indices, float lw,
                                  float r, float g, float b) {
    buildLineIndices(count, type, indices);
    if (g_lineIndexBuf.empty()) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_CONSTANT_COLOR, GL_CONSTANT_ALPHA);
    glBlendColor(r, g, b, 1.f);
    glLineWidth(lw);
    old_glDrawElements(GL_LINES,
                       (GLsizei)g_lineIndexBuf.size(),
                       GL_UNSIGNED_SHORT,
                       g_lineIndexBuf.data());
}

// ---------------------------------------------------------------------------
// Хук glGetUniformLocation
// ---------------------------------------------------------------------------
static GLint new_glGetUniformLocation(GLuint program, const GLchar* name) {
    if (g_dumpShaders.load() && name) {
        std::lock_guard<std::mutex> lk(g_dumpMtx);
        if (g_seenUniforms.insert(name).second) {
            std::ofstream f("/sdcard/Download/Shaders.txt", std::ios::app);
            if (f.is_open()) f << name << '\n';
        }
    }
    return old_glGetUniformLocation(program, name);
}

// ---------------------------------------------------------------------------
// Хук glDrawElements
// ---------------------------------------------------------------------------
static inline bool isWhitelisted(GLsizei count) {
    std::lock_guard<std::mutex> l(g_wlMtx);
    if (g_whitelistCounts.empty()) return true;
    for (auto c : g_whitelistCounts)
        if (std::abs((int)count-(int)c) <= g_tolerance) return true;
    return false;
}

static void new_glDrawElements(GLenum mode, GLsizei count,
                                GLenum type, const void* indices) {
    int cm = g_chamsMode.load();
    if (cm == 0 || !isWhitelisted(count))
        return old_glDrawElements(mode, count, type, indices);

    GLint currProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currProgram);
    if (currProgram == 0)
        return old_glDrawElements(mode, count, type, indices);

    if (g_shaderName.empty())
        return old_glDrawElements(mode, count, type, indices);

    GLint uid = old_glGetUniformLocation(currProgram, g_shaderName.c_str());
    if (uid == -1)
        return old_glDrawElements(mode, count, type, indices);

    GLboolean blend=GL_FALSE, depthTest=GL_FALSE;
    glGetBooleanv(GL_BLEND,      &blend);
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    if (blend || !depthTest)
        return old_glDrawElements(mode, count, type, indices);

    GLStateSnapshot snap;
    saveState(snap);
    updateRainbow();

    float r,g,b;
    ImVec4 wallC, visC;
    {
        std::lock_guard<std::mutex> l(g_colorMtx);
        r=g_r; g=g_g; b=g_b;
        wallC=g_wallColor; visC=g_visibleColor;
    }
    float lw = g_lineWidth.load();

    switch (cm) {
        case 1:
            glDepthRangef(1.f, 0.5f);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            break;
        case 2:
            glEnable(GL_BLEND);
            glDepthRangef(1.f, 0.5f);
            glBlendFunc(GL_ONE, GL_CONSTANT_COLOR);
            glBlendColor(r,g,b,1.f);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            break;
        case 3:
            old_glDrawElements(mode, count, type, indices);
            glDepthRangef(1.f, 0.5f);
            drawWireframe(count, type, indices, lw, r,g,b);
            break;
        case 4:
            old_glDrawElements(mode, count, type, indices);
            glDepthRangef(1.f, 0.f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_CONSTANT_COLOR, GL_CONSTANT_ALPHA);
            glBlendColor(0,0,0,1);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            glDepthRangef(1.f, 0.5f);
            drawWireframe(count, type, indices, lw, r,g,b);
            break;
        case 5:
            glEnable(GL_BLEND);
            glBlendFunc(GL_CONSTANT_COLOR, GL_CONSTANT_ALPHA);
            glBlendColor(r,g,b,1.f);
            glDepthRangef(0.5f,1.f);
            drawWireframe(count, type, indices, lw, r,g,b);
            glBlendFunc(GL_ONE_MINUS_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_ALPHA);
            glBlendColor(0,0,0,1);
            glDepthRangef(1.f, 0.5f);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            break;
        case 6:
            glEnable(GL_BLEND);
            glDisable(GL_DEPTH_TEST);
            glBlendFunc(GL_ONE, GL_CONSTANT_COLOR);
            glBlendColor(wallC.x,wallC.y,wallC.z,1);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            glEnable(GL_DEPTH_TEST);
            glDepthRangef(0.5f,1.f);
            glBlendColor(visC.x,visC.y,visC.z,1);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            break;
        case 7:
            glEnable(GL_BLEND);
            glDisable(GL_DEPTH_TEST);
            glBlendFunc(GL_ONE, GL_CONSTANT_COLOR);
            glBlendColor(wallC.x,wallC.y,wallC.z,1);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            glEnable(GL_DEPTH_TEST);
            glBlendFunc(GL_CONSTANT_COLOR, GL_CONSTANT_ALPHA);
            glDepthRangef(0.5f,1.f);
            drawWireframe(count, type, indices, lw, visC.x,visC.y,visC.z);
            glBlendFunc(GL_ONE_MINUS_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_ALPHA);
            glDepthRangef(0.49f,1.f);
            glBlendColor(0,0,0,1);
            old_glDrawElements(GL_TRIANGLES, count, type, indices);
            break;
    }
    restoreState(snap);
}

// ---------------------------------------------------------------------------
// Инициализация хуков
// ---------------------------------------------------------------------------
inline bool mlovinit() {
    g_handle = dlopen("libGLESv2.so", RTLD_LAZY);
    return g_handle != nullptr;
}

inline void LogShaders() {
    if (!g_handle) return;
    void* p = dlsym(g_handle, "glGetUniformLocation");
    if (!p) return;
    DobbyHook(p,
        reinterpret_cast<void*>(new_glGetUniformLocation),
        reinterpret_cast<void**>(&old_glGetUniformLocation));
}

inline void Wallhack() {
    if (!g_handle) return;
    void* p = dlsym(g_handle, "glDrawElements");
    if (!p) return;
    DobbyHook(p,
        reinterpret_cast<void*>(new_glDrawElements),
        reinterpret_cast<void**>(&old_glDrawElements));
}

#endif // ZEFF_CHAMS
