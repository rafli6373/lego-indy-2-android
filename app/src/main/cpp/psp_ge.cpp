#include "psp_ge.h"
#include <android/log.h>
#include <cstring>
#include <thread>
#include <chrono>

#define LOG_TAG "PspGE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

PspGE& PspGE::instance() {
    static PspGE s_instance;
    return s_instance;
}

PspGE::PspGE()
    : m_fbAddress(0),
      m_fbWidth(512),
      m_fbFormat(PSP_DISPLAY_PIXEL_FORMAT_8888),
      m_vpX(0), m_vpY(0), m_vpW(480), m_vpH(272),
      m_initialized(false) {
    std::memset(&m_worldMatrix, 0, sizeof(m_worldMatrix));
    std::memset(&m_viewMatrix, 0, sizeof(m_viewMatrix));
    std::memset(&m_projMatrix, 0, sizeof(m_projMatrix));

    m_worldMatrix.m[0] = m_worldMatrix.m[5] = m_worldMatrix.m[10] = m_worldMatrix.m[15] = 1.0f;
    m_viewMatrix.m[0]  = m_viewMatrix.m[5]  = m_viewMatrix.m[10]  = m_viewMatrix.m[15]  = 1.0f;
    m_projMatrix.m[0]  = m_projMatrix.m[5]  = m_projMatrix.m[10]  = m_projMatrix.m[15]  = 1.0f;
}

PspGE::~PspGE() {
    shutdown();
}

bool PspGE::init() {
    m_initialized = true;
    LOGI("PSP Graphics Engine (GE) Core Initialized successfully.");
    return true;
}

void PspGE::shutdown() {
    m_initialized = false;
}

void PspGE::setFrameBuffer(uint32_t address, int width, int format) {
    m_fbAddress = address;
    m_fbWidth = width;
    m_fbFormat = format;
}

void PspGE::setWorldMatrix(const float* mat) {
    if (mat) std::memcpy(m_worldMatrix.m, mat, sizeof(m_worldMatrix.m));
}

void PspGE::setViewMatrix(const float* mat) {
    if (mat) std::memcpy(m_viewMatrix.m, mat, sizeof(m_viewMatrix.m));
}

void PspGE::setProjectionMatrix(const float* mat) {
    if (mat) std::memcpy(m_projMatrix.m, mat, sizeof(m_projMatrix.m));
}

void PspGE::setViewport(float x, float y, float w, float h) {
    m_vpX = x;
    m_vpY = y;
    m_vpW = w;
    m_vpH = h;
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

void PspGE::executeDisplayList(const uint32_t* list, size_t count) {
    if (!list || count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        uint32_t cmd = list[i];
        uint8_t op = static_cast<uint8_t>((cmd >> 24) & 0xFF);
        uint32_t arg = cmd & 0x00FFFFFF;

        switch (op) {
            case 0x00: // NOP
                break;
            case 0x04: // PRIM (Draw primitive)
                break;
            case 0x24: // Alpha blend enable/disable
                if (arg & 1) glEnable(GL_BLEND);
                else glDisable(GL_BLEND);
                break;
            case 0x22: // Depth test enable/disable
                if (arg & 1) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
                break;
            default:
                break;
        }
    }
}

void PspGE::waitVblank() {
    // Standard 60 FPS pacing (~16.66 ms)
    std::this_thread::sleep_for(std::chrono::microseconds(16666));
}
