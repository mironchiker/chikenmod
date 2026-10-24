#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>

// Dobby из reveny репо
#include "dobby.h"

#include "imgui.h"
#include "backends/imgui_impl_android.h"
#include "backends/imgui_impl_opengl3.h"
#include "chams.h"
#include "menu.h"

#define LOG_TAG "ChickenMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// eglSwapBuffers hook — рендерим ImGui каждый кадр
// ---------------------------------------------------------------------------
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;
static bool g_imguiInited = false;

static EGLBoolean new_eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (!g_imguiInited) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowRounding = 8.f;
        ImGui::GetStyle().FrameRounding  = 4.f;
        ImGui::GetStyle().GrabRounding   = 4.f;

        ImGui_ImplAndroid_Init(nullptr);
        ImGui_ImplOpenGL3_Init("#version 100");

        g_imguiInited = true;
        LOGI("ImGui initialized OK");
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    DrawChamsMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return old_eglSwapBuffers(display, surface);
}

// ---------------------------------------------------------------------------
// JNI_OnLoad
// ---------------------------------------------------------------------------
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("ChickenGun mod loading...");

    if (!mlovinit()) {
        LOGE("Failed to open libGLESv2.so");
        return JNI_VERSION_1_6;
    }

    LogShaders();
    Wallhack();

    void* libEGL = dlopen("libEGL.so", RTLD_LAZY);
    if (libEGL) {
        void* swapSym = dlsym(libEGL, "eglSwapBuffers");
        if (swapSym) {
            DobbyHook(swapSym,
                reinterpret_cast<void*>(new_eglSwapBuffers),
                reinterpret_cast<void**>(&old_eglSwapBuffers));
            LOGI("eglSwapBuffers hooked");
        }
    }

    LOGI("All hooks installed gl gl brody");
    return JNI_VERSION_1_6;
}
