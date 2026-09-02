/* screen_rec - the capture+encode daemon. No GL, no window: its whole job is
 * (1) negotiate ONE screen-capture session with the compositor via
 * xdg-desktop-portal + PipeWire (so the GNOME "Share" picker only ever pops
 * up once per run, not once per recording), (2) continuously mirror a
 * downscaled RGBA32 preview frame to pieces/display/rgb_frame.raw + a
 * frame_w/frame_h receipt -- same shape screen_rec_gui.c (ported from
 * mutaclsym's gl_mirror.c) already knows how to poll and blit, and (3) poll
 * pieces/control/record_command.txt for "start"/"stop" and mux full-res
 * frames to a real .mp4 via libx264 while a recording is active.
 *
 * Self-contained, no shared .h files -- same convention as
 * 150.gl-canvas/system/import_pet.c. */
#define _GNU_SOURCE

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#define MAX_PATH 4096
#define PREVIEW_MAX_DIM 480
#define PREVIEW_MIN_INTERVAL_MS 2000 /* throttle idle-preview writes to ~0.5fps */

/* ---------- project paths (resolve_root convention, see import_pet.c) ---------- */

static char project_root[MAX_PATH] = ".";
static char recordings_dir[MAX_PATH];
static char frame_raw_path[MAX_PATH];
static char frame_receipt_path[MAX_PATH];
static char recorder_state_path[MAX_PATH];
static char control_command_path[MAX_PATH];

static char pid_file_path[MAX_PATH];

static void write_pid_file(void)
{
    snprintf(pid_file_path, sizeof(pid_file_path), "/tmp/screen_rec.pid");
    FILE *f = fopen(pid_file_path, "w");
    if (f) { fprintf(f, "%d\n", getpid()); fclose(f); }
}

static void remove_pid_file(void)
{
    if (pid_file_path[0]) remove(pid_file_path);
}

static void resolve_root(void)
{
    const char *env = getenv("SCREENREC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    else if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");

    snprintf(recordings_dir, sizeof(recordings_dir), "%s/recordings", project_root);
    snprintf(frame_raw_path, sizeof(frame_raw_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(frame_receipt_path, sizeof(frame_receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    snprintf(recorder_state_path, sizeof(recorder_state_path), "%s/pieces/display/recorder_state.receipt.txt", project_root);
    snprintf(control_command_path, sizeof(control_command_path), "%s/pieces/control/record_command.txt", project_root);

    mkdir(recordings_dir, 0755);
}

/* ---------- portal (xdg-desktop-portal ScreenCast, via GDBus) ---------- */

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define SCREENCAST_IFACE "org.freedesktop.portal.ScreenCast"
#define REQUEST_IFACE "org.freedesktop.portal.Request"

typedef struct {
    GMainLoop *loop;
    guint32 response_code;
    GVariant *results;
} RequestWait;

static void on_response_signal(GDBusConnection *conn, const gchar *sender,
                               const gchar *object_path, const gchar *interface,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data)
{
    (void)conn; (void)sender; (void)object_path; (void)interface; (void)signal_name;
    RequestWait *w = user_data;
    g_variant_get(parameters, "(u@a{sv})", &w->response_code, &w->results);
    g_main_loop_quit(w->loop);
}

static GVariant *call_request_method(GDBusConnection *conn, const char *method, GVariant *params)
{
    GError *error = NULL;
    GVariant *ret = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT_PATH, SCREENCAST_IFACE,
        method, params, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (!ret) {
        fprintf(stderr, "portal: %s call failed: %s\n", method, error->message);
        g_error_free(error);
        return NULL;
    }
    gchar *handle_path = NULL;
    g_variant_get(ret, "(o)", &handle_path);
    g_variant_unref(ret);

    RequestWait wait = { g_main_loop_new(NULL, FALSE), 1, NULL };
    guint sub_id = g_dbus_connection_signal_subscribe(
        conn, PORTAL_BUS_NAME, REQUEST_IFACE, "Response", handle_path, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_response_signal, &wait, NULL);

    g_main_loop_run(wait.loop);

    g_dbus_connection_signal_unsubscribe(conn, sub_id);
    g_main_loop_unref(wait.loop);
    g_free(handle_path);

    if (wait.response_code != 0) {
        fprintf(stderr, "portal: %s was denied or cancelled (code %u)\n", method, wait.response_code);
        if (wait.results) g_variant_unref(wait.results);
        return NULL;
    }
    return wait.results;
}

/* Blocks until the user responds to the GNOME picker dialog. Returns 0 on
 * success and fills *out_pw_fd / *out_node_id, -1 on failure/cancel. */
static int portal_request_screencast(int *out_pw_fd, uint32_t *out_node_id)
{
    GError *error = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) {
        fprintf(stderr, "portal: cannot connect to session bus: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    static int token_counter = 0;
    GVariantBuilder opts;

    gchar *session_token = g_strdup_printf("screenrec_session_%d_%d", getpid(), token_counter++);
    gchar *req_token = g_strdup_printf("screenrec_req_%d_%d", getpid(), token_counter++);

    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&opts, "{sv}", "session_handle_token", g_variant_new_string(session_token));
    g_variant_builder_add(&opts, "{sv}", "handle_token", g_variant_new_string(req_token));

    GVariant *results = call_request_method(conn, "CreateSession", g_variant_new("(a{sv})", &opts));
    if (!results) { g_free(session_token); g_free(req_token); return -1; }

    gchar *session_handle = NULL;
    g_variant_lookup(results, "session_handle", "s", &session_handle);
    g_variant_unref(results);
    if (!session_handle) {
        fprintf(stderr, "portal: CreateSession returned no session_handle\n");
        g_free(session_token); g_free(req_token);
        return -1;
    }

    g_free(req_token);
    req_token = g_strdup_printf("screenrec_req_%d_%d", getpid(), token_counter++);
    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&opts, "{sv}", "handle_token", g_variant_new_string(req_token));
    g_variant_builder_add(&opts, "{sv}", "types", g_variant_new_uint32(1));
    g_variant_builder_add(&opts, "{sv}", "multiple", g_variant_new_boolean(FALSE));
    g_variant_builder_add(&opts, "{sv}", "cursor_mode", g_variant_new_uint32(2));

    results = call_request_method(conn, "SelectSources", g_variant_new("(oa{sv})", session_handle, &opts));
    if (!results) { g_free(session_handle); g_free(session_token); g_free(req_token); return -1; }
    g_variant_unref(results);

    g_free(req_token);
    req_token = g_strdup_printf("screenrec_req_%d_%d", getpid(), token_counter++);
    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&opts, "{sv}", "handle_token", g_variant_new_string(req_token));

    results = call_request_method(conn, "Start", g_variant_new("(osa{sv})", session_handle, "", &opts));
    if (!results) { g_free(session_handle); g_free(session_token); g_free(req_token); return -1; }

    GVariant *streams = g_variant_lookup_value(results, "streams", G_VARIANT_TYPE("a(ua{sv})"));
    if (!streams) {
        fprintf(stderr, "portal: Start response had no streams\n");
        g_variant_unref(results);
        g_free(session_handle); g_free(session_token); g_free(req_token);
        return -1;
    }
    GVariantIter iter;
    guint32 node_id = 0;
    GVariant *stream_props = NULL;
    g_variant_iter_init(&iter, streams);
    g_variant_iter_next(&iter, "(u@a{sv})", &node_id, &stream_props);
    if (stream_props) g_variant_unref(stream_props);
    g_variant_unref(streams);
    g_variant_unref(results);

    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
    GUnixFDList *fd_list = NULL;
    GVariant *fd_ret = g_dbus_connection_call_with_unix_fd_list_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT_PATH, SCREENCAST_IFACE,
        "OpenPipeWireRemote", g_variant_new("(oa{sv})", session_handle, &opts),
        G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &fd_list, NULL, &error);

    g_free(session_handle);
    g_free(session_token);
    g_free(req_token);

    if (!fd_ret) {
        fprintf(stderr, "portal: OpenPipeWireRemote failed: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return -1;
    }
    gint32 fd_index = 0;
    g_variant_get(fd_ret, "(h)", &fd_index);
    g_variant_unref(fd_ret);

    int pw_fd = g_unix_fd_list_get(fd_list, fd_index, &error);
    g_object_unref(fd_list);
    if (pw_fd < 0) {
        fprintf(stderr, "portal: failed to get fd: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    *out_pw_fd = pw_fd;
    *out_node_id = node_id;
    return 0;
}

/* ---------- encoder (libavcodec/libavformat, libx264) ---------- */

static AVFormatContext *g_fmt_ctx = NULL;
static AVCodecContext *g_codec_ctx = NULL;
static AVStream *g_out_stream = NULL;
static struct SwsContext *g_enc_sws = NULL;
static AVFrame *g_yuv_frame = NULL;
static AVPacket *g_pkt = NULL;
static int64_t g_last_pts_ms = -1;
static int g_frames_encoded = 0;
static char g_recording_path[MAX_PATH];
static int g_recording = 0;

static enum AVPixelFormat spa_format_to_av(int spa_fmt)
{
    switch (spa_fmt) {
    case SPA_VIDEO_FORMAT_RGBx: return AV_PIX_FMT_RGB0;
    case SPA_VIDEO_FORMAT_BGRx: return AV_PIX_FMT_BGR0;
    case SPA_VIDEO_FORMAT_RGBA: return AV_PIX_FMT_RGBA;
    case SPA_VIDEO_FORMAT_BGRA: return AV_PIX_FMT_BGRA;
    default: return AV_PIX_FMT_NONE;
    }
}

static void write_recorder_state(void)
{
    FILE *f = fopen(recorder_state_path, "w");
    if (!f) return;
    fprintf(f, "receipt_type=recorder_state\n");
    fprintf(f, "recording=%d\n", g_recording);
    fprintf(f, "output_path=%s\n", g_recording ? g_recording_path : "");
    fprintf(f, "frames_encoded=%d\n", g_frames_encoded);
    fclose(f);
}

static void drain_encoder_packets(void)
{
    int ret = 0;
    while (ret >= 0) {
        ret = avcodec_receive_packet(g_codec_ctx, g_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) break;
        av_packet_rescale_ts(g_pkt, g_codec_ctx->time_base, g_out_stream->time_base);
        g_pkt->stream_index = g_out_stream->index;
        av_interleaved_write_frame(g_fmt_ctx, g_pkt);
        av_packet_unref(g_pkt);
    }
}

static int start_recording(int width, int height, int spa_fmt, int fps_hint)
{
    enum AVPixelFormat src_fmt = spa_format_to_av(spa_fmt);
    if (src_fmt == AV_PIX_FMT_NONE) {
        fprintf(stderr, "encoder: unsupported source pixel format %d\n", spa_fmt);
        return -1;
    }

    time_t now = time(NULL);
    snprintf(g_recording_path, sizeof(g_recording_path), "%s/rec_%ld.mp4", recordings_dir, (long)now);

    if (avformat_alloc_output_context2(&g_fmt_ctx, NULL, NULL, g_recording_path) < 0 || !g_fmt_ctx) {
        fprintf(stderr, "encoder: could not deduce output format for '%s'\n", g_recording_path);
        return -1;
    }
    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        fprintf(stderr, "encoder: libx264 not available in this ffmpeg build\n");
        avformat_free_context(g_fmt_ctx); g_fmt_ctx = NULL;
        return -1;
    }

    g_out_stream = avformat_new_stream(g_fmt_ctx, NULL);
    g_codec_ctx = avcodec_alloc_context3(codec);

    g_codec_ctx->width = width;
    g_codec_ctx->height = height;
    g_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    g_codec_ctx->time_base = (AVRational){1, 1000};
    g_codec_ctx->framerate = (AVRational){fps_hint > 0 ? fps_hint : 30, 1};
    g_codec_ctx->gop_size = (fps_hint > 0 ? fps_hint : 30) * 2;
    g_codec_ctx->bit_rate = 6000000;
    if (g_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        g_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(g_codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(g_codec_ctx->priv_data, "tune", "zerolatency", 0);

    if (avcodec_open2(g_codec_ctx, codec, NULL) < 0 ||
        avcodec_parameters_from_context(g_out_stream->codecpar, g_codec_ctx) < 0) {
        fprintf(stderr, "encoder: open/parameters failed\n");
        goto fail;
    }
    g_out_stream->time_base = g_codec_ctx->time_base;

    if (!(g_fmt_ctx->oformat->flags & AVFMT_NOFILE) &&
        avio_open(&g_fmt_ctx->pb, g_recording_path, AVIO_FLAG_WRITE) < 0) {
        fprintf(stderr, "encoder: avio_open failed for '%s'\n", g_recording_path);
        goto fail;
    }
    if (avformat_write_header(g_fmt_ctx, NULL) < 0) {
        fprintf(stderr, "encoder: avformat_write_header failed\n");
        goto fail;
    }

    g_enc_sws = sws_getContext(width, height, src_fmt, width, height, AV_PIX_FMT_YUV420P,
                               SWS_BILINEAR, NULL, NULL, NULL);
    g_yuv_frame = av_frame_alloc();
    g_yuv_frame->format = AV_PIX_FMT_YUV420P;
    g_yuv_frame->width = width;
    g_yuv_frame->height = height;
    if (!g_enc_sws || av_frame_get_buffer(g_yuv_frame, 32) < 0) {
        fprintf(stderr, "encoder: sws/frame buffer setup failed\n");
        goto fail;
    }

    g_pkt = av_packet_alloc();
    g_last_pts_ms = -1;
    g_frames_encoded = 0;
    g_recording = 1;
    write_recorder_state();
    fprintf(stderr, "screen_rec: recording started -> %s\n", g_recording_path);
    return 0;

fail:
    if (g_codec_ctx) avcodec_free_context(&g_codec_ctx);
    if (g_fmt_ctx) { avformat_free_context(g_fmt_ctx); g_fmt_ctx = NULL; }
    return -1;
}

static void stop_recording(void)
{
    if (!g_recording) return;

    avcodec_send_frame(g_codec_ctx, NULL);
    drain_encoder_packets();
    av_write_trailer(g_fmt_ctx);

    av_packet_free(&g_pkt);
    av_frame_free(&g_yuv_frame);
    sws_freeContext(g_enc_sws); g_enc_sws = NULL;
    avcodec_free_context(&g_codec_ctx);
    if (g_fmt_ctx->pb && !(g_fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&g_fmt_ctx->pb);
    avformat_free_context(g_fmt_ctx);
    g_fmt_ctx = NULL;

    g_recording = 0;
    fprintf(stderr, "screen_rec: recording stopped -> %s (%d frames)\n", g_recording_path, g_frames_encoded);
    write_recorder_state();
}

static void encode_frame(const uint8_t *data, int stride, int64_t pts_ms)
{
    if (!g_recording || pts_ms <= g_last_pts_ms) return;

    const uint8_t *src_slices[1] = { data };
    int src_stride[1] = { stride };

    if (av_frame_make_writable(g_yuv_frame) < 0) return;
    sws_scale(g_enc_sws, src_slices, src_stride, 0, g_codec_ctx->height, g_yuv_frame->data, g_yuv_frame->linesize);

    g_yuv_frame->pts = pts_ms;
    g_last_pts_ms = pts_ms;

    if (avcodec_send_frame(g_codec_ctx, g_yuv_frame) < 0) return;
    drain_encoder_packets();

    g_frames_encoded++;
    if (g_frames_encoded % 60 == 0) write_recorder_state();
}

/* ---------- preview mirror (rgb_frame.raw, same shape gl_mirror.c reads) ---------- */

static struct SwsContext *g_preview_sws = NULL;
static uint8_t *g_preview_buf = NULL;
static int g_preview_w = 0, g_preview_h = 0;
static int64_t g_last_preview_write_ms = 0;

static int64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Same FNV-1a-64 algorithm as mutaclsym's gl_mirror.c/wraith_gl.c -- so a
 * checksum computed here is directly comparable to one computed by
 * screen_rec_gui.c after it loads the same bytes. This is the artifact that
 * lets correctness be confirmed by reading two text files, not by staring
 * at the window. */
static unsigned long long checksum_buffer(const unsigned char *buf, size_t len)
{
    unsigned long long hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) { hash ^= buf[i]; hash *= 1099511628211ULL; }
    return hash;
}

static int g_preview_frame_seq = 0;

static void write_frame_receipt(int w, int h, unsigned long long checksum)
{
    FILE *f = fopen(frame_receipt_path, "w");
    if (!f) return;
    fprintf(f, "receipt_type=capture_frame\n");
    fprintf(f, "generated_by=screen_rec\n");
    fprintf(f, "frame_w=%d\n", w);
    fprintf(f, "frame_h=%d\n", h);
    fprintf(f, "frame_seq=%d\n", g_preview_frame_seq);
    fprintf(f, "frame_bytes=%d\n", w * h * 4);
    fprintf(f, "frame_checksum_fnv1a64=0x%016llX\n", checksum);
    fclose(f);
}

static void update_preview(const uint8_t *data, int stride, int src_w, int src_h, enum AVPixelFormat src_fmt)
{
    int64_t t = now_ms();
    if (t - g_last_preview_write_ms < PREVIEW_MIN_INTERVAL_MS) return;
    g_last_preview_write_ms = t;

    int pw = src_w, ph = src_h;
    if (pw > PREVIEW_MAX_DIM || ph > PREVIEW_MAX_DIM) {
        double scale = (double)PREVIEW_MAX_DIM / (pw > ph ? pw : ph);
        pw = (int)(pw * scale) & ~1;
        ph = (int)(ph * scale) & ~1;
        if (pw < 2) pw = 2;
        if (ph < 2) ph = 2;
    }

    if (!g_preview_sws || pw != g_preview_w || ph != g_preview_h) {
        if (g_preview_sws) sws_freeContext(g_preview_sws);
        free(g_preview_buf);
        g_preview_sws = sws_getContext(src_w, src_h, src_fmt, pw, ph, AV_PIX_FMT_RGBA,
                                        SWS_BILINEAR, NULL, NULL, NULL);
        g_preview_buf = malloc((size_t)pw * ph * 4);
        g_preview_w = pw;
        g_preview_h = ph;
    }
    if (!g_preview_sws || !g_preview_buf) return;

    const uint8_t *src_slices[1] = { data };
    int src_stride[1] = { stride };
    uint8_t *dst_slices[1] = { g_preview_buf };
    int dst_stride[1] = { pw * 4 };

    sws_scale(g_preview_sws, src_slices, src_stride, 0, src_h, dst_slices, dst_stride);

    size_t nbytes = (size_t)pw * ph * 4;
    FILE *f = fopen(frame_raw_path, "wb");
    if (f) {
        fwrite(g_preview_buf, 1, nbytes, f);
        fclose(f);
    }

    g_preview_frame_seq++;
    write_frame_receipt(pw, ph, checksum_buffer(g_preview_buf, nbytes));
}

/* ---------- control file polling ---------- */

static int g_cap_width = 0, g_cap_height = 0, g_cap_spa_format = 0;

static void poll_control_command(void)
{
    FILE *f = fopen(control_command_path, "r");
    if (!f) return;
    char cmd[64] = {0};
    if (fgets(cmd, sizeof(cmd), f)) {
        char *nl = strchr(cmd, '\n');
        if (nl) *nl = '\0';
    }
    fclose(f);
    remove(control_command_path);

    if (strcmp(cmd, "start") == 0 && !g_recording) {
        if (g_cap_width > 0) start_recording(g_cap_width, g_cap_height, g_cap_spa_format, 30);
    } else if (strcmp(cmd, "stop") == 0 && g_recording) {
        stop_recording();
    }
}

/* ---------- PipeWire capture ---------- */

static struct pw_main_loop *g_loop = NULL;
static struct pw_stream *g_stream = NULL;
static struct spa_hook g_stream_listener;
static struct spa_video_info_raw g_format;
static int g_format_known = 0;

static void on_state_changed(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    (void)userdata; (void)old;
    fprintf(stderr, "screen_rec: stream state -> %s%s%s\n", pw_stream_state_as_string(state),
            error ? " error: " : "", error ? error : "");
}

static void on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    (void)userdata;
    uint32_t media_type, media_subtype;
    if (param == NULL || id != SPA_PARAM_Format) return;
    if (spa_format_parse(param, &media_type, &media_subtype) < 0) return;
    if (media_type != SPA_MEDIA_TYPE_video || media_subtype != SPA_MEDIA_SUBTYPE_raw) return;
    if (spa_format_video_raw_parse(param, &g_format) < 0) return;

    g_format_known = 1;
    g_cap_width = g_format.size.width;
    g_cap_height = g_format.size.height;
    g_cap_spa_format = g_format.format;
    fprintf(stderr, "screen_rec: negotiated format=%d size=%dx%d\n",
            g_format.format, g_format.size.width, g_format.size.height);
}

static void on_process(void *userdata)
{
    (void)userdata;
    struct pw_buffer *b = pw_stream_dequeue_buffer(g_stream);
    if (!b) return;

    struct spa_buffer *buf = b->buffer;
    if (buf->datas[0].data != NULL && g_format_known) {
        uint8_t *data = buf->datas[0].data;
        int stride = buf->datas[0].chunk->stride;
        if (stride <= 0) stride = g_format.size.width * 4;

        enum AVPixelFormat av_fmt = spa_format_to_av(g_format.format);
        update_preview(data, stride, g_format.size.width, g_format.size.height, av_fmt);

        if (g_recording) {
            static int64_t rec_start_ms = 0;
            if (g_last_pts_ms < 0) rec_start_ms = now_ms();
            encode_frame(data, stride, now_ms() - rec_start_ms);
        }
    }
    pw_stream_queue_buffer(g_stream, b);
}

static void on_quit_signal(void *userdata, int signal_number)
{
    (void)userdata;
    fprintf(stderr, "screen_rec: received signal %d, shutting down\n", signal_number);
    if (g_recording) stop_recording();
    pw_main_loop_quit(g_loop);
}

static void on_control_timer(void *userdata, uint64_t expirations)
{
    (void)userdata; (void)expirations;
    poll_control_command();
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    resolve_root();
    write_pid_file();
    atexit(remove_pid_file);

    fprintf(stderr, "screen_rec: requesting screencast via xdg-desktop-portal (picker dialog incoming)...\n");
    int pw_fd; uint32_t node_id;
    if (portal_request_screencast(&pw_fd, &node_id) != 0) {
        fprintf(stderr, "screen_rec: portal negotiation failed, exiting\n");
        return 1;
    }
    fprintf(stderr, "screen_rec: portal OK, pipewire_fd=%d node_id=%u\n", pw_fd, node_id);

    pw_init(NULL, NULL);
    g_loop = pw_main_loop_new(NULL);
    struct pw_context *context = pw_context_new(pw_main_loop_get_loop(g_loop), NULL, 0);

    int dup_fd = fcntl(pw_fd, F_DUPFD_CLOEXEC, 3);
    if (dup_fd < 0) dup_fd = pw_fd;
    struct pw_core *core = pw_context_connect_fd(context, dup_fd, NULL, 0);
    if (!core) {
        fprintf(stderr, "screen_rec: pw_context_connect_fd failed\n");
        return 1;
    }

    struct pw_loop *pwloop = pw_main_loop_get_loop(g_loop);
    pw_loop_add_signal(pwloop, SIGINT, on_quit_signal, NULL);
    pw_loop_add_signal(pwloop, SIGTERM, on_quit_signal, NULL);

    struct spa_source *ctl_timer = pw_loop_add_timer(pwloop, on_control_timer, NULL);
    struct timespec ctl_value = { 0, 200000000L }; /* 200ms */
    struct timespec ctl_interval = { 0, 200000000L };
    pw_loop_update_timer(pwloop, ctl_timer, &ctl_value, &ctl_interval, false);

    static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .state_changed = on_state_changed,
        .param_changed = on_param_changed,
        .process = on_process,
    };

    g_stream = pw_stream_new(core, "screen-rec-capture",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
                           PW_KEY_MEDIA_ROLE, "Screen", NULL));
    pw_stream_add_listener(g_stream, &g_stream_listener, &stream_events, NULL);

    uint8_t pod_buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));
    const struct spa_pod *params[1];
    struct spa_rectangle def_size = SPA_RECTANGLE(1920, 1080);
    struct spa_rectangle min_size = SPA_RECTANGLE(1, 1);
    struct spa_rectangle max_size = SPA_RECTANGLE(8192, 8192);
    struct spa_fraction def_framerate = SPA_FRACTION(30, 1);
    struct spa_fraction min_framerate = SPA_FRACTION(0, 1);
    struct spa_fraction max_framerate = SPA_FRACTION(30, 1);

    params[0] = spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(5,
                                        SPA_VIDEO_FORMAT_BGRx,
                                        SPA_VIDEO_FORMAT_BGRx,
                                        SPA_VIDEO_FORMAT_RGBx,
                                        SPA_VIDEO_FORMAT_BGRA,
                                        SPA_VIDEO_FORMAT_RGBA),
        SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(&def_size, &min_size, &max_size),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&def_framerate, &min_framerate, &max_framerate));

    if (pw_stream_connect(g_stream, PW_DIRECTION_INPUT, node_id,
            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS, params, 1) < 0) {
        fprintf(stderr, "screen_rec: pw_stream_connect failed\n");
        return 1;
    }

    write_recorder_state();
    fprintf(stderr, "screen_rec: running, watching %s for start/stop commands\n", control_command_path);
    pw_main_loop_run(g_loop);

    if (g_recording) stop_recording();
    pw_stream_destroy(g_stream);
    pw_context_destroy(context);
    pw_main_loop_destroy(g_loop);
    return 0;
}
