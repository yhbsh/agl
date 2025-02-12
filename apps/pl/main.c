#include <jni.h>

#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <pthread.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))

typedef struct {
    ANativeWindow *window;
    AInputQueue *input;

    bool running;
    pthread_t thread;
} AndroidApp;

void *main_thread(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;
    void *egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl_display, NULL, NULL);

    void *egl_config = NULL;
    EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    eglChooseConfig(egl_display, attribs, &egl_config, 1, NULL);
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    void *egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, contextAttribs);
    void *egl_surface = eglCreateWindowSurface(egl_display, egl_config, app->window, NULL);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    while (app->running) {
        glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(egl_display, egl_surface);
    }

    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_display, egl_surface);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);

    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    LOGI("on_window_init");

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->window = window;
    app->running = true;

    pthread_create(&app->thread, NULL, main_thread, (AndroidApp *)activity->instance);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    LOGI("on_window_deinit");

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->running = false;
    pthread_join(app->thread, NULL);
    app->window = NULL;
}

void on_input_init(ANativeActivity *activity, AInputQueue *input) {
    AndroidApp *app = (AndroidApp *)activity->instance;
    app->input = input;
    AInputQueue_attachLooper(input, ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS), ALOOPER_EVENT_INPUT, NULL, NULL);
}

void on_input_deinit(ANativeActivity *activity, AInputQueue *input) {
    AndroidApp *app = (AndroidApp *)activity->instance;
    AInputQueue_detachLooper(input);
    app->input = NULL;
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    AndroidApp *app = (AndroidApp *)malloc(sizeof(AndroidApp));
    memset(app, 0, sizeof(AndroidApp));

    activity->callbacks->onNativeWindowCreated = on_window_init;
    activity->callbacks->onNativeWindowDestroyed = on_window_deinit;
    activity->callbacks->onInputQueueCreated = on_input_init;
    activity->callbacks->onInputQueueDestroyed = on_input_deinit;
    activity->instance = app;
}
