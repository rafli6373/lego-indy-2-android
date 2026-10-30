#ifndef TOUCH_CONTROLS_H
#define TOUCH_CONTROLS_H

#include "psp_types.h"
#include <vector>
#include <unordered_map>

struct TouchPoint {
    int id;
    float x;
    float y;
    bool active;
};

struct VirtualButton {
    uint32_t pspMask;
    float x;       // Center X in pixels
    float y;       // Center Y in pixels
    float radius;  // Hit radius
    const char* label;
    bool pressed;
};

class TouchControls {
public:
    TouchControls();
    ~TouchControls();

    void initLayout(int screenWidth, int screenHeight);
    void handleTouchDown(int pointerId, float x, float y);
    void handleTouchMove(int pointerId, float x, float y);
    void handleTouchUp(int pointerId);

    void getCtrlData(SceCtrlData* outData);
    const std::vector<VirtualButton>& getButtons() const { return m_buttons; }

    // Analog stick state
    float getStickCenterX() const { return m_stickCenterX; }
    float getStickCenterY() const { return m_stickCenterY; }
    float getStickThumbX() const { return m_stickThumbX; }
    float getStickThumbY() const { return m_stickThumbY; }
    float getStickRadius() const { return m_stickRadius; }
    bool isStickActive() const { return m_stickTouchId != -1; }

private:
    void updateButtons();

    int m_screenWidth;
    int m_screenHeight;

    std::unordered_map<int, TouchPoint> m_activeTouches;
    std::vector<VirtualButton> m_buttons;

    // Analog stick virtual controls
    float m_stickCenterX;
    float m_stickCenterY;
    float m_stickThumbX;
    float m_stickThumbY;
    float m_stickRadius;
    int m_stickTouchId;
    uint8_t m_analogX;
    uint8_t m_analogY;

    uint32_t m_currentButtons;
};

#endif // TOUCH_CONTROLS_H
