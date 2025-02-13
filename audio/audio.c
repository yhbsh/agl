#include <GLES/egl.h>

#include <aaudio/AAudio.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <pthread.h>

#include <math.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#include <unistd.h>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))

typedef struct {
    ANativeWindow *window;
    AInputQueue *input;

    bool running;
    pthread_t thread;
    pthread_t audio_thread;
} AndroidApp;

void *render_task(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;

    void *egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl_display, NULL, NULL);
    EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
    EGLint numConfigs;
    void *egl_config;
    eglChooseConfig(egl_display, attribs, &egl_config, 1, &numConfigs);
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    void *egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, contextAttribs);
    void *egl_surface = eglCreateWindowSurface(egl_display, egl_config, app->window, NULL);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    GLint width, height;
    eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &width);
    eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &height);
    glViewport(0, 0, width, height);

    float dt = 0.0f;

    while (app->running) {
        AInputEvent *event = NULL;
        while (AInputQueue_getEvent(app->input, &event) >= 0) {
            if (AInputQueue_preDispatchEvent(app->input, event)) continue;
            AInputQueue_finishEvent(app->input, event, 0);
        }

        float r = (sinf(dt + 0) * 0.5f) + 0.5f;
        float g = (cosf(dt + 0) * 0.5f) + 0.5f;
        float b = (sinf(dt + 3) * 0.5f) + 0.5f;

        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        eglSwapBuffers(egl_display, egl_surface);

        dt += 0.01f;
    }

    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_display, egl_surface);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);

    return NULL;
}

void *audio_task(void *arg) {
    ANativeActivity *activity = (ANativeActivity *)arg;
    AndroidApp *app = activity->instance;
    AAsset *asset = AAssetManager_open(activity->assetManager, "file.wav", AASSET_MODE_UNKNOWN);
    if (!asset) {
        LOG("cannot load file.wav");
        exit(0);
    }

    int64_t size = AAsset_getLength(asset);
    uint8_t *buffer = (uint8_t *)malloc(size);

    if (AAsset_read(asset, buffer, size) < 0) {
        LOG("cannot read file.wav");
        exit(0);
    }

    int ret;

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 44100);

    if ((ret = ma_decoder_init_memory(buffer, size, &config, &decoder)) != MA_SUCCESS) {
        LOG("cannot initialize decoder %s", ma_result_description(ret));
        free(buffer);
        AAsset_close(asset);
        exit(0);
    }

    AAudioStream *stream = NULL;
    AAudioStreamBuilder *builder;
    if ((ret = AAudio_createStreamBuilder(&builder)) != AAUDIO_OK) {
        LOG("cannot create stream builder: %s", AAudio_convertResultToText(ret));
        exit(0);
    }

    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, 44100);

    if ((ret = AAudioStreamBuilder_openStream(builder, &stream)) != AAUDIO_OK) {
        LOG("cannot open stream: %s", AAudio_convertResultToText(ret));
        AAudioStreamBuilder_delete(builder);
        exit(0);
    }

    if ((ret = AAudioStream_requestStart(stream)) != AAUDIO_OK) {
        LOG("cannot start stream: %s", AAudio_convertResultToText(ret));
        AAudioStream_close(stream);
        AAudioStreamBuilder_delete(builder);
        exit(0);
    }

    float audio_buffer[1024 * 2];
    ma_uint64 frames_read;

    while (app->running) {
        ret = ma_decoder_read_pcm_frames(&decoder, audio_buffer, 1024, &frames_read);
        if (ret == MA_AT_END) {
            if ((ret = ma_decoder_seek_to_pcm_frame(&decoder, 0)) != MA_SUCCESS) {
                LOG("cannot reset decoder: %s", ma_result_description(ret));
                break;
            }
            continue;
        }

        if (ret != MA_SUCCESS) {
            LOG("cannot read PCM frames: %s", ma_result_description(ret));
            break;
        }

        if ((ret = AAudioStream_write(stream, audio_buffer, frames_read, INT64_MAX)) < 0) {
            LOG("cannot write stream: %s", AAudio_convertResultToText(ret));
            break;
        }
    }

    AAudioStream_requestStop(stream);
    AAudioStream_close(stream);
    AAudioStreamBuilder_delete(builder);
    ma_decoder_uninit(&decoder);
    free(buffer);
    AAsset_close(asset);
    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    LOG("on_window_init");

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->window = window;
    app->running = true;

    pthread_create(&app->thread, NULL, render_task, app);
    pthread_create(&app->audio_thread, NULL, audio_task, activity);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    LOG("on_window_deinit");

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
