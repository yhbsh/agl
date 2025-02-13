#include <jni.h>

#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <pthread.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "ENGINE", __VA_ARGS__))

typedef struct {
    ANativeWindow *window;
    ANativeActivity *activity;

    /* EGL */
    void *egl_display;
    void *egl_surface;
    void *egl_context;
    void *egl_config;

    bool running;
    pthread_t thread;
} AndroidApp;

AndroidApp app = {0};

const uint8_t *load_file(const char *file_path, int *data_len) {
    AAsset *asset = AAssetManager_open(app.activity->assetManager, file_path, AASSET_MODE_BUFFER);
    if (!asset) return NULL;

    int asset_len = AAsset_getLength(asset);
    uint8_t *data = (uint8_t *)malloc(asset_len + 1);
    if (!data) {
        AAsset_close(asset);
        return NULL;
    }

    AAsset_read(asset, data, asset_len);
    data[asset_len] = '\0';
    AAsset_close(asset);

    if (data_len != NULL) *data_len = asset_len;
    return data;
}

void *render_task(void *arg) {
    (void)arg;

    app.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (app.egl_display == EGL_NO_DISPLAY) {
        LOGE("cannot get EGL display");
        exit(0);
    }

    if (!eglInitialize(app.egl_display, NULL, NULL)) {
        LOGE("cannot initialize EGL");
        exit(0);
    }

    EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    EGLint numConfigs;
    if (!eglChooseConfig(app.egl_display, attribs, &app.egl_config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("cannot choose EGL config");
        eglTerminate(app.egl_display);
        exit(0);
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    app.egl_context = eglCreateContext(app.egl_display, app.egl_config, EGL_NO_CONTEXT, contextAttribs);
    if (app.egl_context == EGL_NO_CONTEXT) {
        LOGE("cannot create EGL context");
        eglTerminate(app.egl_display);
        exit(0);
    }

    app.egl_surface = eglCreateWindowSurface(app.egl_display, app.egl_config, app.window, NULL);
    if (app.egl_surface == EGL_NO_SURFACE) {
        LOGE("cannot create EGL window surface");
        eglDestroyContext(app.egl_display, app.egl_context);
        eglTerminate(app.egl_display);
        exit(0);
    }

    if (!eglMakeCurrent(app.egl_display, app.egl_surface, app.egl_surface, app.egl_context)) {
        LOGE("cannot make EGL context current");
        eglDestroySurface(app.egl_display, app.egl_surface);
        eglDestroyContext(app.egl_display, app.egl_context);
        eglTerminate(app.egl_display);
        exit(0);
    }

    GLint success;

    const char *shaderSource = (const char *)load_file("shaders.glsl", NULL);

    const char *vertex_shader_source[2] = {"#version 300 es\n#define VERTEX\n", shaderSource};
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 2, vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertex_shader, sizeof(infoLog), NULL, infoLog);
        LOGE("ERROR: Vertex Shader Compilation Failed:\n%s\n", infoLog);
    }

    const char *fragment_shader_source[2] = {"#version 300 es\n#define FRAGMENT\n", shaderSource};
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 2, fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragment_shader, sizeof(infoLog), NULL, infoLog);
        LOGE("ERROR: Fragment Shader Compilation Failed:\n%s\n", infoLog);
    }

    free((void *)shaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // clang-format off
    float vertices[] = {
        -0.5f, +0.25f, +0.0f, +0.0f,
        +0.5f, +0.25f, +0.0f, +1.0f,
        +0.0f, -0.25f, +1.0f, +0.0f,
    };
    // clang-format on

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    int angleX = 0;
    int angleY = 0;
    int angleZ = 0;

    int data_len;
    const uint8_t *data = load_file("image.jpeg", &data_len);

    int x, y, c;
    uint8_t *data_raw = stbi_load_from_memory(data, data_len, &x, &y, &c, 0);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data_raw);

    stbi_image_free(data_raw);

    while (app.running) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        angleX += 0;
        angleY += 0;
        angleZ += 2;

        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "angleX"), angleX);
        glUniform1i(glGetUniformLocation(program, "angleY"), angleY);
        glUniform1i(glGetUniformLocation(program, "angleZ"), angleZ);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        eglSwapBuffers(app.egl_display, app.egl_surface);
    }

    return NULL;
}

void onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window) {
    LOGI("onNativeWindowCreated");

    (void)activity;

    app.window = window;
    app.running = true;

    pthread_create(&app.thread, NULL, render_task, NULL);
}

void onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window) {
    (void)activity;
    (void)window;
    LOGI("onNativeWindowDestroyed");

    app.running = false;
    pthread_join(app.thread, NULL);

    if (!eglMakeCurrent(app.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) LOGE("cannot unbind EGL context");
    if (!eglDestroySurface(app.egl_display, app.egl_surface)) LOGE("cannot destroy EGL surface");
    if (!eglDestroyContext(app.egl_display, app.egl_context)) LOGE("cannot destroy EGL context");
    if (!eglTerminate(app.egl_display)) LOGE("cannot terminate EGL");

    app.egl_context = NULL;
    app.egl_display = NULL;
    app.egl_surface = NULL;
    app.window = NULL;
}

void onNativeWindowResized(ANativeActivity *activity, ANativeWindow *window) {
    (void)activity;
    (void)window;
    LOGI("onNativeWindowResized");

    if (app.egl_display != EGL_NO_DISPLAY && app.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(app.egl_display, app.egl_surface, app.egl_surface, app.egl_context);
    }
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    app.activity = activity;
    app.activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    app.activity->callbacks->onNativeWindowResized = onNativeWindowResized;
    app.activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
}
