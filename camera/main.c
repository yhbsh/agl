#include <jni.h>

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <media/NdkImageReader.h>

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCaptureRequest.h>

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "ENGINE", __VA_ARGS__))

typedef struct {
    bool running;
    pthread_t thread;

    ACameraManager *camera_manager;
    ACameraDevice *camera_device;
    ACaptureRequest *capture_request;
    ACameraCaptureSession *camera_capture_session;

    ACaptureSessionOutputContainer *output_container;
    ACameraOutputTarget *preview_output_target;
    ACaptureSessionOutput *preview_session_output;
    ACameraOutputTarget *process_output_target;
    ACaptureSessionOutput *process_session_output;

    ANativeWindow *preview_window;
    ANativeWindow *process_window;
    AInputQueue *input;
} Context;

void onImageAvailable(void *context, AImageReader *reader) {
    int ret;

    AImage *image = NULL;
    ret = AImageReader_acquireNextImage(reader, &image);
    assert(ret == AMEDIA_OK && image);

    int32_t width, height, format;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);
    AImage_getFormat(image, &format);

    LOGI("Captured image size: width=%d, height=%d, format=%d", width, height, format);

    AImage_delete(image);
}

void *main_thread(void *arg) {
    int ret;

    Context *ctx = (Context *)arg;

    ctx->camera_manager = ACameraManager_create();
    assert(ctx->camera_manager && "cannot create camera manager");

    // 1. list all available cameras
    ACameraIdList *cameraIdList = NULL;
    ret = ACameraManager_getCameraIdList(ctx->camera_manager, &cameraIdList);
    assert(ret == ACAMERA_OK && cameraIdList && "cannot get cameras ids list");

    // 2. open the first available camera
    ACameraDevice_StateCallbacks camera_state_callbacks = {0};
    ret = ACameraManager_openCamera(ctx->camera_manager, cameraIdList->cameraIds[0], &camera_state_callbacks, &ctx->camera_device);
    assert(ret == ACAMERA_OK && ctx->camera_device && "Failed to open camera device");

    // 3. create output container
    ret = ACaptureSessionOutputContainer_create(&ctx->output_container);
    assert(ret == ACAMERA_OK && ctx->output_container && "cannot create output container");

    // 4. create preview session output and add to container
    {
        ret = ACaptureSessionOutput_create(ctx->preview_window, &ctx->preview_session_output);
        assert(ret == ACAMERA_OK && ctx->preview_session_output && "cannot create preview session output");

        ret = ACaptureSessionOutputContainer_add(ctx->output_container, ctx->preview_session_output);
        assert(ret == ACAMERA_OK && "cannot add preview session output to the output container");
    }

    // 5. create process session output and add to container
    {
        ACameraMetadata *metadata = NULL;
        ret = ACameraManager_getCameraCharacteristics(ctx->camera_manager, cameraIdList->cameraIds[0], &metadata);
        assert(ret == ACAMERA_OK && metadata && "cannot get camera characteristics");

        ACameraMetadata_const_entry entry = {0};
        ret = ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
        assert(ret == ACAMERA_OK && "cannot get entry");

        int32_t w = 0;
        int32_t h = 0;
        int32_t f = AIMAGE_FORMAT_YUV_420_888;

        for (uint32_t i = 0; i < entry.count; i += 4) {
            int32_t in_f = entry.data.i32[i + 0]; // format
            int32_t in_w = entry.data.i32[i + 1]; // width
            int32_t in_h = entry.data.i32[i + 2]; // height
            int32_t is_i = entry.data.i32[i + 3]; // is input

            if (is_i == 0 && in_f == f) {
                if (in_w * in_h > w * h) {
                    w = in_w;
                    h = in_h;
                }
            }
        }
        assert(w > 0 && h > 0 && "No supported sizes found for the desired format");
        ACameraMetadata_free(metadata);

        AImageReader *reader = NULL;
        ret = AImageReader_new(w, h, f, 5, &reader);
        assert(ret == AMEDIA_OK && reader && "cannot create an image reader");

        AImageReader_ImageListener ln = {.context = NULL, .onImageAvailable = onImageAvailable};
        ret = AImageReader_setImageListener(reader, &ln);
        assert(ret == AMEDIA_OK && "cannot create an image listener");

        ret = AImageReader_getWindow(reader, &ctx->process_window);
        assert(ret == AMEDIA_OK && reader && "cannot get native window from the image reader");

        ret = ACaptureSessionOutput_create(ctx->process_window, &ctx->process_session_output);
        assert(ret == ACAMERA_OK && ctx->process_session_output && "cannot create process session output");

        ret = ACaptureSessionOutputContainer_add(ctx->output_container, ctx->process_session_output);
        assert(ret == ACAMERA_OK && "cannot add process session output to the output container");
    }

    // 6. create capture request
    ret = ACameraDevice_createCaptureRequest(ctx->camera_device, TEMPLATE_PREVIEW, &ctx->capture_request);
    assert(ret == ACAMERA_OK && ctx->capture_request && "cannot create capture request");

    {
        // 7. create output target for preview and add to capture request
        ret = ACameraOutputTarget_create(ctx->preview_window, &ctx->preview_output_target);
        assert(ret == ACAMERA_OK && ctx->preview_output_target && "cannot create preview output target");

        ret = ACaptureRequest_addTarget(ctx->capture_request, ctx->preview_output_target);
        assert(ret == ACAMERA_OK && "cannot add preview target to capture request");
    }

    {
        // 8. Create output target for process and add to capture request
        ret = ACameraOutputTarget_create(ctx->process_window, &ctx->process_output_target);
        assert(ret == ACAMERA_OK && ctx->process_output_target && "cannot create process output target");

        ret = ACaptureRequest_addTarget(ctx->capture_request, ctx->process_output_target);
        assert(ret == ACAMERA_OK && "cannot add process target to capture request");
    }

    // 8. create capture session
    const ACameraCaptureSession_stateCallbacks capture_session_callbacks = {0};
    ret = ACameraDevice_createCaptureSession(ctx->camera_device, ctx->output_container, &capture_session_callbacks, &ctx->camera_capture_session);
    assert(ret == ACAMERA_OK && ctx->camera_capture_session && "cannot create capture session");

    // 9. start capture session
    ret = ACameraCaptureSession_setRepeatingRequest(ctx->camera_capture_session, NULL, 1, &ctx->capture_request, NULL);
    assert(ret == ACAMERA_OK && "cannot start capture session");

    while (ctx->running) {
        // nothing
    }

    if (cameraIdList) ACameraManager_deleteCameraIdList(cameraIdList);
    if (ctx->camera_capture_session) {
        ACameraCaptureSession_stopRepeating(ctx->camera_capture_session);
        ACameraCaptureSession_close(ctx->camera_capture_session);
    }

    if (ctx->capture_request) ACaptureRequest_free(ctx->capture_request);
    if (ctx->output_container) ACaptureSessionOutputContainer_free(ctx->output_container);

    if (ctx->preview_output_target) ACameraOutputTarget_free(ctx->preview_output_target);
    if (ctx->preview_session_output) ACaptureSessionOutput_free(ctx->preview_session_output);

    if (ctx->process_output_target) ACameraOutputTarget_free(ctx->process_output_target);
    if (ctx->process_session_output) ACaptureSessionOutput_free(ctx->process_session_output);

    if (ctx->camera_device) ACameraDevice_close(ctx->camera_device);
    if (ctx->camera_manager) ACameraManager_delete(ctx->camera_manager);

    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    LOGI("on_window_init");

    Context *ctx = (Context *)activity->instance;

    ctx->preview_window = window;
    ctx->running = true;

    pthread_create(&ctx->thread, NULL, main_thread, ctx);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    LOGI("on_window_deinit");

    Context *ctx = (Context *)activity->instance;

    ctx->running = false;

    pthread_join(ctx->thread, NULL);

    ctx->preview_window = NULL;
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
    Context *ctx = malloc(sizeof(Context));
    memset(ctx, 0, sizeof(Context));

    activity->callbacks->onNativeWindowCreated = on_window_init;
    activity->callbacks->onNativeWindowDestroyed = on_window_deinit;
    activity->callbacks->onInputQueueCreated = on_input_init;
    activity->callbacks->onInputQueueDestroyed = on_input_deinit;
    activity->instance = ctx;
}
