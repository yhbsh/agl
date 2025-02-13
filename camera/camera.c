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

typedef struct AndroidApp {
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
} AndroidApp;

void onImageAvailable(void *context, AImageReader *reader) {
    (void)context;
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

void *camera_task(void *arg) {
    int ret;

    AndroidApp *app = (AndroidApp *)arg;

    app->camera_manager = ACameraManager_create();
    assert(app->camera_manager && "cannot create camera manager");

    // 1. list all available cameras
    ACameraIdList *cameraIdList = NULL;
    ret = ACameraManager_getCameraIdList(app->camera_manager, &cameraIdList);
    assert(ret == ACAMERA_OK && cameraIdList && "cannot get cameras ids list");

    // 2. open the first available camera
    ACameraDevice_StateCallbacks camera_state_callbacks = {0};
    ret = ACameraManager_openCamera(app->camera_manager, cameraIdList->cameraIds[0], &camera_state_callbacks, &app->camera_device);
    assert(ret == ACAMERA_OK && app->camera_device && "Failed to open camera device");

    // 3. create output container
    ret = ACaptureSessionOutputContainer_create(&app->output_container);
    assert(ret == ACAMERA_OK && app->output_container && "cannot create output container");

    // 4. create preview session output and add to container
    {
        ret = ACaptureSessionOutput_create(app->preview_window, &app->preview_session_output);
        assert(ret == ACAMERA_OK && app->preview_session_output && "cannot create preview session output");

        ret = ACaptureSessionOutputContainer_add(app->output_container, app->preview_session_output);
        assert(ret == ACAMERA_OK && "cannot add preview session output to the output container");
    }

    // 5. create process session output and add to container
    {
        ACameraMetadata *metadata = NULL;
        ret = ACameraManager_getCameraCharacteristics(app->camera_manager, cameraIdList->cameraIds[0], &metadata);
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

        ret = AImageReader_getWindow(reader, &app->process_window);
        assert(ret == AMEDIA_OK && reader && "cannot get native window from the image reader");

        ret = ACaptureSessionOutput_create(app->process_window, &app->process_session_output);
        assert(ret == ACAMERA_OK && app->process_session_output && "cannot create process session output");

        ret = ACaptureSessionOutputContainer_add(app->output_container, app->process_session_output);
        assert(ret == ACAMERA_OK && "cannot add process session output to the output container");
    }

    // 6. create capture request
    ret = ACameraDevice_createCaptureRequest(app->camera_device, TEMPLATE_PREVIEW, &app->capture_request);
    assert(ret == ACAMERA_OK && app->capture_request && "cannot create capture request");

    {
        // 7. create output target for preview and add to capture request
        ret = ACameraOutputTarget_create(app->preview_window, &app->preview_output_target);
        assert(ret == ACAMERA_OK && app->preview_output_target && "cannot create preview output target");

        ret = ACaptureRequest_addTarget(app->capture_request, app->preview_output_target);
        assert(ret == ACAMERA_OK && "cannot add preview target to capture request");
    }

    {
        // 8. Create output target for process and add to capture request
        ret = ACameraOutputTarget_create(app->process_window, &app->process_output_target);
        assert(ret == ACAMERA_OK && app->process_output_target && "cannot create process output target");

        ret = ACaptureRequest_addTarget(app->capture_request, app->process_output_target);
        assert(ret == ACAMERA_OK && "cannot add process target to capture request");
    }

    // 8. create capture session
    const ACameraCaptureSession_stateCallbacks capture_session_callbacks = {0};
    ret = ACameraDevice_createCaptureSession(app->camera_device, app->output_container, &capture_session_callbacks, &app->camera_capture_session);
    assert(ret == ACAMERA_OK && app->camera_capture_session && "cannot create capture session");

    // 9. start capture session
    ret = ACameraCaptureSession_setRepeatingRequest(app->camera_capture_session, NULL, 1, &app->capture_request, NULL);
    assert(ret == ACAMERA_OK && "cannot start capture session");

    while (app->running) {
        // nothing
    }

    if (cameraIdList) ACameraManager_deleteCameraIdList(cameraIdList);
    if (app->camera_capture_session) {
        ACameraCaptureSession_stopRepeating(app->camera_capture_session);
        ACameraCaptureSession_close(app->camera_capture_session);
    }

    if (app->capture_request) ACaptureRequest_free(app->capture_request);
    if (app->output_container) ACaptureSessionOutputContainer_free(app->output_container);

    if (app->preview_output_target) ACameraOutputTarget_free(app->preview_output_target);
    if (app->preview_session_output) ACaptureSessionOutput_free(app->preview_session_output);

    if (app->process_output_target) ACameraOutputTarget_free(app->process_output_target);
    if (app->process_session_output) ACaptureSessionOutput_free(app->process_session_output);

    if (app->camera_device) ACameraDevice_close(app->camera_device);
    if (app->camera_manager) ACameraManager_delete(app->camera_manager);

    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    LOGI("onNativeWindowCreated");

    AndroidApp *app = (AndroidApp *)activity->instance;

    app->preview_window = window;
    app->running = true;

    pthread_create(&app->thread, NULL, camera_task, app);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    (void)window;
    LOGI("onNativeWindowDestroyed");

    AndroidApp *app = (AndroidApp *)activity->instance;

    app->running = false;

    pthread_join(app->thread, NULL);

    app->preview_window = NULL;
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    AndroidApp *app = malloc(sizeof(AndroidApp));
    memset(app, 0, sizeof(AndroidApp));

    activity->callbacks->onNativeWindowCreated = on_window_init;
    activity->callbacks->onNativeWindowDestroyed = on_window_deinit;
    activity->instance = app;
}
