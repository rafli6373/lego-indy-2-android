#include "touch_controls.h"
#include <cmath>
#include <algorithm>

TouchControls::TouchControls()
    : m_screenWidth(0),
      m_screenHeight(0),
      m_stickCenterX(0),
      m_stickCenterY(0),
      m_stickThumbX(0),
      m_stickThumbY(0),
      m_stickRadius(0),
      m_stickTouchId(-1),
      m_analogX(128),
      m_analogY(128),
      m_currentButtons(0) {
}

TouchControls::~TouchControls() = default;

void TouchControls::initLayout(int screenWidth, int screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_buttons.clear();

    const float btnRadius = screenHeight * 0.075f;
    const float margin = screenHeight * 0.12f;

    // --- Action Buttons (Right Hand: Cross, Circle, Triangle, Square) ---
    const float rightCenterX = screenWidth - margin - btnRadius * 1.5f;
    const float rightCenterY = screenHeight - margin - btnRadius * 1.5f;
    const float faceOffset = btnRadius * 1.8f;

    m_buttons.push_back({PSP_CTRL_TRIANGLE, rightCenterX, rightCenterY - faceOffset, btnRadius, "TRI", false});
    m_buttons.push_back({PSP_CTRL_CROSS,    rightCenterX, rightCenterY + faceOffset, btnRadius, "CRS", false});
    m_buttons.push_back({PSP_CTRL_SQUARE,   rightCenterX - faceOffset, rightCenterY, btnRadius, "SQR", false});
    m_buttons.push_back({PSP_CTRL_CIRCLE,   rightCenterX + faceOffset, rightCenterY, btnRadius, "CIR", false});

    // --- D-Pad Buttons (Left Hand) ---
    const float dpadCenterX = margin + btnRadius * 1.5f;
    const float dpadCenterY = screenHeight * 0.42f;
    const float dpadOffset = btnRadius * 1.6f;

    m_buttons.push_back({PSP_CTRL_UP,    dpadCenterX, dpadCenterY - dpadOffset, btnRadius, "UP", false});
    m_buttons.push_back({PSP_CTRL_DOWN,  dpadCenterX, dpadCenterY + dpadOffset, btnRadius, "DN", false});
    m_buttons.push_back({PSP_CTRL_LEFT,  dpadCenterX - dpadOffset, dpadCenterY, btnRadius, "LT", false});
    m_buttons.push_back({PSP_CTRL_RIGHT, dpadCenterX + dpadOffset, dpadCenterY, btnRadius, "RT", false});

    // --- Shoulder Triggers (Top Left / Right) ---
    m_buttons.push_back({PSP_CTRL_LTRIGGER, margin, margin * 0.7f, btnRadius * 1.2f, " L ", false});
    m_buttons.push_back({PSP_CTRL_RTRIGGER, screenWidth - margin, margin * 0.7f, btnRadius * 1.2f, " R ", false});

    // --- System / Menu Buttons (Bottom Center) ---
    const float sysY = screenHeight - margin * 0.45f;
    m_buttons.push_back({PSP_CTRL_SELECT, screenWidth * 0.40f, sysY, btnRadius * 0.8f, "SEL", false});
    m_buttons.push_back({PSP_CTRL_START,  screenWidth * 0.60f, sysY, btnRadius * 0.8f, "STA", false});

    // --- Virtual Analog Stick (Bottom Left) ---
    m_stickCenterX = margin + btnRadius * 1.5f;
    m_stickCenterY = screenHeight - margin - btnRadius * 1.5f;
    m_stickThumbX = m_stickCenterX;
    m_stickThumbY = m_stickCenterY;
    m_stickRadius = btnRadius * 2.0f;
    m_stickTouchId = -1;
    m_analogX = 128;
    m_analogY = 128;
}

static inline float distSq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

void TouchControls::handleTouchDown(int pointerId, float x, float y) {
    m_activeTouches[pointerId] = {pointerId, x, y, true};

    // Check if touch is within analog stick base
    if (m_stickTouchId == -1) {
        float rSq = m_stickRadius * m_stickRadius;
        if (distSq(x, y, m_stickCenterX, m_stickCenterY) <= rSq * 1.5f) {
            m_stickTouchId = pointerId;
            handleTouchMove(pointerId, x, y);
        }
    }

    updateButtons();
}

void TouchControls::handleTouchMove(int pointerId, float x, float y) {
    auto it = m_activeTouches.find(pointerId);
    if (it != m_activeTouches.end()) {
        it->second.x = x;
        it->second.y = y;
    }

    // Update Analog Stick position if this pointer owns it
    if (pointerId == m_stickTouchId) {
        float dx = x - m_stickCenterX;
        float dy = y - m_stickCenterY;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > m_stickRadius) {
            dx = (dx / dist) * m_stickRadius;
            dy = (dy / dist) * m_stickRadius;
        }

        m_stickThumbX = m_stickCenterX + dx;
        m_stickThumbY = m_stickCenterY + dy;

        // Normalize to PSP range 0 - 255 (Center 128)
        float normX = dx / m_stickRadius;
        float normY = dy / m_stickRadius;

        m_analogX = static_cast<uint8_t>(std::clamp(128.0f + normX * 127.0f, 0.0f, 255.0f));
        m_analogY = static_cast<uint8_t>(std::clamp(128.0f + normY * 127.0f, 0.0f, 255.0f));
    }

    updateButtons();
}

void TouchControls::handleTouchUp(int pointerId) {
    m_activeTouches.erase(pointerId);

    if (pointerId == m_stickTouchId) {
        m_stickTouchId = -1;
        m_stickThumbX = m_stickCenterX;
        m_stickThumbY = m_stickCenterY;
        m_analogX = 128;
        m_analogY = 128;
    }

    updateButtons();
}

void TouchControls::updateButtons() {
    m_currentButtons = 0;

    for (auto& btn : m_buttons) {
        btn.pressed = false;
        float rSq = btn.radius * btn.radius;

        for (const auto& [id, touch] : m_activeTouches) {
            // Ignore analog stick touch pointer for button presses
            if (id == m_stickTouchId) continue;

            if (distSq(touch.x, touch.y, btn.x, btn.y) <= rSq) {
                btn.pressed = true;
                m_currentButtons |= btn.pspMask;
                break;
            }
        }
    }
}

void TouchControls::getCtrlData(SceCtrlData* outData) {
    if (!outData) return;
    outData->timeStamp = 0;
    outData->buttons = m_currentButtons;
    outData->lx = m_analogX;
    outData->ly = m_analogY;
}
