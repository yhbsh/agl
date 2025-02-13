#include <android/log.h>
#include <android/native_activity.h>

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>

#include <pthread.h>
#include <stdlib.h>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "ENGINE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_INFO, "ENGINE", __VA_ARGS__))

typedef struct AndroidApp {
    bool running;
    pthread_t thread;
} AndroidApp;

void custom_callback(void *ptr, int level, const char *fmt, va_list vl) {
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
            LOG("    Last message repeated %d times\n", count);
            count = 0;
        }
        strcpy(prev, line);
        LOG("%s", line);
    }
}

void *stream_task(void *arg) {
    AndroidApp *app = (AndroidApp *)arg;

    int ret;

    avdevice_register_all();
    // av_log_set_callback(custom_callback);

    AVDictionary *opt = NULL;
    av_dict_set(&opt, "video_size", "480x680", 0);
    av_dict_set(&opt, "framerate", "60", 0);
    av_dict_set(&opt, "camera_index", "0", 0);
    av_dict_set(&opt, "input_queue_size", "5", 0);

    AVFormatContext *input_format_context = NULL;
    const AVInputFormat *android_camera = av_find_input_format("android_camera");
    if (!android_camera) {
        LOGE("[ERROR]: android camera not found\n");
        exit(0);
    }
    if ((ret = avformat_open_input(&input_format_context, "0", android_camera, &opt)) < 0) {
        LOGE("[ERROR]: avformat_open_input: %s\n", av_err2str(ret));
        exit(0);
    }

    if ((ret = avformat_find_stream_info(input_format_context, NULL)) < 0) {
        LOGE("[ERROR]: avformat_find_stream_info: %s\n", av_err2str(ret));
        exit(0);
    }

    const AVCodec *decoder = NULL;
    if ((ret = av_find_best_stream(input_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0)) < 0) {
        LOGE("[ERROR]: av_find_best_stream: %s\n", av_err2str(ret));
        exit(0);
    }
    AVStream *input_stream = input_format_context->streams[ret];
    AVCodecContext *decoder_context = avcodec_alloc_context3(decoder);
    if ((ret = avcodec_parameters_to_context(decoder_context, input_stream->codecpar)) < 0) {
        LOGE("[ERROR]: avcodec_parameters_to_context: %s\n", av_err2str(ret));
        exit(0);
    }
    if ((ret = avcodec_open2(decoder_context, decoder, NULL)) < 0) {
        LOGE("[ERROR]: avcodec_open2: %s\n", av_err2str(ret));
        exit(0);
    }

    LOG("%dx%d", decoder_context->width, decoder_context->height);

    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        LOGE("[ERROR]: cannot find encoder\n");
        exit(0);
    }
    AVCodecContext *encoder_context = avcodec_alloc_context3(encoder);
    if (!encoder_context) {
        LOGE("[ERROR]: cannot allocate encoder context\n");
        exit(0);
    }

    encoder_context->bit_rate = 1 * 1000 * 1000;
    encoder_context->width = decoder_context->width;
    encoder_context->height = decoder_context->height;
    encoder_context->pix_fmt = decoder_context->pix_fmt;
    encoder_context->time_base = (AVRational){1, 60};
    encoder_context->framerate = (AVRational){60, 1};
    encoder_context->gop_size = 10;
    encoder_context->max_b_frames = 1;
    encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if ((ret = avcodec_open2(encoder_context, encoder, NULL)) < 0) {
        LOGE("[ERROR]: avcodec_open2: %s\n", av_err2str(ret));
        exit(0);
    }

    AVFormatContext *output_format_context = NULL;
    if ((ret = avformat_alloc_output_context2(&output_format_context, NULL, "flv", NULL)) < 0) {
        LOGE("[ERROR]: avformat_alloc_output_context2: %s\n", av_err2str(ret));
        exit(0);
    }

    AVStream *output_stream = avformat_new_stream(output_format_context, encoder);
    output_stream->id = 0;
    if ((ret = avcodec_parameters_from_context(output_stream->codecpar, encoder_context)) < 0) {
        LOGE("[ERROR]: avcodec_parameters_from_context: %s\n", av_err2str(ret));
        exit(0);
    }

    if ((ret = avio_open(&output_format_context->pb, "rtmp://192.168.1.187:1935/live/stream", AVIO_FLAG_WRITE)) < 0) {
        LOGE("[ERROR]: avio_open: %s\n", av_err2str(ret));
        exit(0);
    }
    if ((ret = avformat_write_header(output_format_context, NULL)) < 0) {
        LOGE("[ERROR]: avformat_write_header: %s\n", av_err2str(ret));
        exit(0);
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while (app->running) {
        ret = av_read_frame(input_format_context, pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret == AVERROR(EAGAIN) || pkt->stream_index != input_stream->index) {
            continue;
        }

        ret = avcodec_send_packet(decoder_context, pkt);
        while (ret >= 0) {
            ret = avcodec_receive_frame(decoder_context, frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                break;
            } else if (ret < 0) {
                LOGE("[ERROR]: avcodec_receive_frame: %s\n", av_err2str(ret));
                exit(0);
            }

            ret = avcodec_send_frame(encoder_context, frame);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }

            if (ret < 0) {
                LOGE("avcodec_send_frame: %s\n", av_err2str(ret));
                exit(0);
            }

            while (ret >= 0) {
                ret = avcodec_receive_packet(encoder_context, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    LOGE("avcodec_receive_packet: %s\n", av_err2str(ret));
                    exit(0);
                }

                av_packet_rescale_ts(pkt, encoder_context->time_base, output_stream->time_base);

                AVRational *time_base = &output_stream->time_base;
                // LOG("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n", av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base), av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base), av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base), pkt->stream_index);

                if ((ret = av_interleaved_write_frame(output_format_context, pkt)) < 0) {
                    LOGE("av_interleaved_write_frame: %s\n", av_err2str(ret));
                    exit(0);
                }
                av_packet_unref(pkt);
            }
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&decoder_context);
    avcodec_free_context(&encoder_context);
    if (output_format_context && output_format_context->pb) avio_closep(&output_format_context->pb);
    avformat_free_context(output_format_context);
    avformat_close_input(&input_format_context);
    av_dict_free(&opt);

    return NULL;
}

void on_window_init(ANativeActivity *activity, ANativeWindow *window) {
    (void)window;

    LOGE("onNativeWindowCreated");
    AndroidApp *app = (AndroidApp *)activity->instance;

    app->running = true;

    pthread_create(&app->thread, NULL, stream_task, app);
}

void on_window_deinit(ANativeActivity *activity, ANativeWindow *window) {
    (void)window;
    LOGE("onNativeWindowDestroyed");

    AndroidApp *app = (AndroidApp *)activity->instance;

    app->running = false;

    pthread_join(app->thread, NULL);
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
