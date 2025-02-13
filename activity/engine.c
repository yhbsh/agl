#include <jni.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <android/log.h>
#include <android/native_window_jni.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/jni.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))

int ret;

static ANativeWindow *window = NULL;
static ANativeWindow_Buffer buffer;
static AVFormatContext *format_context = NULL;
static AVCodecContext *decoder_context = NULL;
static AVStream *stream = NULL;
static AVPacket *pkt = NULL;
static AVFrame *frame = NULL;
static AVFrame *tmp_frame = NULL;
static struct SwsContext *sws_context = NULL;
static int64_t launch_time;

void custom_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
    (void)ptr;
    (void)level;

    static int count = 0;
    static char prev[120] = {0};
    char line[120];

    vsnprintf(line, sizeof(line), fmt, vl);

    if (!strcmp(line, prev)) {
        count++;
    } else {
        if (count > 0) {
            LOG("    Last message repeated %d times", count);
            count = 0;
        }
        strcpy(prev, line);
        LOG("%s", line);
    }
}

JNIEXPORT void JNICALL Java_com_example_activity_CustomSurfaceView_init(JNIEnv *env, jobject obj, jobject surface) {
    launch_time = av_gettime_relative();

    // av_log_set_callback(custom_log_callback);

    window = ANativeWindow_fromSurface(env, surface);
    if (window == NULL) return;

    JavaVM *javaVM;
    (*env)->GetJavaVM(env, &javaVM);
    av_jni_set_java_vm(javaVM, NULL);

    AVDictionary *options = NULL;
    av_dict_set(&options, "buffer_size", "32", 0);
    av_dict_set(&options, "max_delay", "0", 0);
    if ((ret = avformat_open_input(&format_context, "http://storage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4", NULL, &options)) < 0) {
        LOGE("[ERROR]: avformat_open_input: %s", av_err2str(ret));
        return;
    }
    format_context->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;

    if ((ret = avformat_find_stream_info(format_context, NULL)) < 0) {
        LOGE("[ERROR]: avformat_find_stream_info: %s", av_err2str(ret));
        return;
    }

    const AVCodec *decoder = avcodec_find_decoder_by_name("h264");
    if (!decoder) {
        LOGE("[ERROR]: decoder not found");
        return;
    }
    if ((ret = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
        LOGE("[ERROR]: av_find_best_stream: %s", av_err2str(ret));
        return;
    }

    stream = format_context->streams[ret];
    decoder_context = avcodec_alloc_context3(decoder);
    if ((ret = avcodec_parameters_to_context(decoder_context, stream->codecpar)) < 0) {
        LOGE("[ERROR]: avcodec_parameters_to_context: %s", av_err2str(ret));
        return;
    }

    if ((ret = avcodec_open2(decoder_context, decoder, NULL)) < 0) {
        LOGE("[ERROR]: avcodec_open2: %s", av_err2str(ret));
        return;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    tmp_frame = av_frame_alloc();
}

JNIEXPORT void JNICALL Java_com_example_activity_CustomSurfaceView_step(JNIEnv *env, jobject obj, jint width, jint height) {
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBX_8888);

    ret = av_read_frame(format_context, pkt);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN) || pkt->stream_index != stream->index) {
        av_packet_unref(pkt);
        return;
    }

    ret = avcodec_send_packet(decoder_context, pkt);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(decoder_context, frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret < 0) {
            LOGE("[ERROR]: avcodec_receive_frame: %s", av_err2str(ret));
            return;
        }

        int64_t pts = (1000 * 1000 * frame->pts * stream->time_base.num) / stream->time_base.den;
        int64_t rts = av_gettime_relative() - launch_time;
        if (pts > rts) av_usleep(pts - rts);
        LOG("Delay: %ld | Frame: %s %p w: %d h: %d ls: %d", pts - rts, av_get_pix_fmt_name(frame->format), frame->data[0], frame->width, frame->height, frame->linesize[0]);

        if (!sws_context) {
            sws_context = sws_getContext(frame->width, frame->height, frame->format, width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
        }

        if ((ret = sws_scale_frame(sws_context, tmp_frame, frame)) < 0) {
            LOGE("[ERROR]: sws_scale_frame: %s", av_err2str(ret));
            return;
        }
        if (ANativeWindow_lock(window, &buffer, NULL) != 0) {
            LOGE("[ERROR]: Unable to lock the native window buffer");
            return;
        }

        if (buffer.width != width || buffer.height != height) {
            LOGE("[ERROR]: Buffer dimensions mismatch");
            ANativeWindow_unlockAndPost(window);
            return;
        }

        uint8_t *dst = (uint8_t *)buffer.bits;
        const uint8_t *src = tmp_frame->data[0];
        int frame_line_size = tmp_frame->linesize[0];

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                dst[x * 4 + 0] = src[x * 4 + 0];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4 + 2];
                dst[x * 4 + 3] = src[x * 4 + 3];
            }
            dst += buffer.stride * 4;
            src += frame_line_size;
        }
        // LOG("Frame: %s %p w: %d h: %d ls: %d %p w: %d h: %d", av_get_pix_fmt_name(tmp_frame->format), tmp_frame->data[0], tmp_frame->width, tmp_frame->height, tmp_frame->linesize[0], buffer.bits, buffer.width, buffer.height);
        ANativeWindow_unlockAndPost(window);
    }

    av_packet_unref(pkt);
}

JNIEXPORT void JNICALL Java_com_example_activity_CustomSurfaceView_deinit(JNIEnv *env, jobject obj) {
    if (window != NULL) {
        ANativeWindow_release(window);
        window = NULL;
    }
    if (decoder_context != NULL) avcodec_free_context(&decoder_context);
    if (format_context != NULL) avformat_close_input(&format_context);
    if (pkt != NULL) av_packet_free(&pkt);
    if (frame != NULL) av_frame_free(&frame);
}
