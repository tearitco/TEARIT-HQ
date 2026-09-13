/*
 * nb_video_play.c - V3 real-time video op for the network browser.
 * (NETWORK-BROWSER-VIDEO-V3-DESIGN.md §3)
 *
 * Decodes a media URL (file://, http(s)://, or youtube) with libav in
 * real time and publishes a live raw-RGBA canvas surface for the
 * shared renderer's generic <canvas> element:
 *   <sess>/surface.raw        RGBA pixels (native stride = w*4)
 *   <sess>/surface.receipt.txt  frame_w= / frame_h= (sibling, per
 *                         kh_draw_canvas()'s receipt convention)
 *
 * Audio (when the stream has it) is decoded and written to the ALSA
 * default sink with snd_pcm_writei() in blocking mode; the audio clock
 * paces the video. With no audio/ALSA, pacing falls back to the video
 * PTS against a monotonic clock. Same control contract as the V2
 * player: the manger writes <sess>/video.control, we obey it.
 *
 * Usage:
 *   nb_video_play <url> <sess_dir>                 (resident decode loop)
 *   nb_video_play --pause|--resume|--stop <sess>   (control poke)
 */
#define _POSIX_C_SOURCE 200809L
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <alsa/asoundlib.h>
#include <alloca.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define CANVAS_W 640
#define CANVAS_H 360
#define CTRL_SUFFIX "/video.control"
#define PID_SUFFIX "/video.pid"
#define STATE_SUFFIX "/video.state"
#define RAW_SUFFIX "/surface.raw"
#define RECEIPT_SUFFIX "/surface.receipt.txt"

static volatile sig_atomic_t g_stop = 0;
static void on_signal(int s) { (void)s; g_stop = 1; }

static void write_state(const char *sess, const char *st) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s" STATE_SUFFIX, sess);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s\n", st); fclose(f); }
}

static void write_pid(const char *sess) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s" PID_SUFFIX, sess);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%d\n", (int)getpid()); fclose(f); }
}

static int poke_control(const char *cmd, const char *sess) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s" CTRL_SUFFIX, sess);
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "%s\n", cmd);
    fclose(f);
    return 0;
}

/* Returns a pointer into buf for the current control word, or NULL when
 * the control file is absent (which the loop treats as "keep playing"). */
static const char *poll_control(const char *sess, char *buf, size_t n) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s" CTRL_SUFFIX, sess);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    if (fgets(buf, (int)n, f)) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
        fclose(f);
        return buf[0] ? buf : NULL;
    }
    fclose(f);
    return NULL;
}

/* Atomic-ish surface publish: tmp + rename so kh_draw_canvas() always
 * reads a full frame; receipt rewritten each frame too (also tmp+rename). */
static void publish_frame(const char *sess, const unsigned char *rgba, int w, int h) {
    char raw[PATH_MAX], tmp[PATH_MAX], rc[PATH_MAX];
    snprintf(raw, sizeof(raw), "%s" RAW_SUFFIX, sess);
    snprintf(tmp, sizeof(tmp), "%s" RAW_SUFFIX ".tmp", sess);
    snprintf(rc, sizeof(rc), "%s" RECEIPT_SUFFIX, sess);
    if (rgba && w > 0 && h > 0) {
        FILE *f = fopen(tmp, "wb");
        if (f) {
            fwrite(rgba, 1, (size_t)w * h * 4, f);
            fclose(f);
            rename(tmp, raw);
        }
    }
    snprintf(tmp, sizeof(tmp), "%s" RECEIPT_SUFFIX ".tmp", sess);
    FILE *f = fopen(tmp, "w");
    if (f) {
        fprintf(f, "frame_w=%d\nframe_h=%d\n", w, h);
        fclose(f);
        rename(tmp, rc);
    }
}

static const char *resolve_url(char *buf, size_t n, const char *url) {
    /* YouTube → yt-dlp --get-url (non-DRM only). The resolved direct
     * stream URL feeds the same libav pipeline as a plain http(s) one. */
    if (strstr(url, "youtu.be") || strstr(url, "youtube.com") ||
        strncmp(url, "yt:", 3) == 0) {
        char cmd[PATH_MAX * 2];
        /* player_client=android: the default android_vr client mints
         * googlevideo URLs the CDN 403s even for yt-dlp itself (n-sig / pot
         * stamp mismatch); 'android' mints URLs that stream fine with a
         * plain browser UA. Verified 2026-09-12. */
        snprintf(cmd, sizeof(cmd),
                 "~/.local/bin/yt-dlp --get-url --no-playlist "
                 "--format 'best[ext=mp4]/best' "
                 "--extractor-args 'youtube:player_client=android' '%s' "
                 "2>/dev/null", url);
        FILE *p = popen(cmd, "r");
        if (p) {
            if (fgets(buf, (int)n, p)) {
                size_t l = strlen(buf);
                while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
            }
            int rc = pclose(p);
            if (buf[0] && rc == 0) return buf;
        }
    }
    snprintf(buf, n, "%s", url);
    return buf;
}

static snd_pcm_t *open_alsa(int rate, int channels) {
    snd_pcm_t *p = NULL;
    if (snd_pcm_open(&p, "default", SND_PCM_STREAM_PLAYBACK, 0) != 0) return NULL;
    snd_pcm_hw_params_t *hp;
    snd_pcm_hw_params_alloca(&hp);
    if (snd_pcm_hw_params_any(p, hp) != 0 ||
        snd_pcm_hw_params_set_access(p, hp, SND_PCM_ACCESS_RW_INTERLEAVED) != 0 ||
        snd_pcm_hw_params_set_format(p, hp, SND_PCM_FORMAT_S16_LE) != 0 ||
        snd_pcm_hw_params_set_channels(p, hp, (unsigned)channels) != 0) {
        snd_pcm_close(p);
        return NULL;
    }
    unsigned int want = (unsigned)rate;
    if (snd_pcm_hw_params_set_rate_near(p, hp, &want, 0) != 0) { snd_pcm_close(p); return NULL; }
    unsigned int period = want / 10;
    snd_pcm_uframes_t pf = (snd_pcm_uframes_t)period;
    if (snd_pcm_hw_params_set_period_size_near(p, hp, &pf, NULL) != 0) { snd_pcm_close(p); return NULL; }
    if (snd_pcm_hw_params(p, hp) != 0) { snd_pcm_close(p); return NULL; }
    return p;
}

/* Feed one decoded audio frame to ALSA (resampled to stereo s16 by the
 * swr context created in main). Returns 0 on success, nonzero when the
 * sink is gone (caller drops audio and becomes video-only). */
static int alsa_play_frame(snd_pcm_t *alsa, SwrContext *swr, AVFrame *afr) {
    uint8_t *out = NULL;
    int out_n = swr_get_out_samples(swr, afr->nb_samples);
    if (out_n <= 0) return 0;
    out = malloc((size_t)out_n * 2 * 2);
    if (!out) return 1;
    uint8_t *dstp = out;
    int got = swr_convert(swr, &dstp, out_n,
                          (const uint8_t **)afr->extended_data, afr->nb_samples);
    if (got > 0) {
        snd_pcm_sframes_t w = snd_pcm_writei(alsa, out, (snd_pcm_sframes_t)got);
        if (w < 0) {
            snd_pcm_recover(alsa, (int)w, 1);
            w = snd_pcm_writei(alsa, out, (snd_pcm_sframes_t)got);
        }
        if (w < 0) { free(out); return 1; }
    }
    free(out);
    return 0;
}

static double mono_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void msleep(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

/* Scale one decoded frame to RGBA and publish it. Must be called from
 * inside the receive loop: avcodec_receive_frame() smashes `fr` on the
 * trailing EAGAIN, so the frame must be consumed while it is valid. */
static void emit_frame(const char *sess, AVFrame *fr, AVCodecContext *vctx,
                       AVStream *vs, struct SwsContext **sws, int *sws_fmt,
                       double *play_start, int have_audio_clock) {
    double vts = fr->pts != AV_NOPTS_VALUE
                     ? (double)fr->pts * av_q2d(vs->time_base) : -1;
    if (vts >= 0 && !have_audio_clock) {
        double target = *play_start + vts;
        double now = mono_now();
        if (target > now + 0.004) {
            int ms = (int)((target - now) * 1000);
            if (ms > 0 && ms < 500) msleep(ms);
        }
    }
    if (fr->format != *sws_fmt) {
        if (*sws) sws_freeContext(*sws);
        *sws = sws_getContext(
            vctx->width, vctx->height, fr->format,
            CANVAS_W, CANVAS_H, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
        *sws_fmt = fr->format;
    }
    if (*sws) {
        unsigned char rowbuf[CANVAS_W * CANVAS_H * 4];
        uint8_t *dst[1] = { rowbuf };
        int dstsz[1] = { CANVAS_W * 4 };
        uint8_t *const src[] = { fr->data[0], fr->data[1], fr->data[2], fr->data[3] };
        int srcst[] = { fr->linesize[0], fr->linesize[1], fr->linesize[2], fr->linesize[3] };
        sws_scale(*sws, (const uint8_t *const *)src, srcst, 0, vctx->height,
                  dst, dstsz);
        publish_frame(sess, rowbuf, CANVAS_W, CANVAS_H);
    }
}

int main(int argc, char **argv) {
    if (argc >= 3 && strncmp(argv[1], "--", 2) == 0)
        return poke_control(argv[1] + 2, argv[2]);
    if (argc < 3) {
        fprintf(stderr, "usage: nb_video_play <url|file> <sess_dir>\n");
        return 1;
    }
    const char *sess = argv[2];
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    write_state(sess, "starting");
    write_pid(sess);

    char url[PATH_MAX * 2];
    resolve_url(url, sizeof(url), argv[1]);

    avformat_network_init();
    AVFormatContext *fmt = NULL;
    /* YouTube's googlevideo CDN 403s the default Lavf user-agent; yt-dlp
     * emits sig-verified URLs that still check UA + referer origin. Feed a
     * real browser UA and a youtube referer so avio/http passes the check. */
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "http_user_agent",
                "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36", 0);
    av_dict_set(&opts, "http_referrer", "https://www.youtube.com/", 0);
    av_dict_set(&opts, "seekable", "1", 0);
    if (avformat_open_input(&fmt, url, NULL, &opts) != 0) {
        av_dict_free(&opts);
        write_state(sess, "stopped");
        return 1;
    }
    av_dict_free(&opts);
    if (avformat_find_stream_info(fmt, NULL) < 0) {
        avformat_close_input(&fmt);
        write_state(sess, "stopped");
        return 1;
    }
    int vstream = -1, astream = -1;
    for (unsigned i = 0; i < fmt->nb_streams; i++) {
        int t = fmt->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_VIDEO && vstream < 0) vstream = (int)i;
        if (t == AVMEDIA_TYPE_AUDIO && astream < 0) astream = (int)i;
    }
    if (vstream < 0) {
        avformat_close_input(&fmt);
        write_state(sess, "stopped");
        return 1;
    }

    /* ---- video decoder + scaler ---- */
    AVStream *vs = fmt->streams[vstream];
    const AVCodec *vc = avcodec_find_decoder(vs->codecpar->codec_id);
    AVCodecContext *vctx = vc ? avcodec_alloc_context3(vc) : NULL;
    if (!vctx || avcodec_parameters_to_context(vctx, vs->codecpar) < 0 ||
        avcodec_open2(vctx, vc, NULL) < 0) {
        if (vctx) avcodec_free_context(&vctx);
        avformat_close_input(&fmt);
        write_state(sess, "stopped");
        return 1;
    }
    struct SwsContext *sws = NULL; /* created lazily from fr->format (can be NONE pre-first-frame) */
    int sws_fmt = -1;

    /* ---- audio decoder (separate ctx) ---- */
    AVCodecContext *actx = NULL;
    snd_pcm_t *alsa = NULL;
    SwrContext *swr = NULL;
    if (astream >= 0) {
        AVStream *as = fmt->streams[astream];
        const AVCodec *ac = avcodec_find_decoder(as->codecpar->codec_id);
        actx = ac ? avcodec_alloc_context3(ac) : NULL;
        if (actx && avcodec_parameters_to_context(actx, as->codecpar) == 0 &&
            avcodec_open2(actx, ac, NULL) == 0) {
            int out_rate = actx->sample_rate > 0 ? actx->sample_rate : 44100;
            int out_ch = actx->channel_layout ? av_get_channel_layout_nb_channels(actx->channel_layout)
                                              : (actx->channels > 0 ? actx->channels : 2);
            alsa = open_alsa(out_rate, out_ch);
            swr = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_rate,
                                     actx->channel_layout ? actx->channel_layout : AV_CH_LAYOUT_STEREO,
                                     actx->sample_fmt, actx->sample_rate, 0, NULL);
            if (swr && swr_init(swr) < 0) { swr_free(&swr); swr = NULL; }
        }
    }

    /* ---- main loop ---- */
    AVFrame *fr = av_frame_alloc();
    AVFrame *afr = av_frame_alloc();
    AVPacket *pkt = av_packet_alloc();
    int running = 1, eof = 0, got_v = 0, got_a = 0;
    double play_start = mono_now();
    int have_audio_clock = (alsa != NULL && swr != NULL);
    write_state(sess, "playing");
    publish_frame(sess, NULL, 0, 0); /* receipt exists even before frame 0 */

    while (running && !g_stop) {
        /* ---- control: pause (keep last frame, wait) / stop ---- */
        {
            char ctl[32], ctl2[32];
            const char *c = poll_control(sess, ctl, sizeof(ctl));
            if (c && strcmp(c, "pause") == 0) {
                write_state(sess, "paused");
                if (alsa) snd_pcm_pause(alsa, 1);
                int resumed = 0;
                while (running && !g_stop) {
                    msleep(50);
                    const char *c2 = poll_control(sess, ctl2, sizeof(ctl2));
                    if (c2 && strcmp(c2, "resume") == 0) { resumed = 1; break; }
                    if (c2 && strcmp(c2, "stop") == 0) { running = 0; break; }
                }
                if (alsa) snd_pcm_pause(alsa, 0);
                if (!running) break;
                if (resumed) {
                    play_start = mono_now();

                    write_state(sess, "playing");
                }
                continue;
            }
            if (c && strcmp(c, "stop") == 0) { running = 0; break; }
        }

        got_v = 0; got_a = 0;
        if (!eof) {
            int r = av_read_frame(fmt, pkt);
            if (r >= 0) {
                if (pkt->stream_index == vstream) {
                    if (avcodec_send_packet(vctx, pkt) == 0) {
                        while (avcodec_receive_frame(vctx, fr) == 0) {
                            got_v = 1;
                            emit_frame(sess, fr, vctx, vs, &sws, &sws_fmt,
                                       &play_start, have_audio_clock);
                        }
                    }
                } else if (pkt->stream_index == astream && actx) {
                    if (avcodec_send_packet(actx, pkt) == 0) {
                        while (avcodec_receive_frame(actx, afr) == 0) got_a = 1;
                    }
                }
                av_packet_unref(pkt);
            } else {
                eof = 1;
            }
        } else {
            if (avcodec_send_packet(vctx, NULL) == 0)
                while (avcodec_receive_frame(vctx, fr) == 0) {
                    got_v = 1;
                    emit_frame(sess, fr, vctx, vs, &sws, &sws_fmt,
                               &play_start, have_audio_clock);
                }
            if (actx && avcodec_send_packet(actx, NULL) == 0)
                while (avcodec_receive_frame(actx, afr) == 0) got_a = 1;
        }

        /* audio first: its blocking write is the real-time clock */
        if (got_a && alsa && swr) {
            if (alsa_play_frame(alsa, swr, afr) != 0) {
                /* sink gone: go video-only pacing on next frames */
                if (alsa) { snd_pcm_close(alsa); alsa = NULL; }
                have_audio_clock = 0;
            }
            av_frame_unref(afr);
        }

        if (eof && !got_v && !got_a) { running = 0; break; }
        /* busy-loop guard: without audio, a fast stream still paces on
         * pts above; cap loop rate when a stream has no usable pts */
        msleep(2);
        /* a pre-rolled (synced) file may decode instantly: throttle so
         * we never > ~300fps even for a silent 0-pts stream */
    }

    if (alsa) { snd_pcm_drain(alsa); snd_pcm_close(alsa); }
    if (swr) swr_free(&swr);
    if (actx) avcodec_free_context(&actx);
    av_frame_free(&fr);
    av_frame_free(&afr);
    av_packet_free(&pkt);
    if (sws) sws_freeContext(sws);
    avcodec_free_context(&vctx);
    avformat_close_input(&fmt);
    write_state(sess, "stopped");
    unlink(sess);
    return 0;
}