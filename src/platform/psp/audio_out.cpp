#include "platform/psp/audio_out.h"
#include "runtime/log.h"

#include <pspaudio.h>
#include <pspkernel.h>

#include <cstring>

namespace rs::audio {

namespace {

constexpr u32 RING_FRAMES = 8192;            /* power of two */
constexpr u32 RING_MASK   = RING_FRAMES - 1;
constexpr int BLOCK_FRAMES = 1024;           /* sceAudio block size */

s16 s_ring[RING_FRAMES * 2];
volatile u32 s_write = 0;   /* in frames, free-running */
volatile u32 s_read  = 0;
volatile bool s_running = false;

int s_channel = -1;

/* Resampler state. */
u32 s_srcRate = OUTPUT_RATE;
u32 s_step = 0x10000;       /* 16.16 fixed source-frames per output frame */
u32 s_frac = 0;
s16 s_lastL = 0, s_lastR = 0;

alignas(64) s16 s_block[BLOCK_FRAMES * 2];

int audioThread(SceSize, void*) {
    while (s_running) {
        const u32 avail = s_write - s_read;
        const u32 take = avail < u32(BLOCK_FRAMES) ? avail : u32(BLOCK_FRAMES);
        for (u32 i = 0; i < take; i++) {
            const u32 idx = (s_read + i) & RING_MASK;
            s_block[i * 2 + 0] = s_ring[idx * 2 + 0];
            s_block[i * 2 + 1] = s_ring[idx * 2 + 1];
        }
        /* Underrun pads with silence rather than stalling the channel. */
        for (u32 i = take; i < u32(BLOCK_FRAMES); i++) {
            s_block[i * 2 + 0] = 0;
            s_block[i * 2 + 1] = 0;
        }
        s_read = s_read + take;
        sceAudioOutputBlocking(s_channel, PSP_AUDIO_VOLUME_MAX, s_block);
    }
    return 0;
}

}  // namespace

bool init() {
    s_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BLOCK_FRAMES,
                                  PSP_AUDIO_FORMAT_STEREO);
    if (s_channel < 0) {
        RS_LOGE("audio: channel reserve failed (%08x)", unsigned(s_channel));
        return false;
    }
    s_running = true;
    const SceUID tid = sceKernelCreateThread("rs_audio", audioThread, 0x10,
                                             16 * 1024, PSP_THREAD_ATTR_USER,
                                             nullptr);
    if (tid < 0 || sceKernelStartThread(tid, 0, nullptr) < 0) {
        RS_LOGE("audio: thread start failed");
        s_running = false;
        sceAudioChRelease(s_channel);
        s_channel = -1;
        return false;
    }
    RS_LOGI("audio: channel %d at %u Hz", s_channel, unsigned(OUTPUT_RATE));
    return true;
}

void shutdown() {
    s_running = false;
    if (s_channel >= 0) {
        sceKernelDelayThread(80 * 1000);   /* let the thread drain out */
        sceAudioChRelease(s_channel);
        s_channel = -1;
    }
}

void setSourceRate(u32 srcRate) {
    if (srcRate == 0) srcRate = OUTPUT_RATE;
    s_srcRate = srcRate;
    s_step = u32((u64(srcRate) << 16) / OUTPUT_RATE);
    s_frac = 0;
}

void push(const s16* stereo, u32 frames) {
    if (!frames) return;

    if (s_srcRate == OUTPUT_RATE) {
        for (u32 i = 0; i < frames; i++) {
            if (s_write - s_read >= RING_FRAMES) break;   /* full: drop */
            const u32 idx = s_write & RING_MASK;
            s_ring[idx * 2 + 0] = stereo[i * 2 + 0];
            s_ring[idx * 2 + 1] = stereo[i * 2 + 1];
            s_write = s_write + 1;
        }
        return;
    }

    /* Linear resample srcRate → 44100. `s_frac` carries across pushes. */
    u32 pos = s_frac;
    while (true) {
        const u32 srcIdx = pos >> 16;
        if (srcIdx >= frames) break;
        const u32 t = pos & 0xFFFF;
        const s16 l0 = srcIdx == 0 ? s_lastL : stereo[(srcIdx - 1) * 2 + 0];
        const s16 r0 = srcIdx == 0 ? s_lastR : stereo[(srcIdx - 1) * 2 + 1];
        const s16 l1 = stereo[srcIdx * 2 + 0];
        const s16 r1 = stereo[srcIdx * 2 + 1];
        if (s_write - s_read < RING_FRAMES) {
            const u32 idx = s_write & RING_MASK;
            s_ring[idx * 2 + 0] = s16(l0 + ((s32(l1) - l0) * s32(t) >> 16));
            s_ring[idx * 2 + 1] = s16(r0 + ((s32(r1) - r0) * s32(t) >> 16));
            s_write = s_write + 1;
        }
        pos += s_step;
    }
    s_frac = pos - (frames << 16);
    s_lastL = stereo[(frames - 1) * 2 + 0];
    s_lastR = stereo[(frames - 1) * 2 + 1];
}

void clear() {
    s_read = s_write;
    s_frac = 0;
}

u32 buffered() { return s_write - s_read; }

}  // namespace rs::audio
