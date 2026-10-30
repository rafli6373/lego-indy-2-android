#include "psp_renderer_gl.h"
#include <android/log.h>
#include <cmath>
#include <vector>

#define LOG_TAG "PspRendererGL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* VERTEX_SHADER_SRC =
    "#version 300 es\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "uniform mat4 uOrthoMatrix;\n"
    "void main() {\n"
    "    gl_Position = uOrthoMatrix * vec4(aPosition, 0.0, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER_SRC =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = uColor;\n"
    "}\n";

PspRendererGL::PspRendererGL()
    : m_surfaceWidth(0),
      m_surfaceHeight(0),
      m_viewportX(0),
      m_viewportY(0),
      m_viewportWidth(0),
      m_viewportHeight(0),
      m_shaderProgram(0),
      m_uColorLocation(-1),
      m_uOrthoMatrixLocation(-1),
      m_vao(0),
      m_vbo(0) {
}

PspRendererGL::~PspRendererGL() {
    destroy();
}

bool PspRendererGL::init(int surfaceWidth, int surfaceHeight) {
    m_shaderProgram = createProgram(VERTEX_SHADER_SRC, FRAGMENT_SHADER_SRC);
    if (!m_shaderProgram) {
        LOGE("Failed to create GLES3 shader program");
        return false;
    }

    m_uColorLocation = glGetUniformLocation(m_shaderProgram, "uColor");
    m_uOrthoMatrixLocation = glGetUniformLocation(m_shaderProgram, "uOrthoMatrix");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    updateDimensions(surfaceWidth, surfaceHeight);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    LOGI("PspRendererGL initialized successfully. Display: %dx%d", surfaceWidth, surfaceHeight);
    return true;
}

void PspRendererGL::updateDimensions(int surfaceWidth, int surfaceHeight) {
    m_surfaceWidth = surfaceWidth;
    m_surfaceHeight = surfaceHeight;

    // Calculate letterboxed 16:9 viewport
    float targetAspect = 16.0f / 9.0f;
    float currentAspect = static_cast<float>(surfaceWidth) / static_cast<float>(surfaceHeight);

    if (currentAspect >= targetAspect) {
        m_viewportHeight = surfaceHeight;
        m_viewportWidth = static_cast<int>(surfaceHeight * targetAspect);
        m_viewportX = (surfaceWidth - m_viewportWidth) / 2;
        m_viewportY = 0;
    } else {
        m_viewportWidth = surfaceWidth;
        m_viewportHeight = static_cast<int>(surfaceWidth / targetAspect);
        m_viewportX = 0;
        m_viewportY = (surfaceHeight - m_viewportHeight) / 2;
    }

    // Set 2D Orthographic Matrix (Top-Left = 0,0)
    float left = 0.0f;
    float right = static_cast<float>(surfaceWidth);
    float top = 0.0f;
    float bottom = static_cast<float>(surfaceHeight);
    float near = -1.0f;
    float far = 1.0f;

    m_orthoMatrix[0] = 2.0f / (right - left);
    m_orthoMatrix[1] = 0.0f;
    m_orthoMatrix[2] = 0.0f;
    m_orthoMatrix[3] = 0.0f;

    m_orthoMatrix[4] = 0.0f;
    m_orthoMatrix[5] = 2.0f / (top - bottom); // Invert Y for top-left origin
    m_orthoMatrix[6] = 0.0f;
    m_orthoMatrix[7] = 0.0f;

    m_orthoMatrix[8] = 0.0f;
    m_orthoMatrix[9] = 0.0f;
    m_orthoMatrix[10] = -2.0f / (far - near);
    m_orthoMatrix[11] = 0.0f;

    m_orthoMatrix[12] = -(right + left) / (right - left);
    m_orthoMatrix[13] = -(top + bottom) / (top - bottom);
    m_orthoMatrix[14] = -(far + near) / (far - near);
    m_orthoMatrix[15] = 1.0f;
}

void PspRendererGL::beginFrame() {
    // Clear whole screen with dark background
    glViewport(0, 0, m_surfaceWidth, m_surfaceHeight);
    glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render game screen area (Letterbox / 16:9 zone) with subtle blue-gray background
    glViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
    glScissor(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    // Reset viewport for full UI overlay
    glViewport(0, 0, m_surfaceWidth, m_surfaceHeight);
    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uOrthoMatrixLocation, 1, GL_FALSE, m_orthoMatrix);
}

void PspRendererGL::renderTouchHUD(const TouchControls& controls) {
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // 1. Draw Analog Stick Base & Thumb
    float stickBaseAlpha = 0.25f;
    drawCircle(controls.getStickCenterX(), controls.getStickCenterY(), controls.getStickRadius(), 0.7f, 0.7f, 0.8f, stickBaseAlpha);

    float thumbAlpha = controls.isStickActive() ? 0.75f : 0.45f;
    float thumbRadius = controls.getStickRadius() * 0.45f;
    drawCircle(controls.getStickThumbX(), controls.getStickThumbY(), thumbRadius, 0.95f, 0.75f, 0.2f, thumbAlpha);

    // 2. Draw Virtual Buttons
    for (const auto& btn : controls.getButtons()) {
        float r = 0.8f, g = 0.8f, b = 0.85f;
        float a = btn.pressed ? 0.85f : 0.35f;

        // Distinct colors for face buttons
        if (btn.pspMask == PSP_CTRL_TRIANGLE) { r = 0.2f; g = 0.85f; b = 0.3f; }
        else if (btn.pspMask == PSP_CTRL_CIRCLE)   { r = 0.9f; g = 0.2f;  b = 0.2f; }
        else if (btn.pspMask == PSP_CTRL_CROSS)    { r = 0.2f; g = 0.45f; b = 0.95f; }
        else if (btn.pspMask == PSP_CTRL_SQUARE)   { r = 0.9f; g = 0.4f;  b = 0.85f; }

        if (btn.pressed) {
            drawCircle(btn.x, btn.y, btn.radius * 1.15f, 1.0f, 1.0f, 1.0f, 0.5f);
        }
        drawCircle(btn.x, btn.y, btn.radius, r, g, b, a);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void PspRendererGL::renderStatusOverlay(const std::string& message) {
    (void)message;
    // Draw top status bar background
    float barHeight = m_surfaceHeight * 0.06f;
    drawFilledRect(0, 0, static_cast<float>(m_surfaceWidth), barHeight, 0.0f, 0.0f, 0.0f, 0.65f);
}

void PspRendererGL::endFrame() {
    // End of rendering passes
}

void PspRendererGL::drawCircle(float cx, float cy, float radius, float r, float g, float b, float a) {
    constexpr int segments = 24;
    std::vector<float> vertices;
    vertices.reserve((segments + 2) * 2);

    vertices.push_back(cx);
    vertices.push_back(cy);

    for (int i = 0; i <= segments; ++i) {
        float angle = i * (2.0f * 3.1415926535f / segments);
        vertices.push_back(cx + std::cos(angle) * radius);
        vertices.push_back(cy + std::sin(angle) * radius);
    }

    glUniform4f(m_uColorLocation, r, g, b, a);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
}

void PspRendererGL::drawFilledRect(float x, float y, float width, float height, float r, float g, float b, float a) {
    float vertices[] = {
        x,         y,
        x + width, y,
        x,         y + height,
        x + width, y + height,
    };

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform4f(m_uColorLocation, r, g, b, a);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

GLuint PspRendererGL::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog.data());
            LOGE("Shader compilation failed:\n%s", infoLog.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint PspRendererGL::createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vShader) return 0;

    GLuint fShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fShader) {
        glDeleteShader(vShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(program, infoLen, nullptr, infoLog.data());
            LOGE("Program link failed:\n%s", infoLog.data());
        }
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return program;
}

void PspRendererGL::destroy() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}
