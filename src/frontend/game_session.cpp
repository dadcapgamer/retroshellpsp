#include "frontend/game_session.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/power.h"
#include "platform/psp/threading.h"
#include "platform/psp/vram.h"
#include "runtime/arena.h"
#include "runtime/bounds.h"
#include "runtime/config.h"
#include "runtime/host_services.h"
#include "runtime/log.h"

#include "miniz.h"

#include <pspctrl.h>
#include <pspgu.h>
#include <pspkernel.h>

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace rs {

namespace {
constexpr u32 MENU_COMBO = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER;
constexpr float SRAM_FLUSH_SECONDS = 10.f;
constexpr int MAX_WALL_CATCHUP = 3;
/* PicoDrive's six-frame profile is the measured PSP-1000 baseline: reducing
 * it made both tested Genesis workloads slower and starved their audio. */
constexpr int MAX_DEEP_RECOVERY = 6;
/* Secret of Mana's byte-validated idle-loop substitutions reduced skipped
 * frames to roughly 8-10 ms while expensive colour-math frames remain around
 * 25-30 ms. A six-frame batch now over-recovers and limits presentation to
 * about 15 Hz. Four frames still provide approximately one net audio frame
 * per demanding 50 ms batch, but should present closer to 20 Hz. Keep this
 * separate from PicoDrive so its proven profile cannot regress again. */
constexpr int MAX_SNES_RECOVERY = 4;
/* Four-frame SNES batches gave smoother visual cadence, but a sustained
 * colour-math workload could consume the queue faster than they replenish
 * it. Deepen only below ~35 ms of audio, then return to four as soon as the
 * emergency has passed. This retains tyl4's cadence without repeating its
 * steadily increasing real-hardware underruns. */
constexpr int MAX_SNES_EMERGENCY_RECOVERY = 6;
constexpr u32 AUDIO_SNES_EMERGENCY =
    audio::OUTPUT_BLOCK_FRAMES * 6u;
/* PCE Fast nearly sustained native speed with three-frame recovery on the
 * PSP-1000, but could not replenish the queue and accumulated persistent
 * underruns. Four frames give it one bounded catch-up frame while retaining
 * a shallower exit watermark than the deep SNES/PicoDrive profile. */
constexpr int MAX_PCE_RECOVERY = 4;
/* Enter recovery while there is still ~46 ms queued, not at the old
 * ~23 ms emergency boundary. Exit around 81 ms so normal workload spikes
 * have room without pushing latency toward the full 186 ms ring capacity. */
constexpr u32 AUDIO_RECOVERY_ENTER = audio::OUTPUT_BLOCK_FRAMES * 8u;
constexpr u32 AUDIO_RECOVERY_EXIT  = audio::OUTPUT_BLOCK_FRAMES * 14u;
constexpr u32 AUDIO_BALANCED_EXIT  = audio::OUTPUT_BLOCK_FRAMES * 10u;
constexpr u32 AUDIO_PRIME_TARGET   = AUDIO_RECOVERY_EXIT;
constexpr u32 MIN_CORE_HEADROOM = 2u * 1024u * 1024u;
constexpr u32 MIN_BUFFERED_CORE_HEADROOM = 1u * 1024u * 1024u;
constexpr u32 MAX_ROM_BYTES = 16u * 1024u * 1024u;
constexpr u32 MAX_STREAMED_ROM_BYTES = 64u * 1024u * 1024u;
constexpr u32 MAX_ZIP_ENTRIES = 4096;
constexpr u64 MAX_COMPRESSION_RATIO = 200;

const char* MENU_LABELS[] = {
    "Resume", "Save state", "Load state", "Reset", "Aspect", "Filter",
    "Screenshot", "Exit game",
};
}  // namespace

const char* GameSession::scaleOptionName(ScaleMode mode) {
    switch (mode) {
        case ScaleMode::FourThree: return "4:3";
        case ScaleMode::Stretch:   return "stretch";
        case ScaleMode::OneToOne:  return "1:1";
        default:                   return "fit";
    }
}

const char* GameSession::scaleDisplayName(ScaleMode mode) {
    switch (mode) {
        case ScaleMode::FourThree: return "4:3";
        case ScaleMode::Stretch:   return "Stretch";
        case ScaleMode::OneToOne:  return "1:1";
        default:                   return "Original";
    }
}

bool GameSession::injectFailure(const char* stage) const {
#ifdef RS_FAILURE_INJECTION
    return cfg::gameOption(m_game.pathHash, "injectFailure") == stage;
#else
    (void)stage;
    return false;
#endif
}

/* ---------------------------------------------------------------------- */
/* Launch / teardown                                                       */
/* ---------------------------------------------------------------------- */

bool GameSession::startCore(App& app) {
    const CoreInfo* info = app.cores().find(m_coreName.c_str());
    if (!info) {
        std::snprintf(m_error, sizeof m_error, "core '%s' not installed",
                      m_coreName.c_str());
        return false;
    }

    /* Resolve frontend settings while the menu heap is still intact.
     * Large cores must not trigger JSON parsing/allocation after load. */
    const std::string scale = cfg::gameOption(m_game.pathHash, "scale");
    m_scaleMode = scale == "1:1"     ? ScaleMode::OneToOne
                : scale == "4:3"     ? ScaleMode::FourThree
                : scale == "stretch" ? ScaleMode::Stretch
                                     : ScaleMode::Fit;
    m_nearestFilter =
        cfg::gameOption(m_game.pathHash, "filter") == "nearest";

    /* 1. Evict frontend caches (the arena/vram space becomes the core's). */
    app.evictForCore();
    m_frontendEvicted = true;

#ifndef RS_STATIC_CORES
    /* 2. PRX mode: the module loader needs partition space, so the arena
     * is released around the load and re-reserved afterwards. */
    mem::shutdown();
    m_arenaReady = false;
#endif
    bool loaded = false;
    if (!injectFailure("missing_prx"))
        loaded = m_cores.loadCore(*info);
#ifndef RS_STATIC_CORES
    if (!mem::init()) {
        std::snprintf(m_error, sizeof m_error, "arena re-reserve failed");
        return false;
    }
    m_arenaReady = true;
#endif
    if (!loaded) {
        std::snprintf(m_error, sizeof m_error, "%s",
                      injectFailure("missing_prx") ? "injected missing PRX"
                                                    : m_cores.error());
        return false;
    }
    if (injectFailure("bad_api")) {
        std::snprintf(m_error, sizeof m_error, "injected bad API");
        return false;
    }
    /* Init only now: the arena is guaranteed to be in place. */
    host::beginCoreSession();
    m_coreSessionStarted = true;
    if (injectFailure("arena_exhaustion")) {
        (void)mem::alloc(mem::available(), 16);
        std::snprintf(m_error, sizeof m_error, "injected arena exhaustion");
        return false;
    }
    if (injectFailure("init_failure") ||
        !m_cores.initialize(host::table())) {
        std::snprintf(m_error, sizeof m_error, "core init failed");
        return false;
    }

    /* 3. ROM into the arena. */
    u8* romData = nullptr;
    u32 romSize = 0;
    if (!m_game.zipEntry.empty()) {
        if (info->preferVfs) {
            std::snprintf(m_error, sizeof m_error,
                          "this core requires an uncompressed ROM");
            return false;
        }
        mz_zip_archive zip;
        std::memset(&zip, 0, sizeof zip);
        if (!mz_zip_reader_init_file(&zip, m_game.path.c_str(), 0)) {
            std::snprintf(m_error, sizeof m_error, "zip open failed");
            return false;
        }
        if (mz_zip_reader_get_num_files(&zip) > MAX_ZIP_ENTRIES) {
            mz_zip_reader_end(&zip);
            std::snprintf(m_error, sizeof m_error, "zip has too many entries");
            return false;
        }
        const int idx =
            mz_zip_reader_locate_file(&zip, m_game.zipEntry.c_str(), nullptr, 0);
        mz_zip_archive_file_stat st;
        if (idx < 0 || !mz_zip_reader_file_stat(&zip, mz_uint(idx), &st)) {
            mz_zip_reader_end(&zip);
            std::snprintf(m_error, sizeof m_error, "zip entry missing");
            return false;
        }
        if (!st.m_is_supported || st.m_is_encrypted ||
            !st.m_uncomp_size || st.m_uncomp_size > MAX_ROM_BYTES ||
            !bounds::decompressionRatio(st.m_comp_size, st.m_uncomp_size,
                                        MAX_COMPRESSION_RATIO) ||
            mem::available() <= MIN_CORE_HEADROOM ||
            st.m_uncomp_size > mem::available() - MIN_CORE_HEADROOM) {
            mz_zip_reader_end(&zip);
            std::snprintf(m_error, sizeof m_error, "rom exceeds memory budget");
            return false;
        }
        romSize = u32(st.m_uncomp_size);
        romData = static_cast<u8*>(mem::alloc(romSize, 64));
        if (!romData ||
            !mz_zip_reader_extract_to_mem(&zip, mz_uint(idx), romData, romSize,
                                          0)) {
            mz_zip_reader_end(&zip);
            std::snprintf(m_error, sizeof m_error, "zip extract failed");
            return false;
        }
        mz_zip_reader_end(&zip);
    } else {
        const s32 size = fs::fileSize(m_game.path.c_str());
        if (size <= 0) {
            std::snprintf(m_error, sizeof m_error, "rom missing");
            return false;
        }
        if (u32(size) > MAX_STREAMED_ROM_BYTES) {
            std::snprintf(m_error, sizeof m_error, "rom exceeds memory budget");
            return false;
        }
        const u32 requiredHeadroom = info->requiresFullContent
                                         ? MIN_BUFFERED_CORE_HEADROOM
                                         : MIN_CORE_HEADROOM;
        const bool canBuffer =
            !info->preferVfs && u32(size) <= MAX_ROM_BYTES &&
            mem::available() > requiredHeadroom &&
            u32(size) <= mem::available() - requiredHeadroom;
        if (canBuffer) {
            romSize = u32(size);
            romData = static_cast<u8*>(mem::alloc(romSize, 64));
            if (!romData ||
                fs::readRange(m_game.path.c_str(), romData, 0, romSize) !=
                    s32(romSize)) {
                std::snprintf(m_error, sizeof m_error, "rom read failed");
                return false;
            }
        } else if (!info->requiresFullContent) {
            /* The shim exposes a read-only libretro VFS backed by host
             * random-access reads. Cores that require a full path can stream
             * large uncompressed ROMs without duplicating them in RAM. */
            romData = nullptr;
            romSize = 0;
        } else {
            std::snprintf(m_error, sizeof m_error,
                          "rom exceeds core memory budget");
            return false;
        }
    }

    /* 4. Boot the core. */
    if (injectFailure("corrupt_rom") && romData && romSize)
        romData[0] ^= 0xFF;
    host::setActiveGame(m_game.pathHash);
    if (injectFailure("rom_rejection") ||
        !m_cores.core().loadROM(m_game.path.c_str(), romData, romSize)) {
        std::snprintf(m_error, sizeof m_error, "core rejected rom");
        return false;
    }
    RS_LOGI("session: ROM loaded; %u KB arena free before SRAM",
            unsigned(mem::available() / 1024));
    if (!save::loadSram(m_game, m_cores.core()))
        RS_LOGW("session: SRAM restore skipped; continuing with core defaults");
    RS_LOGI("session: post-load restore complete");
    m_romLoaded = true;   /* only now is it safe to persist SRAM on exit */
    power::setCpuMhz(cfg::get().cpuGameMhz);
    RS_LOGI("session: game clock set to %d MHz", cfg::get().cpuGameMhz);
    audio::setPaused(true);
    audio::clear();
    audio::resetTelemetry();
    primeAudio();
    audio::setPaused(false);
    RS_LOGI("session: audio reset and primed");
    if (m_coreName == "pcefast")
        RS_LOGI("session: PCE recovery cap %d, exit %u frames",
                MAX_PCE_RECOVERY, unsigned(AUDIO_BALANCED_EXIT));
    else if (m_coreName == "snes9x2005")
        RS_LOGI("session: SNES adaptive recovery %d-%d, emergency %u, "
                "exit %u frames",
                MAX_SNES_RECOVERY, MAX_SNES_EMERGENCY_RECOVERY,
                unsigned(AUDIO_SNES_EMERGENCY),
                unsigned(AUDIO_RECOVERY_EXIT));
    else
        RS_LOGI("session: deep recovery cap %d, exit %u frames",
                MAX_DEEP_RECOVERY, unsigned(AUDIO_RECOVERY_EXIT));
    RS_LOGI("session: '%s' running (%u KB arena free)", m_game.name.c_str(),
            unsigned(mem::available() / 1024));
    return true;
}

void GameSession::enter(App& app) {
    if (startCore(app)) {
        m_state = State::Running;
        app.toast("L + R + SELECT for menu");
    } else {
        RS_LOGE("session: launch failed: %s", m_error);
        m_state = State::Failed;
    }
}

void GameSession::teardown(App& app, bool restoreFrontend) {
    if (m_teardownComplete) return;
    m_teardownComplete = true;

    audio::setPaused(true);
    const bool completedLaunch = m_romLoaded;
    finishPeriodicSram();
    /* Persist SRAM only if a ROM actually loaded and ran. A failed launch
     * leaves the core "loaded" (module up) but with default/empty SRAM —
     * saving that would clobber the user's real .srm. */
    if (m_romLoaded) {
        if (m_cores.core().sramDirty())
            save::saveSram(m_game, m_cores.core());
        m_cores.core().unloadROM();
        m_romLoaded = false;
    }
    gfx::Renderer::freeTexture(m_frameTex);
    gfx::Renderer::freeTexture(m_thumbTex);

    /* Unload the core BEFORE releasing the arena: the core's shutdown()
     * touches state it allocated from the arena, so freeing the partition
     * first would be a use-after-free. */
#ifndef RS_STATIC_CORES
    m_cores.unloadCore();
    if (m_coreSessionStarted) {
        host::endCoreSession();
        m_coreSessionStarted = false;
    }
    if (restoreFrontend && m_frontendEvicted) {
        if (m_arenaReady) mem::shutdown();
        m_arenaReady = mem::init();
    }
#else
    m_cores.unloadCore();
    if (m_coreSessionStarted) {
        host::endCoreSession();
        m_coreSessionStarted = false;
    }
    if (restoreFrontend && m_frontendEvicted) mem::reset(0);
#endif
    audio::clear();
    host::setActiveGame(0);
    power::setCpuMhz(cfg::get().cpuMenuMhz);

    /* Persist launch bookkeeping only after the PRX and its heap have been
     * released. cJSON and Library::save use the small newlib heap, so these
     * writes must not compete with a loaded emulator core. */
    if (completedLaunch) {
        app.library().notePlayed(m_game.pathHash, power::localTimestamp());
        cfg::setGameOption(m_game.pathHash, "core", m_coreName.c_str());
        if (m_videoOptionsDirty) {
            cfg::setGameOption(m_game.pathHash, "scale",
                               scaleOptionName(m_scaleMode));
            cfg::setGameOption(m_game.pathHash, "filter",
                               m_nearestFilter ? "nearest" : "linear");
        }
    }

    if (restoreFrontend && m_frontendEvicted) {
        app.restoreAfterCore();
        m_frontendEvicted = false;
    }
    m_state = State::Exiting;
    RS_LOGI("session: teardown complete%s",
            restoreFrontend ? "; returning to frontend" : "; application exit");
}

void GameSession::shutdown(App& app) {
    teardown(app, /*restoreFrontend=*/false);
}

void GameSession::exitToHome(App& app) {
    teardown(app, /*restoreFrontend=*/true);
    app.switchScene(std::make_unique<HomeScene>());
}

/* ---------------------------------------------------------------------- */
/* Input mapping                                                           */
/* ---------------------------------------------------------------------- */

u32 GameSession::mapButtons(const input::Pad& pad) const {
    /* Defaults; per-game remapping arrives with the settings overlay. */
    const u32 held = pad.held();
    u32 out = 0;
    if (held & PSP_CTRL_UP) out |= RS_BTN_UP;
    if (held & PSP_CTRL_DOWN) out |= RS_BTN_DOWN;
    if (held & PSP_CTRL_LEFT) out |= RS_BTN_LEFT;
    if (held & PSP_CTRL_RIGHT) out |= RS_BTN_RIGHT;
    if (held & PSP_CTRL_CIRCLE) out |= RS_BTN_A;
    if (held & PSP_CTRL_CROSS) out |= RS_BTN_B;
    if (held & PSP_CTRL_TRIANGLE) out |= RS_BTN_X;
    if (held & PSP_CTRL_SQUARE) out |= RS_BTN_Y;
    if (held & PSP_CTRL_LTRIGGER) out |= RS_BTN_L;
    if (held & PSP_CTRL_RTRIGGER) out |= RS_BTN_R;
    if (held & PSP_CTRL_START) out |= RS_BTN_START;
    if (held & PSP_CTRL_SELECT) out |= RS_BTN_SELECT;
    return out;
}

int GameSession::sramWriter(void* arg) {
    auto* self = static_cast<GameSession*>(arg);
    const u32 start = sceKernelGetSystemTimeLow();
    self->m_sramWriteOk = save::writeSramSnapshot(
        self->m_game, self->m_sramSnapshot, self->m_sramSnapshotSize);
    self->m_sramWriteUs = sceKernelGetSystemTimeLow() - start;
    return 0;
}

void GameSession::finishPeriodicSram() {
    if (m_sramThread < 0) return;
    thread::join(m_sramThread);
    m_sramThread = -1;
    RS_LOGI("save: async SRAM flush %s (%u bytes, %u ms)",
            m_sramWriteOk ? "ok" : "FAILED", unsigned(m_sramSnapshotSize),
            unsigned(m_sramWriteUs / 1000u));
    if (m_sramSnapshot) {
        host::table()->mem_free(m_sramSnapshot);
        m_sramSnapshot = nullptr;
    }
    m_sramSnapshotSize = 0;
}

void GameSession::queuePeriodicSram() {
    /* Reap the previous job before reusing its bounded snapshot. With a
     * ten-second interval a normal 150 ms Memory Stick write has long since
     * finished, so this join does not touch frame pacing. */
    finishPeriodicSram();

    const u32 size = m_cores.core().sramSize();
    const void* data = m_cores.core().sramData();
    if (!size || !data || size > 1024u * 1024u) {
        RS_LOGW("save: invalid async SRAM snapshot (%u bytes)",
                unsigned(size));
        return;
    }
    m_sramSnapshot = host::table()->mem_alloc(size, 16);
    if (!m_sramSnapshot) {
        RS_LOGW("save: no arena space for async SRAM snapshot (%u bytes)",
                unsigned(size));
        return;
    }
    std::memcpy(m_sramSnapshot, data, size);
    m_sramSnapshotSize = size;
    m_sramWriteOk = false;
    m_sramWriteUs = 0;
    m_sramThread = thread::spawn("rs_sram_write", sramWriter, this, 32);
    if (m_sramThread < 0) {
        RS_LOGW("save: SRAM writer thread failed to start");
        host::table()->mem_free(m_sramSnapshot);
        m_sramSnapshot = nullptr;
        m_sramSnapshotSize = 0;
        return;
    }
    RS_LOGI("save: async SRAM snapshot queued (%u bytes, audio %u)",
            unsigned(size), unsigned(audio::buffered()));
}

/* ---------------------------------------------------------------------- */
/* Update                                                                  */
/* ---------------------------------------------------------------------- */

void GameSession::updateRunning(App& app, float dt) {
    const auto& pad = app.pad();

    if ((pad.held() & MENU_COMBO) == MENU_COMBO &&
        pad.isPressed(PSP_CTRL_SELECT)) {
        openMenu(app);
        return;
    }

    u32 buttons = mapButtons(pad);
    host::setInputState(buttons);

    /* Pace normal emulation by wall clock at the core's native rate.
     * Recovery may run a small bounded logic/audio-only catch-up burst:
     * producing exactly one frame of audio per wall frame can maintain a
     * depleted queue, but can never refill it. */
    const float period = 1.f / float(m_cores.core().fps());
    const bool pceRecovery = m_coreName == "pcefast";
    const bool snesRecovery = m_coreName == "snes9x2005";
    int recoveryFrameCap = pceRecovery
        ? MAX_PCE_RECOVERY
        : snesRecovery ? MAX_SNES_RECOVERY : MAX_DEEP_RECOVERY;
    const u32 recoveryExit =
        pceRecovery ? AUDIO_BALANCED_EXIT : AUDIO_RECOVERY_EXIT;
    m_emuAccum += dt;
    int framesDue = int(m_emuAccum / period);
    framesDue = rsClamp(framesDue, 0, MAX_WALL_CATCHUP);
    /* Hysteresis prevents recovery mode from flapping every time the audio
     * thread consumes one block. Recovery uses a bounded six-frame batch,
     * but must still present its final frame: on real PSP hardware a core
     * may only maintain (not increase) the queue while the display loop is
     * vblank paced. Suppressing the entire batch would then freeze video
     * forever because the exit threshold can never be reached. */
    const u32 bufferedAudio = audio::buffered();
    if (snesRecovery && bufferedAudio < AUDIO_SNES_EMERGENCY)
        recoveryFrameCap = MAX_SNES_EMERGENCY_RECOVERY;
    if (!m_audioRecovery && bufferedAudio < AUDIO_RECOVERY_ENTER)
        m_audioRecovery = true;
    else if (m_audioRecovery && bufferedAudio >= recoveryExit)
        m_audioRecovery = false;
    if (m_audioRecovery)
        framesDue = recoveryFrameCap;
    int ran = 0;
    while (ran < framesDue) {
        /* A slow presentation pass can contain multiple emulated frames.
         * Re-sample immediately before each one instead of applying the
         * input captured before the entire 30-50 ms batch. */
        app.padMutable().refreshHeld();
        buttons = mapButtons(app.pad());
        host::setInputState(buttons);
        const u32 frameStart = sceKernelGetSystemTimeLow();
        /* With multiple wall-due or recovery frames, skip obsolete
         * intermediate images and present the newest. This guarantees
         * forward visual progress even when recovery cannot reach its high
         * watermark on native hardware. */
        const bool renderVideo = (ran == framesDue - 1);
        const u32 sequenceBefore = m_cores.core().frame().sequence;
        m_cores.core().runFrame(buttons, renderVideo);
        const u32 frameUs = sceKernelGetSystemTimeLow() - frameStart;
        const bool producedVideo =
            m_cores.core().frame().sequence != sequenceBefore;
        m_perfEmuUs += frameUs;
        if (m_perfSampleCount < PERF_SAMPLES)
            m_perfSamples[m_perfSampleCount++] = frameUs;
        if (producedVideo) {
            m_perfRenderedUs += frameUs;
            m_perfRenderedFrames++;
            if (m_perfRenderedSampleCount < PERF_SAMPLES)
                m_perfRenderedSamples[m_perfRenderedSampleCount++] = frameUs;
        } else {
            m_perfSkippedUs += frameUs;
            m_perfSkippedFrames++;
            if (m_perfSkippedSampleCount < PERF_SAMPLES)
                m_perfSkippedSamples[m_perfSkippedSampleCount++] = frameUs;
        }
        if (m_emuAccum >= period) m_emuAccum -= period;
        else m_emuAccum = 0.f;  /* audio recovery frame ran ahead of wall */
        ran++;
        /* A recovery batch is only an upper bound. Some cores emit more
         * audio per retro_run() than others, so re-check after every frame
         * and stop as soon as the safe high-water mark is restored. Without
         * this, the remaining frames in a fixed four-frame burst can
         * overflow an otherwise healthy queue. */
        if (m_audioRecovery && audio::buffered() >= recoveryExit) {
            m_audioRecovery = false;
            /* Never leave a recovery batch before its presentation frame.
             * PCE Fast can refill the queue in an early skipped frame; the
             * old early break then left the last uploaded image frozen even
             * though logic and audio continued at 60 Hz. Shorten the batch
             * to exactly one final rendered frame instead. */
            if (!renderVideo)
                framesDue = ran + 1;
            else
                break;
        }
    }
    if (m_emuAccum > period) m_emuAccum = period;
    if (ran > 0) {
        m_videoProbe.observe(m_cores.core().frame());
        if (m_videoProbe.blackFrames() == FrameProbe::BLACK_WARNING_FRAMES) {
            RS_LOGW("video: persistent black output; FPS only confirms core "
                    "frame calls, not visible rendering");
        } else if (m_videoProbe.staticFrames() ==
                   FrameProbe::STATIC_WARNING_FRAMES) {
            RS_LOGW("video: output unchanged for %u observations",
                    unsigned(m_videoProbe.staticFrames()));
        } else if (m_videoProbe.missingFrames() ==
                   FrameProbe::MISSING_WARNING_FRAMES) {
            RS_LOGW("video: core has not supplied a valid frame");
        }
    }

    /* Report against an independent wall-clock window. This avoids
     * attributing the work of a boundary frame to the next dt window and
     * claiming 60 fps when core calls take more than the native budget. */
    m_perfFrames += ran;
    const u32 perfNowUs = sceKernelGetSystemTimeLow();
    if (!m_perfWindowStartUs) m_perfWindowStartUs = perfNowUs;
    const u32 perfWallUs = perfNowUs - m_perfWindowStartUs;
    if (perfWallUs >= 1000000u) {
        std::sort(m_perfSamples, m_perfSamples + m_perfSampleCount);
        std::sort(m_perfRenderedSamples,
                  m_perfRenderedSamples + m_perfRenderedSampleCount);
        std::sort(m_perfSkippedSamples,
                  m_perfSkippedSamples + m_perfSkippedSampleCount);
        const int p95Index = m_perfSampleCount
            ? (m_perfSampleCount * 95 - 1) / 100 : 0;
        const u32 p95 = m_perfSampleCount ? m_perfSamples[p95Index] : 0;
        const int renderP95Index = m_perfRenderedSampleCount
            ? (m_perfRenderedSampleCount * 95 - 1) / 100 : 0;
        const int skipP95Index = m_perfSkippedSampleCount
            ? (m_perfSkippedSampleCount * 95 - 1) / 100 : 0;
        const u32 renderP95 = m_perfRenderedSampleCount
            ? m_perfRenderedSamples[renderP95Index] : 0;
        const u32 skipP95 = m_perfSkippedSampleCount
            ? m_perfSkippedSamples[skipP95Index] : 0;
        const u32 actualFps = u32(
            (u64(m_perfFrames) * 1000000u + perfWallUs / 2u) / perfWallUs);
        const u32 speedPercent = u32(
            double(actualFps) * 100.0 / m_cores.core().fps() + 0.5);
        RS_LOGI("perf: %u emu fps (%u%% speed) | avg %u us | p95 %u us | "
                "cpu %d MHz | "
                "arena %u/%u KB | allocfail %u | audio buf %u underrun %u "
                "drop %u | "
                "video %s nonblack %u/64",
                unsigned(actualFps), unsigned(speedPercent),
                unsigned(m_perfEmuUs / u32(m_perfFrames > 0 ? m_perfFrames : 1)),
                unsigned(p95), power::cpuMhz(),
                unsigned(mem::used() / 1024), unsigned(mem::highWater() / 1024),
                unsigned(host::allocationFailures()),
                unsigned(audio::buffered()),
                unsigned(audio::underruns()), unsigned(audio::droppedFrames()),
                m_videoProbe.status(),
                unsigned(m_videoProbe.nonBlackSamples()));
        RS_LOGI("perf detail: rendered %d avg %u us p95 %u us | "
                "skipped %d avg %u us p95 %u us | uploaded seq %u | "
                "recovery %s",
                m_perfRenderedFrames,
                unsigned(m_perfRenderedUs /
                    u32(m_perfRenderedFrames ? m_perfRenderedFrames : 1)),
                unsigned(renderP95),
                m_perfSkippedFrames,
                unsigned(m_perfSkippedUs /
                    u32(m_perfSkippedFrames ? m_perfSkippedFrames : 1)),
                unsigned(skipP95), unsigned(m_uploadedFrameSequence),
                m_audioRecovery ? "on" : "off");
        m_perfWindowStartUs = perfNowUs;
        m_perfEmuUs = 0;
        m_perfFrames = 0;
        m_perfSampleCount = 0;
        m_perfRenderedUs = m_perfSkippedUs = 0;
        m_perfRenderedFrames = m_perfSkippedFrames = 0;
        m_perfRenderedSampleCount = m_perfSkippedSampleCount = 0;
    }

    /* Periodic battery-save flush. */
    m_sramTimer += dt;
    if (m_sramTimer >= SRAM_FLUSH_SECONDS) {
        m_sramTimer = 0.f;
        if (cfg::get().autosave && m_cores.core().sramDirty())
            queuePeriodicSram();
    }
}

void GameSession::openMenu(App& app) {
    (void)app;
    audio::setPaused(true);
    /* Keep save-state and screenshot I/O serialized with the background SRAM
     * writer. This wait occurs only after gameplay audio has been paused. */
    finishPeriodicSram();
    resetPerfWindow();
    m_state = State::Menu;
    m_menuRow = 0;
    m_menuPos.snap(0.f);
    m_menuScroll.snap(0.f);
    m_menuFade.start(0.18f);
    save::querySlots(m_game, m_slots);
    m_thumbSlot = -1;
}

void GameSession::resetPerfWindow() {
    m_perfWindowStartUs = 0;
    m_perfEmuUs = 0;
    m_perfFrames = 0;
    m_perfSampleCount = 0;
    m_perfRenderedUs = m_perfSkippedUs = 0;
    m_perfRenderedFrames = m_perfSkippedFrames = 0;
    m_perfRenderedSampleCount = m_perfSkippedSampleCount = 0;
}

void GameSession::primeAudio() {
    int primedFrames = 0;
    while (audio::buffered() < AUDIO_PRIME_TARGET && primedFrames < 8) {
        m_cores.core().runFrame(0, /*renderVideo=*/false);
        primedFrames++;
    }
    m_emuAccum = 0.f;
    m_audioRecovery = audio::buffered() < AUDIO_RECOVERY_EXIT;
    RS_LOGI("audio: primed %d frames to %u/%u", primedFrames,
            unsigned(audio::buffered()), unsigned(AUDIO_PRIME_TARGET));
}

void GameSession::resumeGame(bool discardAudio) {
    if (discardAudio) {
        audio::clear();
        primeAudio();
    } else {
        m_audioRecovery = audio::buffered() < AUDIO_RECOVERY_ENTER;
    }
    audio::setPaused(false);
    m_emuAccum = 0.f;
    resetPerfWindow();
    m_state = State::Running;
}

void GameSession::makeThumb(u16* out) const {
    /* Nearest-neighbour downsample of the core frame to thumbnail size. */
    const RSVideoFrame f = const_cast<CoreManager&>(m_cores).core().frame();
    if (!f.pixels ||
        (f.format != RS_PIXFMT_RGB565 &&
         f.format != RS_PIXFMT_RGBA5551)) {
        std::memset(out, 0, save::THUMB_W * save::THUMB_H * 2);
        return;
    }
    const u8* src = static_cast<const u8*>(f.pixels);
    for (int y = 0; y < save::THUMB_H; y++) {
        const int sy = y * f.height / save::THUMB_H;
        const u16* row = reinterpret_cast<const u16*>(src + sy * f.pitch);
        for (int x = 0; x < save::THUMB_W; x++) {
            const u16 pixel = row[x * f.width / save::THUMB_W];
            /* Save thumbnails are GU_PSM_5650. Native SNES frames are
             * BGR555, so expand the green field and discard the spare bit. */
            out[y * save::THUMB_W + x] =
                f.format == RS_PIXFMT_RGBA5551
                    ? u16((pixel & 0x001Fu) | ((pixel & 0x03E0u) << 1) |
                          ((pixel & 0x7C00u) << 1))
                    : pixel;
        }
    }
}

void GameSession::updateMenu(App& app) {
    const auto& pad = app.pad();
    auto& core = m_cores.core();

    if (pad.navPressed(PSP_CTRL_UP) && m_menuRow > 0) m_menuRow--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_menuRow < MENU_COUNT - 1)
        m_menuRow++;

    if (m_menuRow == MENU_SAVE || m_menuRow == MENU_LOAD) {
        if (pad.navPressed(PSP_CTRL_LEFT) && m_slot > 0) m_slot--;
        if (pad.navPressed(PSP_CTRL_RIGHT) && m_slot < save::SLOTS - 1)
            m_slot++;
    }

    const auto cycleAspect = [this](int direction) {
        int mode = static_cast<int>(m_scaleMode);
        mode = (mode + direction + 4) % 4;
        m_scaleMode = static_cast<ScaleMode>(mode);
        m_videoOptionsDirty = true;
    };
    if (m_menuRow == MENU_ASPECT) {
        if (pad.navPressed(PSP_CTRL_LEFT)) cycleAspect(-1);
        if (pad.navPressed(PSP_CTRL_RIGHT)) cycleAspect(1);
    } else if (m_menuRow == MENU_FILTER &&
               (pad.navPressed(PSP_CTRL_LEFT) ||
                pad.navPressed(PSP_CTRL_RIGHT))) {
        m_nearestFilter = !m_nearestFilter;
        m_videoOptionsDirty = true;
    }

    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        resumeGame();
        return;
    }

    if (!pad.isPressed(PSP_CTRL_CROSS)) return;
    switch (m_menuRow) {
        case MENU_RESUME:
            resumeGame();
            break;
        case MENU_SAVE: {
            u16 thumb[save::THUMB_W * save::THUMB_H];
            makeThumb(thumb);
            app.toast(save::saveState(m_game, core, m_slot, thumb)
                          ? "State saved"
                          : "Save failed");
            save::querySlots(m_game, m_slots);
            m_thumbSlot = -1;
            break;
        }
        case MENU_LOAD:
            if (m_slots[m_slot].exists) {
                app.toast(save::loadState(m_game, core, m_slot)
                              ? "State loaded"
                              : "Load failed");
                resumeGame(/*discardAudio=*/true);
            } else {
                app.toast("Empty slot");
            }
            break;
        case MENU_RESET:
            core.reset();
            resumeGame(/*discardAudio=*/true);
            app.toast("Reset");
            break;
        case MENU_ASPECT:
            cycleAspect(1);
            break;
        case MENU_FILTER:
            m_nearestFilter = !m_nearestFilter;
            m_videoOptionsDirty = true;
            break;
        case MENU_SCREENSHOT: {
            char path[128];
            char dir[64];
            std::snprintf(dir, sizeof dir, "%s/screenshots", fs::ROOT);
            fs::mkdirs(dir);
            std::snprintf(path, sizeof path,
                          "%s/screenshots/%08x_%u.png", fs::ROOT,
                          unsigned(m_game.pathHash),
                          unsigned(app.time() * 10.f));
            app.renderer().requestCapture(path);
            app.toast("Screenshot saved");
            resumeGame();
            break;
        }
        case MENU_EXIT:
            exitToHome(app);
            break;
    }
}

void GameSession::update(App& app, float dt) {
    switch (m_state) {
        case State::Running:
            updateRunning(app, dt);
            break;
        case State::Menu:
            m_menuPos.to(float(m_menuRow));
            m_menuPos.update(dt, 14.f);
            m_menuScroll.to(float(rsClamp(m_menuRow - 2, 0,
                                          MENU_COUNT - 5)));
            m_menuScroll.update(dt, 14.f);
            m_menuFade.update(dt);
            updateMenu(app);
            break;
        case State::Failed:
            if (app.pad().isPressed(PSP_CTRL_CIRCLE) ||
                app.pad().isPressed(PSP_CTRL_CROSS))
                exitToHome(app);
            break;
        default:
            break;
    }
}

/* ---------------------------------------------------------------------- */
/* Draw                                                                    */
/* ---------------------------------------------------------------------- */

void GameSession::drawFrame(App& app) {
    auto& r = app.renderer();
    const RSVideoFrame f = m_cores.core().frame();
    if (!f.pixels || !f.width || !f.height) return;

    const int psm = f.format == RS_PIXFMT_RGBA8888
                        ? GU_PSM_8888
                        : f.format == RS_PIXFMT_RGBA5551 ? GU_PSM_5551
                                                        : GU_PSM_5650;
    const int bpp = f.format == RS_PIXFMT_RGBA8888 ? 4 : 2;
    const u32 sourceTexW = f.pitch / u32(bpp);
    const bool direct =
        f.pitch % bpp == 0 && sourceTexW >= f.width && sourceTexW <= 512 &&
        f.storage_height >= f.height && f.storage_height <= 512 &&
        sourceTexW == rsNextPow2(sourceTexW) &&
        f.storage_height == rsNextPow2(f.storage_height);

    /* Width and height alone do not identify a libretro video surface.
     * Snes9x can change from its ordinary 256-wide buffer (512-pixel
     * backing stride) to the shim's 256-wide pseudo-hires resolve without
     * changing the reported logical dimensions.  Keeping the old binding
     * in that case draws the left half of the 512-wide source and bypasses
     * the resolve entirely.  Rebind whenever any direct-texture identity or
     * layout property changes. */
    const bool layoutChanged =
        !m_frameTex.valid() || m_frameW != f.width || m_frameH != f.height ||
        m_frameTex.psm != psm || m_directFrameTexture != direct ||
        (direct &&
         (m_frameTex.pixels != f.pixels || m_frameTex.texW != sourceTexW ||
          m_frameTex.texH != f.storage_height));

    if (layoutChanged) {
        gfx::Renderer::freeTexture(m_frameTex);
        m_directFrameTexture = false;
        if (direct) {
            m_frameTex.pixels = const_cast<void*>(f.pixels);
            m_frameTex.width = f.width;
            m_frameTex.height = f.height;
            m_frameTex.texW = u16(sourceTexW);
            m_frameTex.texH = f.storage_height;
            m_frameTex.psm = u8(psm);
            m_frameTex.swizzled = false;
            m_directFrameTexture = true;
            RS_LOGI("video: direct core texture %ux%u storage %ux%u",
                    unsigned(f.width), unsigned(f.height),
                    unsigned(sourceTexW), unsigned(f.storage_height));
        } else if (!gfx::Renderer::createTexture(
                       m_frameTex, f.width, f.height, psm, nullptr,
                       /*dynamic=*/true)) {
            m_directFrameTexture = false;
        }
        if (!m_frameTex.valid() || injectFailure("framebuffer_allocation")) {
            gfx::Renderer::freeTexture(m_frameTex);
            std::snprintf(m_error, sizeof m_error,
                          "framebuffer allocation failed");
            m_state = State::Failed;
            return;
        }
        m_frameW = f.width;
        m_frameH = f.height;
    }
    if (!m_directFrameTexture && m_uploadedFrameSequence != f.sequence) {
        gfx::Renderer::updateTexture(m_frameTex, f.pixels, f.pitch);
    }
    m_uploadedFrameSequence = f.sequence;

    /* Scale mode resolved at launch (see m_scaleMode). */
    float dw, dh;
    if (m_scaleMode == ScaleMode::OneToOne) {
        dw = f.width;
        dh = f.height;
    } else if (m_scaleMode == ScaleMode::Stretch) {
        dw = RS_SCREEN_W;
        dh = RS_SCREEN_H;
    } else if (m_scaleMode == ScaleMode::FourThree) {
        constexpr float aspect = 4.f / 3.f;
        dw = RS_SCREEN_W;
        dh = dw / aspect;
        if (dh > RS_SCREEN_H) {
            dh = RS_SCREEN_H;
            dw = dh * aspect;
        }
    } else {
        const float s = rsClamp(float(RS_SCREEN_W) / f.width,
                                0.f, float(RS_SCREEN_H) / f.height);
        dw = f.width * s;
        dh = f.height * s;
    }
    r.setTexFilter(m_nearestFilter ? gfx::TexFilter::Nearest
                                   : gfx::TexFilter::Linear);
    r.sprite(m_frameTex, 0, 0, f.width, f.height, (RS_SCREEN_W - dw) / 2.f,
             (RS_SCREEN_H - dh) / 2.f, dw, dh, rsHex(0xFFFFFF));
    r.setTexFilter(gfx::TexFilter::Linear);
}

void GameSession::drawMenu(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float k = ui::easeOutCubic(m_menuFade.t);
    const u32 a = u32(k * 255.f);

    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, rsHex(0x06080C, u32(k * 170.f)));

    constexpr int VISIBLE_ROWS = 5;
    const float px = 16.f, pw = 264.f;
    const float py = 24.f, rowH = 32.f;
    const float ph = rowH * VISIBLE_ROWS + 48.f;
    ui::prim::roundedRect(r, px, py, pw, ph, 8.f,
                          rsWithAlpha(pal.menuBg, a));
    ui::prim::roundedOutline(r, px, py, pw, ph, 8.f,
                             rsWithAlpha(pal.panelOutline, a));

    /* A long ROM title must never escape the pause panel. */
    r.setScissor(int(px + 16.f), int(py + 8.f), int(pw - 32.f), 12);
    fonts.small.draw(r, px + 16.f, py + 8.f, m_game.name.c_str(),
                     rsWithAlpha(pal.textDim, a));
    r.resetScissor();

    const float listY = py + 40.f;
    const float listH = rowH * VISIBLE_ROWS;
    const float hy = listY + (m_menuPos.v - m_menuScroll.v) * rowH;

    /* Keep both the animated focus surface and labels inside the list viewport.
     * During rapid navigation the two smooth values can briefly be more than
     * one row apart, so clipping only the labels allowed the focus surface to
     * bleed onto the dimmed game behind the panel. */
    r.setScissor(int(px + 16.f), int(listY), int(pw - 32.f), int(listH));
    ui::prim::focusRow(r, px + 16.f, hy, pw - 32.f, rowH,
                       rsWithAlpha(pal.tileFocusBg,
                                   rsAlphaOf(pal.tileFocusBg) * a / 255u),
                       rsWithAlpha(pal.accent, a),
                       rsWithAlpha(pal.shadow,
                                   rsAlphaOf(pal.shadow) * a / (255u * 2u)));

    for (int i = 0; i < MENU_COUNT; i++) {
        const float rowY =
            listY + (float(i) - m_menuScroll.v) * rowH;
        if (rowY + rowH <= listY || rowY >= listY + listH) continue;
        const bool sel = i == m_menuRow;
        char label[48];
        if (i == MENU_SAVE || i == MENU_LOAD) {
            std::snprintf(label, sizeof label, "%s  < %d%s >", MENU_LABELS[i],
                          m_slot + 1, m_slots[m_slot].exists ? "" : " ·");
        } else if (i == MENU_ASPECT) {
            std::snprintf(label, sizeof label, "%s  < %s >", MENU_LABELS[i],
                          scaleDisplayName(m_scaleMode));
        } else if (i == MENU_FILTER) {
            std::snprintf(label, sizeof label, "%s  < %s >", MENU_LABELS[i],
                          m_nearestFilter ? "Sharp" : "Smooth");
        } else {
            std::snprintf(label, sizeof label, "%s", MENU_LABELS[i]);
        }
        fonts.body.draw(r, px + 32.f, rowY + 8.f, label,
                        rsWithAlpha(sel ? pal.textPrimary
                                            : pal.textSecondary,
                                    a));
    }
    r.resetScissor();

    /* Slot thumbnail preview beside the panel. */
    if ((m_menuRow == MENU_SAVE || m_menuRow == MENU_LOAD) &&
        m_slots[m_slot].exists) {
        if (m_thumbSlot != m_slot) {
            static u16 thumb[save::THUMB_W * save::THUMB_H];
            if (save::loadThumb(m_game, m_slot, thumb)) {
                if (!m_thumbTex.valid())
                    gfx::Renderer::createTexture(m_thumbTex, save::THUMB_W,
                                                 save::THUMB_H, GU_PSM_5650,
                                                 nullptr, /*dynamic=*/true);
                gfx::Renderer::updateTexture(m_thumbTex, thumb,
                                             save::THUMB_W * 2);
                m_thumbSlot = m_slot;
            }
        }
        if (m_thumbSlot == m_slot && m_thumbTex.valid()) {
            const float tx = px + pw + 16.f, ty = py + 40.f;
            ui::prim::roundedRect(r, tx - 8.f, ty - 8.f,
                                  save::THUMB_W + 16.f,
                                  save::THUMB_H + 36.f,
                                  8.f, rsWithAlpha(pal.menuBg, a));
            r.sprite(m_thumbTex, 0, 0, save::THUMB_W, save::THUMB_H, tx, ty,
                     save::THUMB_W, save::THUMB_H, rsHex(0xFFFFFF, a));
            char cap[24];
            std::snprintf(cap, sizeof cap, "Slot %d", m_slot + 1);
            fonts.small.draw(r, tx + save::THUMB_W / 2.f,
                             ty + save::THUMB_H + 8.f, cap,
                             rsWithAlpha(pal.textDim, a),
                             text::Align::Center);
        }
    }

    const App::Hint hints[] = {
        {ui::prim::Button::Cross, "Select"},
        {ui::prim::Button::Circle, "Resume"},
    };
    r.rect(0.f, 240.f, RS_SCREEN_W, 32.f,
           rsWithAlpha(pal.menuBg, 232u * a / 255u));
    app.drawHintBar(hints, 2);
}

void GameSession::draw(App& app) {
    auto& r = app.renderer();

    if (m_state == State::Failed) {
        app.drawBackground();
        const auto& pal = app.pal();
        app.fonts().large.draw(r, RS_SCREEN_W / 2.f, 100.f, "Launch failed",
                               pal.textPrimary, text::Align::Center);
        app.fonts().body.draw(r, RS_SCREEN_W / 2.f, 130.f, m_error,
                              pal.textSecondary, text::Align::Center);
        app.fonts().small.draw(r, RS_SCREEN_W / 2.f, 156.f,
                               "Press × to return", pal.textDim,
                               text::Align::Center);
        return;
    }
    if (m_state == State::Exiting || !m_cores.loaded()) return;

    /* Letterbox background stays pure black for the game frame. */
    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, rsHex(0x000000));
    drawFrame(app);
    if (m_state == State::Menu) drawMenu(app);
}

}  // namespace rs
