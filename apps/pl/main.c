#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jni.h>

#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <libavformat/avformat.h>
#include <pthread.h>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))

typedef struct {
    ANativeWindow *window;
    AInputQueue *input;

    bool running;
    pthread_t thread;
} Context;

void *ffmpeg_thread(void *arg) {
    Context *ctx = (Context *)arg;

    const char *url = "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";
    AVFormatContext *format = NULL;
    int ret = avformat_open_input(&format, url, NULL, NULL);
    if (ret < 0) {
        LOG("avformat_open_input %s\n", av_err2str(ret));
        return NULL;
    }

    AVPacket *pkt = av_packet_alloc();
    while (av_read_frame(format, pkt) >= 0 && ctx->running) {
        // should read as fast as possible
        // buffer frames, not packets, and then main loop should read from circular buffer and render
        // LOG("pkt->size %d\n", pkt->size);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    avformat_close_input(&format);

    return NULL;
}

void *main_thread(void *arg) {
    Context *ctx = (Context *)arg;

    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) {
        LOG("ERROR eglGetDisplay");
        return NULL;
    }

    if (!eglInitialize(egl_display, NULL, NULL)) {
        LOG("ERROR eglInitialize");
        return NULL;
    }

    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE
    };

    EGLConfig egl_config;
    EGLint num_configs;
    if (!eglChooseConfig(egl_display, attributes, &egl_config, 1, &num_configs) || num_configs == 0) {
        LOG("ERROR eglChooseConfig");
        eglTerminate(egl_display);
        return NULL;
    }

    EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attributes);
    if (egl_context == EGL_NO_CONTEXT) {
        LOG("ERROR eglCreateContext");
        eglTerminate(egl_display);
        return NULL;
    }

    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, egl_config, ctx->window, NULL);
    if (egl_surface == EGL_NO_SURFACE) {
        LOG("ERROR eglCreateWindowSurface");
        eglDestroyContext(egl_display, egl_context);
        eglTerminate(egl_display);
        return NULL;
    }

    if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
        LOG("ERROR eglMakeCurrent");
        eglDestroySurface(egl_display, egl_surface);
        eglDestroyContext(egl_display, egl_context);
        eglTerminate(egl_display);
        return NULL;
    }

    pthread_t ffmpeg_tid;
    pthread_create(&ffmpeg_tid, NULL, ffmpeg_thread, ctx);

    while (ctx->running) {
        AInputEvent *event = NULL;
        while (AInputQueue_getEvent(ctx->input, &event) >= 0) {
            if (AInputQueue_preDispatchEvent(ctx->input, event)) continue;
            AInputQueue_finishEvent(ctx->input, event, 0);
        }

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
    LOG("on_window_init");

    Context *ctx = (Context *)activity->instance;
    ctx->window = window;
    ctx->running = true;

    pthread_create(&ctx->thread, NULL, main_thread, (Context *)activity->instance);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    LOG("on_window_deinit");

    Context *ctx = (Context *)activity->instance;
    ctx->running = false;
    pthread_join(ctx->thread, NULL);
    ctx->window = NULL;
}

void on_input_init(ANativeActivity *activity, AInputQueue *input) {
    Context *ctx = (Context *)activity->instance;
    ctx->input = input;
    AInputQueue_attachLooper(input, ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS), ALOOPER_EVENT_INPUT, NULL, NULL);
}

void on_input_deinit(ANativeActivity *activity, AInputQueue *input) {
    Context *ctx = (Context *)activity->instance;
    AInputQueue_detachLooper(input);
    ctx->input = NULL;
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    Context *ctx = (Context *)malloc(sizeof(Context));
    memset(ctx, 0, sizeof(Context));

    activity->callbacks->onNativeWindowCreated = on_window_init;
    activity->callbacks->onNativeWindowDestroyed = on_window_deinit;
    activity->callbacks->onInputQueueCreated = on_input_init;
    activity->callbacks->onInputQueueDestroyed = on_input_deinit;
    activity->instance = ctx;
}
