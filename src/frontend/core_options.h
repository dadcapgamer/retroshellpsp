/** Emulator settings offered in the pause menu, per core.
 *
 * A curated subset of each libretro core's own options: the ones that matter
 * on a PSP (speed and sound cost first, then a visual choice or two). Values
 * are the cores' own option strings; they are stored per game through
 * cfg::setGameOption and reach the core through RSHostAPI::get_option the
 * next time the game starts. `def` is the effective PSP default — where the
 * shim or core substitutes a PSP default, that is the value listed here, not
 * the desktop default.
 *
 * Frame-skip options are deliberately absent: RetroShell's own frame pacing
 * and audio recovery already decide when video is skipped.
 */
#pragma once

#include <cstring>

namespace rs::coreopt {

struct Value {
    const char* value;    /* the core's option string */
    const char* label;    /* what the menu shows */
};

struct Option {
    const char* core;     /* CoreInfo::name */
    const char* key;
    const char* label;
    const char* def;
    const Value* values;
    int count;
};

namespace detail {
constexpr Value PD_RENDERER[] = {
    {"accurate", "Accurate"}, {"good", "Good"}, {"fast", "Fast"}};
constexpr Value PD_RATE[] = {
    {"16000", "16 kHz"}, {"22050", "22 kHz"}, {"32000", "32 kHz"},
    {"44100", "44 kHz"}};
constexpr Value SPRITE_LIMIT[] = {
    {"disabled", "Original"}, {"enabled", "Removed"}};
constexpr Value OFF_ON[] = {{"disabled", "Off"}, {"enabled", "On"}};
constexpr Value GPSP_RATE[] = {{"32768", "32 kHz"}, {"65536", "65 kHz"}};
constexpr Value QN_PALETTE[] = {
    {"default", "Default"}, {"nintendo-vc", "Virtual Console"},
    {"rgb", "RGB"}, {"pal", "PAL"}};
constexpr Value GB_COLOR[] = {
    {"disabled", "Off"}, {"auto", "Auto"}, {"GBC", "GBC"}, {"SGB", "SGB"}};
constexpr Value GBC_CORRECT[] = {
    {"GBC only", "GBC only"}, {"always", "Always"}, {"disabled", "Off"}};
constexpr Value SMS_FM[] = {{"auto", "Auto"}, {"disabled", "Off"}};

#define RS_N(a) int(sizeof(a) / sizeof((a)[0]))
constexpr Option ALL[] = {
    {"picodrive", "picodrive_renderer", "Renderer", "good", PD_RENDERER,
     RS_N(PD_RENDERER)},
    {"picodrive", "picodrive_sound_rate", "Sound quality", "32000", PD_RATE,
     RS_N(PD_RATE)},
    {"picodrive", "picodrive_sprlim", "Sprite limit", "disabled", SPRITE_LIMIT,
     RS_N(SPRITE_LIMIT)},
    {"gpsp", "gpsp_sound_rate", "Sound quality", "32768", GPSP_RATE,
     RS_N(GPSP_RATE)},
    {"gpsp", "gpsp_color_correction", "Color correction", "disabled", OFF_ON,
     RS_N(OFF_ON)},
    {"snes9x2005", "snes9x_2005_low_pass_filter", "Low-pass filter",
     "disabled", OFF_ON, RS_N(OFF_ON)},
    {"snes9x2005", "snes9x_2005_reduce_sprite_flicker", "Reduce flicker",
     "disabled", OFF_ON, RS_N(OFF_ON)},
    {"quicknes", "quicknes_no_sprite_limit", "Sprite limit", "disabled",
     SPRITE_LIMIT, RS_N(SPRITE_LIMIT)},
    {"quicknes", "quicknes_palette", "Palette", "default", QN_PALETTE,
     RS_N(QN_PALETTE)},
    {"pcefast", "pce_fast_nospritelimit", "Sprite limit", "disabled",
     SPRITE_LIMIT, RS_N(SPRITE_LIMIT)},
    {"gambatte", "gambatte_gb_colorization", "GB colorization", "disabled",
     GB_COLOR, RS_N(GB_COLOR)},
    {"gambatte", "gambatte_gbc_color_correction", "GBC color correction",
     "GBC only", GBC_CORRECT, RS_N(GBC_CORRECT)},
    {"smsplus", "smsplus_fm_sound", "FM sound", "auto", SMS_FM, RS_N(SMS_FM)},
};
#undef RS_N
}  // namespace detail

constexpr int MAX_PER_CORE = 4;

/* The options offered for `core`, in menu order. Returns the count. */
inline int forCore(const char* core, const Option* out[MAX_PER_CORE]) {
    int n = 0;
    for (const Option& o : detail::ALL)
        if (n < MAX_PER_CORE && std::strcmp(o.core, core) == 0) out[n++] = &o;
    return n;
}

/* Index of `value` in the option's list, or of its default when unknown. */
inline int indexOf(const Option& o, const char* value) {
    for (int pass = 0; pass < 2; pass++) {
        const char* want = pass == 0 ? value : o.def;
        if (!want || !*want) continue;
        for (int i = 0; i < o.count; i++)
            if (std::strcmp(o.values[i].value, want) == 0) return i;
    }
    return 0;
}

}  // namespace rs::coreopt
