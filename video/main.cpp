extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}

#include <jni.h>

#include <GLES/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <android/native_activity.h>

#include <pthread.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))

const char *vertex_shader_source = R"(#version 300 es
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    TexCoord = texCoord;
})";

const char *fragment_shader_source = R"(#version 300 es
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D textureY;
uniform sampler2D textureU;
uniform sampler2D textureV;

void main() {
    float y = texture(textureY, TexCoord).r;
    float u = texture(textureU, TexCoord).r - 0.5;
    float v = texture(textureV, TexCoord).r - 0.5;

    y = 1.164 * (y - 0.0625);

    float r = y + 1.596 * v;
    float g = y - 0.391 * u - 0.813 * v;
    float b = y + 2.018 * u;

    FragColor = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
})";

typedef struct {
    ANativeWindow *window;

    bool running;
    pthread_t thread;
} AndroidApp;

void *run_main(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;

    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl_display, NULL, NULL);

    EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};

    EGLConfig egl_config;
    EGLint num_configs;
    eglChooseConfig(egl_display, attributes, &egl_config, 1, &num_configs);

    EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attributes);
    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, egl_config, app->window, NULL);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    GLuint textures[3];
    glGenTextures(3, textures);

    // clang-format off
    GLfloat vertices[] = {
        -1.0f, +1.0f, +0.0f, +0.0f,
        -1.0f, -1.0f, +0.0f, +1.0f,
        +1.0f, +1.0f, +1.0f, +0.0f,
        +1.0f, -1.0f, +1.0f, +1.0f,
    };
    GLuint indices[] = {
        0, 1, 2,
        1, 2, 3,
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)(2 * sizeof(GLfloat)));

    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        char buffer[512];
        glGetProgramInfoLog(program, 512, NULL, buffer);
        LOGE("Shader program linking error: %s", buffer);
    }

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "textureY"), 0);
    glUniform1i(glGetUniformLocation(program, "textureU"), 1);
    glUniform1i(glGetUniformLocation(program, "textureV"), 2);

    int ret;

    AVFormatContext *format_context;
    if ((ret = avformat_open_input(&format_context, "rtmp://192.168.8.7:1935/live/stream", NULL, NULL)) < 0) {
        LOGE("[ERROR]: avformat_open_input %s", av_err2str(ret));
        return NULL;
    }

    if ((ret = avformat_find_stream_info(format_context, NULL)) < 0) {
        LOG("[ERROR]: avformat_find_stream_info %s", av_err2str(ret));
        exit(1);
    }

    const AVCodec *vcodec = NULL;
    if ((ret = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, 0, &vcodec, 0)) < 0) {
        LOG("[ERROR]: av_find_best_stream %s", av_err2str(ret));
        exit(1);
    }

    AVStream *vstream = format_context->streams[ret];

    AVCodecContext *codec_context = avcodec_alloc_context3(vcodec);
    if (!codec_context) {
        LOG("[ERROR]: avcodec_alloc_context3");
        exit(1);
    }
    if ((ret = avcodec_parameters_to_context(codec_context, vstream->codecpar)) < 0) {
        LOG("[ERROR]: avcodec_parameters_to_context: %s", av_err2str(ret));
        exit(1);
    }

    av_opt_set_int(codec_context, "threads", 16, 0);
    if ((ret = avcodec_open2(codec_context, vcodec, NULL)) < 0) {
        LOG("[ERROR]: avcodec_open2: %s", av_err2str(ret));
        exit(1);
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        LOG("[ERROR]: av_packet_alloc");
        exit(1);
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOG("[ERROR]: av_frame_alloc");
        exit(1);
    }

    while (app->running) {
        ret = av_read_frame(format_context, pkt);
        if (ret == AVERROR_EOF) {
            av_seek_frame(format_context, vstream->index, 0, 0);
            avcodec_flush_buffers(codec_context);
        }
        if (ret == AVERROR(EAGAIN)) continue;

        if (vstream && pkt->stream_index == vstream->index) {
            if ((ret = avcodec_send_packet(codec_context, pkt)) < 0) {
                LOG("[ERROR]: avcodec_send_packet: %s", av_err2str(ret));
                exit(1);
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_context, frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) break;

                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures[0]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame->width / 1, frame->height / 1, 0, GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, textures[1]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame->width / 2, frame->height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, textures[2]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame->width / 2, frame->height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                eglSwapBuffers(egl_display, egl_surface);
            }
        }

        av_packet_unref(pkt);
    }

    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    LOG("onNativeWindowCreated");

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->window = window;
    app->running = true;
    pthread_create(&app->thread, NULL, run_main, app);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    LOG("onNativeWindowDestroyed");
    (void)window;

    AndroidApp *app = (AndroidApp *)activity->instance;
    app->running = false;
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
