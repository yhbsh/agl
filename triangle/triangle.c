#include <jni.h>

#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <android/native_activity.h>

#include <pthread.h>

#include <math.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "ENGINE", __VA_ARGS__))

typedef struct {
    ANativeWindow *window;
    AInputQueue *input;

    /* EGL */
    void *egl_display;
    void *egl_surface;
    void *egl_context;
    void *egl_config;

    /* GLES */
    GLuint program, VAO, texture;

    bool running;
    pthread_t thread;
} AndroidApp;

AndroidApp app = {0};

void *render_task(void *arg) {
    (void)arg;

    // Get display
    app.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (app.egl_display == EGL_NO_DISPLAY) {
        LOGE("cannot get EGL display");
        exit(0);
    }

    // Initialize EGL
    if (!eglInitialize(app.egl_display, NULL, NULL)) {
        LOGE("cannot initialize EGL");
        exit(0);
    }

    // Choose config for GLES v2
    EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
    EGLint numConfigs;
    if (!eglChooseConfig(app.egl_display, attribs, &app.egl_config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("cannot choose EGL config");
        eglTerminate(app.egl_display);
        exit(0);
    }

    // Create context for GLES v2
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE}; // GLES 2.0
    app.egl_context = eglCreateContext(app.egl_display, app.egl_config, EGL_NO_CONTEXT, contextAttribs);
    if (app.egl_context == EGL_NO_CONTEXT) {
        LOGE("cannot create EGL context");
        eglTerminate(app.egl_display);
        exit(0);
    }

    // Create window surface
    app.egl_surface = eglCreateWindowSurface(app.egl_display, app.egl_config, app.window, NULL);
    if (app.egl_surface == EGL_NO_SURFACE) {
        LOGE("cannot create EGL window surface");
        eglDestroyContext(app.egl_display, app.egl_context);
        eglTerminate(app.egl_display);
        exit(0);
    }

    // Make current
    if (!eglMakeCurrent(app.egl_display, app.egl_surface, app.egl_surface, app.egl_context)) {
        LOGE("cannot make EGL context current");
        eglDestroySurface(app.egl_display, app.egl_surface);
        eglDestroyContext(app.egl_display, app.egl_context);
        eglTerminate(app.egl_display);
        exit(0);
    }

    GLint width, height;
    eglQuerySurface(app.egl_display, app.egl_surface, EGL_WIDTH, &width);
    eglQuerySurface(app.egl_display, app.egl_surface, EGL_HEIGHT, &height);
    glViewport(0, 0, width, height);

    const char *vsrc = "#version 300 es                                  \n"
                       "layout(location = 0) in vec4 vPosition;          \n"
                       "uniform mat4 uRotationMatrix;                    \n"
                       "void main()                                      \n"
                       "{                                                \n"
                       "   gl_Position = uRotationMatrix * vPosition;    \n"
                       "}                                                \n";

    const char *fsrc = "#version 300 es                              \n"
                       "precision mediump float;                     \n"
                       "out vec4 fragColor;                          \n"
                       "void main()                                  \n"
                       "{                                            \n"
                       "   fragColor = vec4 (1.0, 0.2, 0.2, 1.0);    \n"
                       "}                                            \n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, NULL);
    glCompileShader(fs);

    app.program = glCreateProgram();
    glAttachShader(app.program, vs);
    glAttachShader(app.program, fs);
    glLinkProgram(app.program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLuint VBO;
    float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
    glGenVertexArrays(1, &app.VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(app.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    float previousX = 0.0f;
    float rotationAngle = 0.0f;

    while (app.running) {
        AInputEvent *event = NULL;

        while (AInputQueue_hasEvents(app.input) > 0) {
            if (AInputQueue_getEvent(app.input, &event) >= 0) {
                if (AInputQueue_preDispatchEvent(app.input, event)) {
                    continue;
                }

                int32_t eventType = AInputEvent_getType(event);
                switch (eventType) {
                case AINPUT_EVENT_TYPE_MOTION:
                    if (AMotionEvent_getPointerCount(event) == 1) {
                        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
                        float x = AMotionEvent_getX(event, 0);
                        float y = AMotionEvent_getY(event, 0);

                        if (action == AMOTION_EVENT_ACTION_DOWN) {
                            previousX = x;
                        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                            float deltaX = x - previousX;

                            float rotationSpeed = 0.01f;
                            rotationAngle += deltaX * rotationSpeed;
                            previousX = x;
                        }

                        LOGI("Motion event: x=%f, y=%f", x, y);
                    }
                    break;

                default: LOGI("Unknown input event type: %d", eventType); break;
                }

                AInputQueue_finishEvent(app.input, event, 1);
            }
        }

        float cosAngle = cosf(rotationAngle);
        float sinAngle = sinf(rotationAngle);
        GLfloat rotationMatrix[] = {cosAngle, -sinAngle, 0.0, 0.0, sinAngle, cosAngle, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0};

        GLuint rotationLoc = glGetUniformLocation(app.program, "uRotationMatrix");

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(app.program);
        glUniformMatrix4fv(rotationLoc, 1, GL_FALSE, rotationMatrix);
        glBindVertexArray(app.VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        eglSwapBuffers(app.egl_display, app.egl_surface);
    }

    return NULL;
}

void onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *w) {
    LOGI("onNativeWindowCreated");

    (void)activity;

    app.window = w;
    if (app.window == NULL) {
        LOGE("no window available");
        return;
    }

    app.running = true;
    pthread_create(&app.thread, NULL, render_task, NULL);
}

void onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window) {
    (void)activity;
    (void)window;
    LOGI("onNativeWindowDestroyed");

    app.running = false;
    pthread_join(app.thread, NULL);

    // Unbind EGL context and surface
    if (!eglMakeCurrent(app.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        LOGE("cannot unbind EGL context");
    }

    // Destroy EGL surface and context
    if (!eglDestroySurface(app.egl_display, app.egl_surface)) {
        LOGE("cannot destroy EGL surface");
    }

    if (!eglDestroyContext(app.egl_display, app.egl_context)) {
        LOGE("cannot destroy EGL context");
    }

    // Terminate EGL
    if (!eglTerminate(app.egl_display)) {
        LOGE("cannot terminate EGL");
    }

    app.egl_context = NULL;
    app.egl_display = NULL;
    app.egl_surface = NULL;
    app.window = NULL;
}

void onInputQueueCreated(ANativeActivity *activity, AInputQueue *queue) {
    (void)activity;

    app.input = queue;
}

void onInputQueueDestroyed(ANativeActivity *activity, AInputQueue *queue) {
    (void)activity;

    if (app.input == queue) {
        app.input = NULL;
    }
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->instance = NULL;
}
