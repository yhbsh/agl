#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <android/native_activity.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cube.h"

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))

static const char *vertex_shader_source = R"(#version 300 es
    precision mediump float;

    layout(location = 0) in vec3 position;
    layout(location = 1) in vec3 color;

    uniform float time;

    out vec3 Color;

    void main() {
        float c = cos(time);
        float s = sin(time);

        mat4 rotationX = mat4(
            1.0, 0.0, 0.0, 0.0,
            0.0, c, -s, 0.0,
            0.0, s, c, 0.0,
            0.0, 0.0, 0.0, 1.0
        );

        mat4 rotationY = mat4(
            c, 0.0, s, 0.0,
            0.0, 1.0, 0.0, 0.0,
            -s, 0.0, c, 0.0,
            0.0, 0.0, 0.0, 1.0
        );

        mat4 rotationZ = mat4(
            c, -s, 0.0, 0.0,
            s, c, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        );

        Color = color;
        gl_Position = rotationZ * rotationX * vec4(position, 1.0);

    }
)";

static const char *fragment_shader_source = R"(#version 300 es
    precision mediump float;

    in vec3 Color;
    out vec4 FragColor;

    void main() {
        FragColor = vec4(Color, 1.0);
    }
)";

GLuint make_cube() {
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const void *)(3 * sizeof(GLfloat)));

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    return VAO;
}

void render_cube(GLuint VAO) {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(cube_indices) / sizeof(cube_indices[0]), GL_UNSIGNED_INT, 0);
}

GLuint make_program() {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, sizeof(info_log), NULL, info_log);
        LOGE("Vertex Shader Compilation Failed:\n%s\n", info_log);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, sizeof(info_log), NULL, info_log);
        LOGE("Fragment Shader Compilation Failed:\n%s\n", info_log);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        LOGE("Program Linking Failed:\n%s\n", info_log);
    }

    glUseProgram(program);

    return program;
}

typedef struct {
    ANativeWindow *window;

    int running;
    pthread_t thread;
} AndroidApp;

float glfwGetTime() {
    static struct timespec start;
    struct timespec now;

    // Initialize start time only once
    if (start.tv_sec == 0 && start.tv_nsec == 0) {
        clock_gettime(CLOCK_MONOTONIC, &start);
    }

    // Get current time
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Calculate elapsed time in seconds
    return (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9f;
}

void *run_main(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;

    void *egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl_display, NULL, NULL);
    EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    int confgs;
    void *egl_config;
    eglChooseConfig(egl_display, attributes, &egl_config, 1, &confgs);
    int context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    void *egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attributes);
    void *egl_surface = eglCreateWindowSurface(egl_display, egl_config, app->window, NULL);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    GLuint cube = make_cube();
    GLuint program = make_program();

    while (app->running) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glUniform1f(glGetUniformLocation(program, "time"), glfwGetTime());
        glUniform1i(glGetUniformLocation(program, "object"), 1);
        render_cube(cube);
        eglSwapBuffers(egl_display, egl_surface);
    }

    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_display, egl_surface);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);

    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    LOG("onNativeWindowCreated");

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->window = window;
    app->running = 1;
    pthread_create(&app->thread, NULL, run_main, app);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    LOG("onNativeWindowDestroyed");
    (void)window;

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->running = 0;
    pthread_join(app->thread, NULL);

    app->window = NULL;
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    AndroidApp *app = (AndroidApp *)malloc(sizeof(AndroidApp));
    memset(app, 0, sizeof(AndroidApp));

    activity->callbacks->onNativeWindowCreated = on_window_init;
    activity->callbacks->onNativeWindowDestroyed = on_window_deinit;
    activity->instance = app;
}
