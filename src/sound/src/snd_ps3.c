/*
 * PS3 native audio backend using PSL1GHT libaudio.
 *
 * Closely follows the SDL2 PSL1GHT audio driver (SDL_psl1ghtaudio.c).
 * A dedicated audio thread loops: wait for DMA block consumption event,
 * convert Quake's int16 ring buffer to float32, write to the next DMA block.
 */

#include "sys.h"
#include "sound.h"
#include "console.h"
#include "quakedef.h"
#include <audio/audio.h>
#include <sys/event_queue.h>
#include <sys/thread.h>
#include <string.h>
#include <stdlib.h>

static u32               _portNum = -1;
static audioPortConfig   _config;
static sys_event_queue_t _snd_queue;
static sys_ipc_key_t     _snd_queue_key;
static int               _last_filled_buf;
static volatile int      _audio_running = 0;
static sys_ppu_thread_t  _audio_thread_id;
static qboolean          _audio_initialized = false;

/*
 * Audio thread: mirrors SDL2's WaitDevice → GetDeviceBuf → fill loop.
 * Blocks on the event queue, converts one block of int16 → float32, repeats.
 */
static void _audio_thread_func(void *arg) {
    (void)arg;
    int num_blocks = (int)_config.numBlocks;
    int ch = (int)_config.channelCount;
    int samples_per_block = AUDIO_BLOCK_SAMPLES * ch;
    size_t block_bytes = samples_per_block * sizeof(float);
    const float scale = 1.0f / 32768.0f;

    while (_audio_running) {
        /* WaitDevice — block until hardware consumes a DMA block */
        sys_event_t event;
        s32 ret = sysEventQueueReceive(_snd_queue, &event, 20 * 1000);
        if (ret != 0) continue; /* timeout or error, retry */

        /* GetDeviceBuf — advance to next block */
        int filling = (_last_filled_buf + 1) % num_blocks;
        _last_filled_buf = filling;

        float *dst = (float *)(uintptr_t)_config.audioDataStart;
        dst += filling * AUDIO_BLOCK_SAMPLES * ch;

        /* Copy from Quake's int16 ring buffer → DMA float32 block */
        if (shm && shm->buffer) {
            int pos = shm->samplepos;
            /* Ensure buffer writes from game thread are visible before we read */
            __asm__ volatile("lwsync" ::: "memory");
            int16_t *src = (int16_t *)shm->buffer;
            for (int i = 0; i < samples_per_block; i++) {
                int idx = (pos + i) % shm->samples;
                dst[i] = (float)src[idx] * scale;
            }
            shm->samplepos = (pos + samples_per_block) % shm->samples;
        } else {
            /* No audio data yet — write silence */
            memset(dst, 0, block_bytes);
        }
    }

    sysThreadExit(0);
}

qboolean SNDDMA_Init(dma_t *dma) {
    int ret;

    ret = audioInit();
    if (ret != 0) {
        Con_Printf("PS3 audio: audioInit failed (%d)\n", ret);
        return false;
    }

    audioPortParam params;
    memset(&params, 0, sizeof(params));
    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks   = AUDIO_BLOCK_8;
    params.attrib      = 0;
    params.level       = 1;

    ret = audioPortOpen(&params, &_portNum);
    if (ret != 0) {
        Con_Printf("PS3 audio: audioPortOpen failed (%d)\n", ret);
        audioQuit();
        return false;
    }

    ret = audioGetPortConfig(_portNum, &_config);
    if (ret != 0) {
        Con_Printf("PS3 audio: audioGetPortConfig failed (%d)\n", ret);
        audioPortClose(_portNum);
        audioQuit();
        return false;
    }

    ret = audioCreateNotifyEventQueue(&_snd_queue, &_snd_queue_key);
    if (ret != 0) {
        Con_Printf("PS3 audio: audioCreateNotifyEventQueue failed (%d)\n", ret);
        audioPortClose(_portNum);
        audioQuit();
        return false;
    }

    ret = audioSetNotifyEventQueue(_snd_queue_key);
    if (ret != 0) {
        Con_Printf("PS3 audio: audioSetNotifyEventQueue failed (%d)\n", ret);
        audioPortClose(_portNum);
        sysEventQueueDestroy(_snd_queue, 0);
        audioQuit();
        return false;
    }

    sysEventQueueDrain(_snd_queue);

    ret = audioPortStart(_portNum);
    if (ret != 0) {
        Con_Printf("PS3 audio: audioPortStart failed (%d)\n", ret);
        audioRemoveNotifyEventQueue(_snd_queue_key);
        audioPortClose(_portNum);
        sysEventQueueDestroy(_snd_queue, 0);
        audioQuit();
        return false;
    }

    _last_filled_buf = (int)(_config.numBlocks - 1);

    /* Set up Quake's DMA descriptor */
    Q_memset(dma, 0, sizeof(dma_t));
    shm = dma;
    shm->samplebits  = 16;
    shm->signed8     = false;
    shm->speed       = 48000;
    shm->channels    = (i32)_config.channelCount;
    shm->samplepos   = 0;
    shm->submission_chunk = AUDIO_BLOCK_SAMPLES;
    shm->samples     = (i32)(AUDIO_BLOCK_SAMPLES * _config.numBlocks * 8);
    int buffersize   = shm->samples * (shm->samplebits / 8);
    shm->buffer      = (byte*) Q_calloc(1, buffersize);
    if (!shm->buffer) {
        audioPortStop(_portNum);
        audioRemoveNotifyEventQueue(_snd_queue_key);
        audioPortClose(_portNum);
        sysEventQueueDestroy(_snd_queue, 0);
        audioQuit();
        shm = NULL;
        return false;
    }

    /* Spawn audio thread — 64 KB stack, priority 1001 (just below game thread) */
    _audio_running = 1;
    ret = sysThreadCreate(&_audio_thread_id, _audio_thread_func, NULL,
                          1001, 64 * 1024, THREAD_JOINABLE, "ps3_audio");
    if (ret != 0) {
        Con_Printf("PS3 audio: sysThreadCreate failed (%d)\n", ret);
        _audio_running = 0;
        Q_free(shm->buffer);
        shm->buffer = NULL;
        shm = NULL;
        audioPortStop(_portNum);
        audioRemoveNotifyEventQueue(_snd_queue_key);
        audioPortClose(_portNum);
        sysEventQueueDestroy(_snd_queue, 0);
        audioQuit();
        return false;
    }

    _audio_initialized = true;
    Con_Printf("PS3 audio: %d Hz, float32, %dch, %d blocks, %d samples/block\n",
               48000, (int)_config.channelCount, (int)_config.numBlocks, AUDIO_BLOCK_SAMPLES);
    return true;
}

i32 SNDDMA_GetDMAPos(void) {
    return shm ? shm->samplepos : 0;
}

void SNDDMA_LockBuffer(void) {
}

void SNDDMA_Submit(void) {
    /* Audio thread handles all DMA submission — nothing to do here. */
}

void SNDDMA_Shutdown(void) {
    if (!shm) return;
    if (_audio_initialized) {
        /* Signal the audio thread to stop and wait for it */
        _audio_running = 0;
        u64 exit_code = 0;
        sysThreadJoin(_audio_thread_id, &exit_code);

        audioPortStop(_portNum);
        audioRemoveNotifyEventQueue(_snd_queue_key);
        audioPortClose(_portNum);
        sysEventQueueDestroy(_snd_queue, 0);
        audioQuit();
        _audio_initialized = false;
    }
    if (shm->buffer) {
        Q_free(shm->buffer);
    }
    shm->buffer = NULL;
    shm = NULL;
}

void SNDDMA_BlockSound(void) {
}

void SNDDMA_UnblockSound(void) {
    if (_audio_initialized) {
        sysEventQueueDrain(_snd_queue);
    }
}
