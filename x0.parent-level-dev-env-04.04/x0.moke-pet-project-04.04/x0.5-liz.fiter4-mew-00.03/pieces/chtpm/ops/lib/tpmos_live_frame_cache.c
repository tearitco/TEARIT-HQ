#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define TPMOS_LIVE_FRAME_CACHE_SHM_NAME "/tpmos_live_frame_cache_v1"
#define TPMOS_LIVE_FRAME_CACHE_MAX_KEY 256
#define TPMOS_LIVE_FRAME_CACHE_MAX_BYTES (8 * 1024 * 1024)

typedef struct {
    pthread_mutex_t mutex;
    int initialized;
    int active;
    int width;
    int height;
    int channels;
    size_t byte_count;
    unsigned long generation;
    unsigned long frame_epoch_ms;
    char key[TPMOS_LIVE_FRAME_CACHE_MAX_KEY];
    unsigned char rgba[TPMOS_LIVE_FRAME_CACHE_MAX_BYTES];
} TpmosLiveFrameCache;

static int tpmos_live_frame_cache_attach(TpmosLiveFrameCache **out_cache) {
    int shm_fd;
    TpmosLiveFrameCache *cache;
    int created = 0;
    pthread_mutexattr_t attr;
    if (!out_cache) return -1;
    *out_cache = NULL;

    shm_fd = shm_open(TPMOS_LIVE_FRAME_CACHE_SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd >= 0) {
        created = 1;
        if (ftruncate(shm_fd, sizeof(TpmosLiveFrameCache)) != 0) {
            close(shm_fd);
            return -1;
        }
    } else {
        shm_fd = shm_open(TPMOS_LIVE_FRAME_CACHE_SHM_NAME, O_RDWR, 0666);
        if (shm_fd < 0) return -1;
    }

    cache = (TpmosLiveFrameCache *)mmap(NULL, sizeof(TpmosLiveFrameCache), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (cache == MAP_FAILED) return -1;

    if (created || !cache->initialized) {
        memset(cache, 0, sizeof(*cache));
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&cache->mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        cache->initialized = 1;
    }

    *out_cache = cache;
    return 0;
}

static void tpmos_live_frame_cache_detach(TpmosLiveFrameCache *cache) {
    if (cache) munmap(cache, sizeof(TpmosLiveFrameCache));
}

static unsigned long tpmos_live_frame_cache_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

static int tpmos_live_frame_cache_write_rgba(const char *key, const unsigned char *rgba, size_t byte_count, int width, int height, int channels) {
    TpmosLiveFrameCache *cache = NULL;
    if (!key || !rgba || byte_count == 0) return -1;
    if (byte_count > TPMOS_LIVE_FRAME_CACHE_MAX_BYTES) return -1;
    if (tpmos_live_frame_cache_attach(&cache) != 0) return -1;

    pthread_mutex_lock(&cache->mutex);
    cache->active = 1;
    cache->width = width;
    cache->height = height;
    cache->channels = channels;
    cache->byte_count = byte_count;
    cache->generation++;
    cache->frame_epoch_ms = tpmos_live_frame_cache_now_ms();
    snprintf(cache->key, sizeof(cache->key), "%s", key);
    memcpy(cache->rgba, rgba, byte_count);
    pthread_mutex_unlock(&cache->mutex);

    tpmos_live_frame_cache_detach(cache);
    return 0;
}

static int tpmos_live_frame_cache_clear(const char *key) {
    TpmosLiveFrameCache *cache = NULL;
    if (tpmos_live_frame_cache_attach(&cache) != 0) return -1;
    pthread_mutex_lock(&cache->mutex);
    if (!key || strcmp(cache->key, key) == 0) {
        cache->active = 0;
        cache->width = 0;
        cache->height = 0;
        cache->channels = 0;
        cache->byte_count = 0;
        cache->frame_epoch_ms = 0;
        if (key) snprintf(cache->key, sizeof(cache->key), "%s", key);
    }
    pthread_mutex_unlock(&cache->mutex);
    tpmos_live_frame_cache_detach(cache);
    return 0;
}

static int tpmos_live_frame_cache_read_rgba(const char *key, unsigned char **out_rgba, size_t *out_byte_count, int *out_width, int *out_height, int *out_channels, unsigned long *out_generation, unsigned long *out_epoch_ms) {
    TpmosLiveFrameCache *cache = NULL;
    unsigned char *copy = NULL;
    if (!key || !out_rgba || !out_byte_count) return -1;
    *out_rgba = NULL;
    *out_byte_count = 0;
    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
    if (out_channels) *out_channels = 0;
    if (out_generation) *out_generation = 0;
    if (out_epoch_ms) *out_epoch_ms = 0;
    if (tpmos_live_frame_cache_attach(&cache) != 0) return -1;

    pthread_mutex_lock(&cache->mutex);
    if (!cache->active || strcmp(cache->key, key) != 0 || cache->byte_count == 0) {
        pthread_mutex_unlock(&cache->mutex);
        tpmos_live_frame_cache_detach(cache);
        return -1;
    }

    copy = (unsigned char *)malloc(cache->byte_count);
    if (!copy) {
        pthread_mutex_unlock(&cache->mutex);
        tpmos_live_frame_cache_detach(cache);
        return -1;
    }
    memcpy(copy, cache->rgba, cache->byte_count);
    *out_rgba = copy;
    *out_byte_count = cache->byte_count;
    if (out_width) *out_width = cache->width;
    if (out_height) *out_height = cache->height;
    if (out_channels) *out_channels = cache->channels;
    if (out_generation) *out_generation = cache->generation;
    if (out_epoch_ms) *out_epoch_ms = cache->frame_epoch_ms;
    pthread_mutex_unlock(&cache->mutex);
    tpmos_live_frame_cache_detach(cache);
    return 0;
}
