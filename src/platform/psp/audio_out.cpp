#include "platform/psp/audio_out.h"
#include "runtime/log.h"

#include <pspaudio.h>
#include <pspkernel.h>

#include <cstring>
#include <atomic>
#include <cmath>

namespace rs::audio {

namespace {

constexpr u32 RING_FRAMES = 8192;            /* power of two */
constexpr u32 RING_MASK   = RING_FRAMES - 1;
constexpr int BLOCK_FRAMES = int(OUTPUT_BLOCK_FRAMES);

s16 s_ring[RING_FRAMES * 2];
std::atomic<u32> s_write{0}; /* in frames, free-running (producer writes) */
std::atomic<u32> s_read{0};  /* audio thread is the ONLY writer of s_read */
std::atomic<bool> s_running{false};
std::atomic<bool> s_flush{false}; /* producer asks consumer to drop the ring */
std::atomic<bool> s_paused{true};
std::atomic<u32> s_underruns{0};
std::atomic<u32> s_dropped{0};
std::atomic<int> s_uiCommand{-1};

int s_channel = -1;
SceUID s_thread = -1;

/* Resampler state. */
u32 s_srcRate = OUTPUT_RATE;
u32 s_step = 0x10000;       /* 16.16 fixed source-frames per output frame */
u32 s_frac = 0;
s16 s_lastL = 0, s_lastR = 0;

alignas(64) s16 s_block[BLOCK_FRAMES * 2];
constexpr int UI_WAVE_SIZE = 64;
s16 s_uiWave[UI_WAVE_SIZE];
u32 s_uiPhase = 0;
u32 s_uiStep = 0;
u32 s_uiFrames = 0;
u32 s_uiTotal = 0;

int audioThread(SceSize, void*) {
    while (s_running.load(std::memory_order_acquire)) {
        /* Honor a flush here so s_read is only ever written by this thread;
         * clear() on the producer must not touch s_read or it can race this
         * loop's read-modify-write and drive s_read past s_write. */
        if (s_flush.exchange(false, std::memory_order_acq_rel)) {
            s_read.store(s_write.load(std::memory_order_acquire),
                         std::memory_order_release);
        }
        const bool paused = s_paused.load(std::memory_order_acquire);
        const u32 read = s_read.load(std::memory_order_relaxed);
        const u32 write = s_write.load(std::memory_order_acquire);
        const u32 avail = paused ? 0 : write - read;
        const u32 take = avail < u32(BLOCK_FRAMES) ? avail : u32(BLOCK_FRAMES);
        if (!paused && take < u32(BLOCK_FRAMES))
            s_underruns.fetch_add(1, std::memory_order_relaxed);
        for (u32 i = 0; i < take; i++) {
            const u32 idx = (read + i) & RING_MASK;
            s_block[i * 2 + 0] = s_ring[idx * 2 + 0];
            s_block[i * 2 + 1] = s_ring[idx * 2 + 1];
        }
        /* Underrun pads with silence rather than stalling the channel. */
        for (u32 i = take; i < u32(BLOCK_FRAMES); i++) {
            s_block[i * 2 + 0] = 0;
            s_block[i * 2 + 1] = 0;
        }
        const int command = s_uiCommand.exchange(-1, std::memory_order_acq_rel);
        if (command >= 0) {
            const u32 hz = command == int(UiSound::Move) ? 720u
                         : command == int(UiSound::Confirm) ? 1040u : 480u;
            s_uiTotal = command == int(UiSound::Move) ? 1050u : 1650u;
            s_uiFrames = s_uiTotal;
            s_uiPhase = 0;
            s_uiStep = u32((u64(hz) * UI_WAVE_SIZE << 16) / OUTPUT_RATE);
        }
        for (u32 i = 0; i < u32(BLOCK_FRAMES) && s_uiFrames; i++) {
            const u32 elapsed = s_uiTotal - s_uiFrames;
            const u32 attack = elapsed < 80u ? elapsed : 80u;
            const u32 decay = (s_uiFrames * 256u) / s_uiTotal;
            const s32 gain = s32((attack * decay) / 80u);
            const s32 sample =
                (s32(s_uiWave[(s_uiPhase >> 16) & (UI_WAVE_SIZE - 1)]) *
                 gain) >> 9;
            const s32 left = rsClamp<s32>(s32(s_block[i * 2 + 0]) + sample,
                                          -32768, 32767);
            const s32 right = rsClamp<s32>(s32(s_block[i * 2 + 1]) + sample,
                                           -32768, 32767);
            s_block[i * 2 + 0] = s16(left);
            s_block[i * 2 + 1] = s16(right);
            s_uiPhase += s_uiStep;
            s_uiFrames--;
        }
        s_read.store(read + take, std::memory_order_release);
        sceAudioOutputBlocking(s_channel, PSP_AUDIO_VOLUME_MAX, s_block);
    }
    return 0;
}

}  // namespace

bool init() {
    for (int i = 0; i < UI_WAVE_SIZE; i++)
        s_uiWave[i] = s16(std::sin(6.283185307f * float(i) /
                                   float(UI_WAVE_SIZE)) * 4800.f);
    s_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BLOCK_FRAMES,
                                  PSP_AUDIO_FORMAT_STEREO);
    if (s_channel < 0) {
        RS_LOGE("audio: channel reserve failed (%08x)", unsigned(s_channel));
        return false;
    }
    s_write.store(0);
    s_read.store(0);
    s_flush.store(false);
    s_paused.store(true);
    s_running.store(true, std::memory_order_release);
    s_thread = sceKernelCreateThread("rs_audio", audioThread, 0x10,
                                     16 * 1024, PSP_THREAD_ATTR_USER, nullptr);
    if (s_thread < 0 || sceKernelStartThread(s_thread, 0, nullptr) < 0) {
        RS_LOGE("audio: thread start failed");
        s_running.store(false);
        if (s_thread >= 0) sceKernelDeleteThread(s_thread);
        s_thread = -1;
        sceAudioChRelease(s_channel);
        s_channel = -1;
        return false;
    }
    RS_LOGI("audio: channel %d at %u Hz", s_channel, unsigned(OUTPUT_RATE));
    return true;
}

void playUiSound(UiSound sound) {
    if (s_channel < 0) return;
    s_uiCommand.store(int(sound), std::memory_order_release);
}

void shutdown() {
    s_running.store(false, std::memory_order_release);
    if (s_thread >= 0) {
        sceKernelWaitThreadEnd(s_thread, nullptr);
        sceKernelDeleteThread(s_thread);
        s_thread = -1;
    }
    if (s_channel >= 0) {
        sceAudioChRelease(s_channel);
        s_channel = -1;
    }
}

void setSourceRate(u32 srcRate) {
    if (srcRate < 8000 || srcRate > 192000) srcRate = OUTPUT_RATE;
    s_srcRate = srcRate;
    s_step = u32((u64(srcRate) << 16) / OUTPUT_RATE);
    s_frac = 0;
    s_lastL = s_lastR = 0;   /* don't interpolate against the old game's tail */
}

void push(const s16* stereo, u32 frames) {
    if (!frames) return;

    if (s_srcRate == OUTPUT_RATE) {
        u32 write = s_write.load(std::memory_order_relaxed);
        for (u32 i = 0; i < frames; i++) {
            if (write - s_read.load(std::memory_order_acquire) >= RING_FRAMES) {
                s_dropped.fetch_add(frames - i, std::memory_order_relaxed);
                break;   /* full: drop */
            }
            const u32 idx = write & RING_MASK;
            s_ring[idx * 2 + 0] = stereo[i * 2 + 0];
            s_ring[idx * 2 + 1] = stereo[i * 2 + 1];
            write++;
            s_write.store(write, std::memory_order_release);
        }
        return;
    }

    /* Linear resample srcRate → 44100. `s_frac` carries across pushes. */
    u32 pos = s_frac;
    u32 write = s_write.load(std::memory_order_relaxed);
    while (true) {
        const u32 srcIdx = pos >> 16;
        if (srcIdx >= frames) break;
        const u32 t = pos & 0xFFFF;
        const s16 l0 = srcIdx == 0 ? s_lastL : stereo[(srcIdx - 1) * 2 + 0];
        const s16 r0 = srcIdx == 0 ? s_lastR : stereo[(srcIdx - 1) * 2 + 1];
        const s16 l1 = stereo[srcIdx * 2 + 0];
        const s16 r1 = stereo[srcIdx * 2 + 1];
        if (write - s_read.load(std::memory_order_acquire) < RING_FRAMES) {
            const u32 idx = write & RING_MASK;
            s_ring[idx * 2 + 0] = s16(l0 + ((s32(l1) - l0) * s32(t) >> 16));
            s_ring[idx * 2 + 1] = s16(r0 + ((s32(r1) - r0) * s32(t) >> 16));
            write++;
            s_write.store(write, std::memory_order_release);
        } else {
            s_dropped.fetch_add(1, std::memory_order_relaxed);
        }
        pos += s_step;
    }
    s_frac = pos - (frames << 16);
    s_lastL = stereo[(frames - 1) * 2 + 0];
    s_lastR = stereo[(frames - 1) * 2 + 1];
}

void clear() {
    /* Producer side: request a drain and wait for the consumer to acknowledge
     * it before returning. The old fire-and-forget flush could run after the
     * core had already produced its new post-load audio, discarding that
     * fresh prefill and guaranteeing an audible underrun on resume. */
    s_flush.store(true, std::memory_order_release);
    if (!s_running.load(std::memory_order_acquire)) {
        s_read.store(s_write.load(std::memory_order_acquire),
                     std::memory_order_release);
        s_flush.store(false, std::memory_order_release);
    } else {
        int waits = 0;
        while (s_flush.load(std::memory_order_acquire) && waits++ < 500)
            sceKernelDelayThread(100); /* normally one 5.8 ms audio block */
        if (s_flush.load(std::memory_order_acquire))
            RS_LOGW("audio: flush acknowledgement timed out");
    }
    s_frac   = 0;
    s_lastL  = s_lastR = 0;
}

void setPaused(bool paused) {
    s_paused.store(paused, std::memory_order_release);
}
bool isPaused() { return s_paused.load(std::memory_order_acquire); }

u32 buffered() {
    return s_write.load(std::memory_order_acquire) -
           s_read.load(std::memory_order_acquire);
}
u32 capacity() { return RING_FRAMES; }

u32 underruns() { return s_underruns.load(std::memory_order_relaxed); }
u32 droppedFrames() { return s_dropped.load(std::memory_order_relaxed); }
void resetTelemetry() {
    s_underruns.store(0, std::memory_order_relaxed);
    s_dropped.store(0, std::memory_order_relaxed);
}

}  // namespace rs::audio
