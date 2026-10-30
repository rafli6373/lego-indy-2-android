#ifndef PSP_GE_H
#define PSP_GE_H

#include "psp_types.h"
#include <GLES3/gl3.h>
#include <vector>
#include <cstdint>

struct GeMatrix4x4 {
    float m[16];
};

class PspGE {
public:
    static PspGE& instance();

    bool init();
    void shutdown();

    void setFrameBuffer(uint32_t address, int width, int format);
    void executeDisplayList(const uint32_t* list, size_t count);
    void waitVblank();

    // Matrix states
    void setWorldMatrix(const float* mat);
    void setViewMatrix(const float* mat);
    void setProjectionMatrix(const float* mat);

    // Viewport
    void setViewport(float x, float y, float w, float h);

private:
    PspGE();
    ~PspGE();

    uint32_t m_fbAddress;
    int m_fbWidth;
    int m_fbFormat;

    GeMatrix4x4 m_worldMatrix;
    GeMatrix4x4 m_viewMatrix;
    GeMatrix4x4 m_projMatrix;

    float m_vpX, m_vpY, m_vpW, m_vpH;
    bool m_initialized;
};

#endif // PSP_GE_H
