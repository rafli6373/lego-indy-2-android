#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "psp_types.h"
#include "touch_controls.h"
#include "psp_renderer_gl.h"
#include "psp_hle.h"

#define LOG_TAG "LegoIndy2Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct AppEngine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;

    bool animating;
    bool hasFocus;

    TouchControls touchControls;
    PspRendererGL renderer;
};

static int initDisplay(AppEngine* engine) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, nullptr);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("Unable to eglMakeCurrent");
        return -1;
    }

    EGLint w, h;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->surface = surface;
    engine->context = context;
    engine->width = w;
    engine->height = h;

    engine->touchControls.initLayout(w, h);
    engine->renderer.init(w, h);

    // Get external storage directory for game data (e.g. /sdcard/Android/data/com.ttgames.legoindy2/files)
    std::string storagePath = "";
    if (engine->app->activity && engine->app->activity->internalDataPath) {
        storagePath = engine->app->activity->internalDataPath;
    }

    PspHLE::instance().init(engine->app->activity->assetManager, storagePath);
    PspHLE::instance().setTouchControls(&engine->touchControls);

    LOGI("Native Engine initialized. Screen resolution: %dx%d", w, h);
    return 0;
}

static void termDisplay(AppEngine* engine) {
    engine->renderer.destroy();
    PspHLE::instance().shutdown();

    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->animating = false;
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

static void drawFrame(AppEngine* engine) {
    if (engine->display == EGL_NO_DISPLAY) return;

    engine->renderer.beginFrame();

    // Query controller state
    SceCtrlData pad;
    PspHLE::instance().sceCtrlReadBufferPositive(&pad, 1);

    // Render virtual on-screen game controller overlay
    engine->renderer.renderTouchHUD(engine->touchControls);

    // Render status overlay
    engine->renderer.renderStatusOverlay(PspHLE::instance().getStatusMessage());

    engine->renderer.endFrame();

    eglSwapBuffers(engine->display, engine->surface);
}

static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    auto* engine = static_cast<AppEngine*>(app->userData);
    int32_t type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        size_t pointerCount = AMotionEvent_getPointerCount(event);
        int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
        int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        switch (actionMasked) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN: {
                int id = AMotionEvent_getPointerId(event, pointerIndex);
                float x = AMotionEvent_getX(event, pointerIndex);
                float y = AMotionEvent_getY(event, pointerIndex);
                engine->touchControls.handleTouchDown(id, x, y);
                return 1;
            }
            case AMOTION_EVENT_ACTION_MOVE: {
                for (size_t i = 0; i < pointerCount; ++i) {
                    int id = AMotionEvent_getPointerId(event, i);
                    float x = AMotionEvent_getX(event, i);
                    float y = AMotionEvent_getY(event, i);
                    engine->touchControls.handleTouchMove(id, x, y);
                }
                return 1;
            }
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
            case AMOTION_EVENT_ACTION_CANCEL: {
                int id = AMotionEvent_getPointerId(event, pointerIndex);
                engine->touchControls.handleTouchUp(id);
                return 1;
            }
            default:
                break;
        }
    }
    return 0;
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    auto* engine = static_cast<AppEngine*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (engine->app->window != nullptr) {
                initDisplay(engine);
                engine->animating = true;
            }
            break;
        case APP_CMD_TERM_WINDOW:
            termDisplay(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            engine->hasFocus = true;
            engine->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            engine->hasFocus = false;
            engine->animating = false;
            drawFrame(engine);
            break;
        default:
            break;
    }
}

void android_main(struct android_app* state) {
    AppEngine engine{};
    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = handleCmd;
    state->onInputEvent = handleInput;
    engine.app = state;

    LOGI("LEGO Indiana Jones 2 Native Android Wrapper started.");

    while (true) {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident = ALooper_pollOnce(engine.animating ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(state, source);
            }
            if (state->destroyRequested != 0) {
                termDisplay(&engine);
                return;
            }
        }

        if (engine.animating && engine.hasFocus) {
            drawFrame(&engine);
        }
    }
}
