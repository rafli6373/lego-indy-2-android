#ifndef PSP_RENDERER_GL_H
#define PSP_RENDERER_GL_H

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include "touch_controls.h"
#include <string>

class PspRendererGL {
public:
    PspRendererGL();
    ~PspRendererGL();

    bool init(int surfaceWidth, int surfaceHeight);
    void updateDimensions(int surfaceWidth, int surfaceHeight);
    void beginFrame();
    void renderTouchHUD(const TouchControls& controls);
    void renderStatusOverlay(const std::string& message);
    void endFrame();
    void destroy();

    int getViewportX() const { return m_viewportX; }
    int getViewportY() const { return m_viewportY; }
    int getViewportWidth() const { return m_viewportWidth; }
    int getViewportHeight() const { return m_viewportHeight; }

private:
    GLuint compileShader(GLenum type, const char* source);
    GLuint createProgram(const char* vertexSource, const char* fragmentSource);

    void drawCircle(float cx, float cy, float radius, float r, float g, float b, float a);
    void drawFilledRect(float x, float y, float width, float height, float r, float g, float b, float a);

    int m_surfaceWidth;
    int m_surfaceHeight;

    int m_viewportX;
    int m_viewportY;
    int m_viewportWidth;
    int m_viewportHeight;

    GLuint m_shaderProgram;
    GLint m_uColorLocation;
    GLint m_uOrthoMatrixLocation;

    GLuint m_vao;
    GLuint m_vbo;

    float m_orthoMatrix[16];
};

#endif // PSP_RENDERER_GL_H
