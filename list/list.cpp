#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))

typedef struct {
    ANativeWindow *window;
    AInputQueue *input;
    ANativeActivity *activity;

    /* EGL */
    void *egl_display;
    void *egl_surface;
    void *egl_context;
    void *egl_config;

    bool is_rendering;
    pthread_t render_thread;
} AndroidApp;

AndroidApp app = {0};

const char *vertex_shader_source = R"(#version 300 es
layout(location = 0) in vec2 vPosition;
uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(vPosition, 0.0, 1.0);
})";

const char *fragment_shader_source = R"(#version 300 es
precision mediump float;
out vec4 fragColor;

void main() {
    fragColor = vec4(0.5, 0.7, 0.4, 1.0);
})";

void *render_task(void *arg) {
    (void)arg;

    app.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (app.egl_display == EGL_NO_DISPLAY) {
        LOG("cannot get EGL display");
        exit(0);
    }

    if (!eglInitialize(app.egl_display, NULL, NULL)) {
        LOG("cannot initialize EGL");
        exit(0);
    }

    EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    EGLint numConfigs;
    if (!eglChooseConfig(app.egl_display, attribs, &app.egl_config, 1, &numConfigs) || numConfigs == 0) {
        LOG("cannot choose EGL config");
        eglTerminate(app.egl_display);
        exit(0);
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    app.egl_context = eglCreateContext(app.egl_display, app.egl_config, EGL_NO_CONTEXT, contextAttribs);
    if (app.egl_context == EGL_NO_CONTEXT) {
        LOG("cannot create EGL context");
        eglTerminate(app.egl_display);
        exit(0);
    }

    app.egl_surface = eglCreateWindowSurface(app.egl_display, app.egl_config, app.window, NULL);
    if (app.egl_surface == EGL_NO_SURFACE) {
        LOG("cannot create EGL window surface");
        eglDestroyContext(app.egl_display, app.egl_context);
        eglTerminate(app.egl_display);
        exit(0);
    }

    if (!eglMakeCurrent(app.egl_display, app.egl_surface, app.egl_surface, app.egl_context)) {
        LOG("cannot make EGL context current");
        eglDestroySurface(app.egl_display, app.egl_surface);
        eglDestroyContext(app.egl_display, app.egl_context);
        eglTerminate(app.egl_display);
        exit(0);
    }

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader_source, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader_source, NULL);
    glCompileShader(fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glUseProgram(program);

    // clang-format off
    float vertices[] = {
        -0.9f, +0.6f, 
        +0.9f, +0.6f, 
        -0.9f, +0.8f, 
        +0.9f, +0.8f,
    };
    // clang-format on

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

    GLuint transformLoc = glGetUniformLocation(program, "transform");
    float transform[16];
    float scroll_offset = 0.0f;
    float scroll_velocity = 0.0f;
    const float damping = 0.95f;

    while (app.is_rendering) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        AInputEvent *event = NULL;
        while (AInputQueue_getEvent(app.input, &event) >= 0) {
            if (AInputQueue_preDispatchEvent(app.input, event)) {
                continue;
            }

            if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
                int32_t action = AMotionEvent_getAction(event);
                float y = AMotionEvent_getY(event, 0);

                static float last_y = 0.0f;
                if (action == AMOTION_EVENT_ACTION_MOVE) {
                    float delta = (y - last_y) * 0.0003f;
                    scroll_velocity += delta;
                }
                last_y = y;
            }

            AInputQueue_finishEvent(app.input, event, 1);
        }

        LOG("%f", scroll_offset);
        if (scroll_offset >= 0.0f) scroll_offset = 0.0f;
        scroll_offset += scroll_velocity;
        scroll_velocity *= damping;

        for (int i = 0; i < 1000; i++) {
            memset(transform, 0, sizeof(float) * 16);

            transform[00] = 1.0f;
            transform[05] = 1.0f;
            transform[10] = 1.0f;
            transform[15] = 1.0f;

            transform[12] = 0.0f;
            transform[13] = i * -0.25f - scroll_offset;

            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, transform);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        eglSwapBuffers(app.egl_display, app.egl_surface);
    }

    return NULL;
}

void onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window) {
    LOG("onNativeWindowCreated");

    (void)activity;

    app.window = window;
    app.is_rendering = true;

    pthread_create(&app.render_thread, NULL, render_task, NULL);
}

void onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window) {
    (void)activity;
    (void)window;
    LOG("onNativeWindowDestroyed");

    app.is_rendering = false;
    pthread_join(app.render_thread, NULL);

    if (!eglMakeCurrent(app.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        LOG("cannot unbind EGL context");
    }

    if (!eglDestroySurface(app.egl_display, app.egl_surface)) {
        LOG("cannot destroy EGL surface");
    }

    if (!eglDestroyContext(app.egl_display, app.egl_context)) {
        LOG("cannot destroy EGL context");
    }

    if (!eglTerminate(app.egl_display)) {
        LOG("cannot terminate EGL");
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

    app.activity = activity;
    app.activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    app.activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    app.activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    app.activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
}
