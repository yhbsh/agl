#include <jni.h>

#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "penger.h"

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))

const char *vertex_shader_source = R"(#version 300 es
precision mediump float;

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texcoord;
layout (location = 2) in vec3 normal;

out vec2 TexCoords;
out vec3 Normal;

uniform float angle;

void main() {
    mat4 rotation_matrix = mat4(
        cos(angle),  0.0, sin(angle), 0.0,
        0.0,         1.0, 0.0,        0.0,
       -sin(angle),  0.0, cos(angle), 0.0,
        0.0,         0.0, 0.0,        1.0
    );

    vec3 rotated_position = (rotation_matrix * vec4(position, 1.0)).xyz;
    vec3 translated_position = rotated_position + vec3(0.0, -0.60, 0.0);
    TexCoords = texcoord;
    Normal = normal;

    gl_Position = vec4(translated_position, 1.0);
}
)";

const char *fragment_shader_source = R"(#version 300 es
precision mediump float;

in vec2 TexCoords;
in vec3 Normal;
out vec4 FragColor;

void main() {
    vec3 texColor = vec3(TexCoords, 0.5);
    vec3 normalColor = normalize(Normal) * 0.5 + 0.5;

    // Blend texture and normal colors
    vec3 color = mix(texColor, normalColor, 0.5); // Adjust the blend factor as needed
    FragColor = vec4(color, 1.0);
}
)";

typedef struct {
    ANativeWindow *window;
    AInputQueue *input;

    int running;
    pthread_t thread;
} AndroidApp;

void check_shader_compile_status(GLuint shader) {
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *log = (char *)malloc(length);
        glGetShaderInfoLog(shader, length, &length, log);
        LOGE("Shader compile error: %s", log);
        free(log);
    }
}

void check_program_link_status(GLuint program) {
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *log = (char *)malloc(length);
        glGetProgramInfoLog(program, length, &length, log);
        LOGE("Program link error: %s", log);
        free(log);
    }
}

void *run_main(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;

    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl_display, NULL, NULL);

    EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};

    EGLConfig egl_config;
    EGLint num_configs;
    eglChooseConfig(egl_display, attributes, &egl_config, 1, &num_configs);

    EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attributes);
    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, egl_config, app->window, NULL);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    GLuint VBO_vertices;
    glGenBuffers(1, &VBO_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const void *)0);

    GLuint VBO_texcoords;
    glGenBuffers(1, &VBO_texcoords);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_texcoords);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const void *)0);

    GLuint VBO_normals;
    glGenBuffers(1, &VBO_normals);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normals);
    glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const void *)0);

    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vertex_indices), vertex_indices, GL_STATIC_DRAW);

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    check_shader_compile_status(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    check_shader_compile_status(fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    check_program_link_status(program);
    glUseProgram(program);

    GLint angle_location = glGetUniformLocation(program, "angle");
    float i = 0;
    AInputEvent *event = NULL;

    while (app->running) {
        while (AInputQueue_getEvent(app->input, &event) >= 0) {
            if (AInputQueue_preDispatchEvent(app->input, event)) continue;
            AInputQueue_finishEvent(app->input, event, 0);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        glUniform1f(angle_location, 2 * i);
        i += 0.010;
        glDrawElements(GL_TRIANGLES, sizeof(vertex_indices), GL_UNSIGNED_INT, 0);
        eglSwapBuffers(egl_display, egl_surface);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO_vertices);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(program);

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
    (void)savedState;
    (void)savedStateSize;

    AndroidApp *app = (AndroidApp *)malloc(sizeof(AndroidApp));
    memset(app, 0, sizeof(AndroidApp));

    activity->callbacks->onNativeWindowCreated = on_window_init;
    activity->callbacks->onNativeWindowDestroyed = on_window_deinit;
    activity->callbacks->onInputQueueCreated = on_input_init;
    activity->callbacks->onInputQueueDestroyed = on_input_deinit;
    activity->instance = app;
}
