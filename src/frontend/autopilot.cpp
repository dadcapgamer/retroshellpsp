#include "frontend/autopilot.h"

#ifdef RS_AUTOPILOT

#include "frontend/app.h"
#include "frontend/ui/relative_time.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/power.h"
#include "runtime/log.h"

#include <pspctrl.h>
#include <pspiofilemgr.h>

#include <cstdio>

extern volatile bool g_exitRequested;

namespace rs::autopilot {

namespace {

struct Step {
    int frame;
    u32 press;             /* buttons held for this single frame */
    const char* capture;   /* shot name, or nullptr */
};

/* PSP-1000 regression flow: boot → navigate to the configured system → launch →
 * capture sustained gameplay → save/load a state → exit through the
 * frontend menu → verify frontend recovery → relaunch the same core.
 * The one-frame gaps are intentional: Pad::poll must observe a release
 * between navigation presses. */
constexpr Step GAME_SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {229,  PSP_CTRL_CROSS,    nullptr},   /* open the target system */
    {240,  0,                 "list_target"},
    {248,  PSP_CTRL_TRIANGLE, nullptr},   /* selected-game quick actions */
    {260,  0,                 "quick_actions"},
    {264,  PSP_CTRL_DOWN,     nullptr},   /* Favorite */
    {268,  PSP_CTRL_CROSS,    nullptr},
    {272,  PSP_CTRL_CIRCLE,   nullptr},
    {280,  PSP_CTRL_CROSS,    nullptr},   /* Library: X plays (default core) */
    {560,  0,                 "game_a"},
    {820,  0,                 "game_b"},
    {1080, 0,                 "game_c"},
    {1120, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT,
                                    nullptr},   /* open frontend menu */
    {1126, 0,                 "pause_menu"},
    {1130, PSP_CTRL_DOWN,     nullptr},         /* Save state */
    {1140, PSP_CTRL_CROSS,    nullptr},
    {1150, PSP_CTRL_DOWN,     nullptr},         /* Load state */
    {1160, PSP_CTRL_CROSS,    nullptr},
    {1200, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT,
                                    nullptr},
    {1210, PSP_CTRL_DOWN,     nullptr},
    {1214, PSP_CTRL_DOWN,     nullptr},
    {1218, PSP_CTRL_DOWN,     nullptr},
    {1222, PSP_CTRL_DOWN,     nullptr},
    {1226, PSP_CTRL_DOWN,     nullptr},
    {1230, PSP_CTRL_DOWN,     nullptr},
    {1232, 0,                 "pause_fast_scroll"},
    {1234, PSP_CTRL_DOWN,     nullptr},         /* Emulator Settings */
    {1238, PSP_CTRL_DOWN,     nullptr},         /* Exit */
    {1247, PSP_CTRL_CROSS,    nullptr},
    {1290, 0,                 "returned_home"},
    {1298, PSP_CTRL_CIRCLE,   nullptr},         /* Library → Systems */
    {1320, 0,                 "home_recent"},
    {1324, PSP_CTRL_UP,       nullptr},         /* Systems → Continue Playing */
    {1344, 0,                 "recent_focus"},
    {1348, PSP_CTRL_DOWN,     nullptr},         /* Continue → Systems */
    {1396, 0,                 "favorites_home"},
    {1400, PSP_CTRL_CROSS,    nullptr},         /* open Library again */
    {1420, 0,                 "favorites_list"},/* shows the starred game */
    {1424, PSP_CTRL_CIRCLE,   nullptr},
    {1432, PSP_CTRL_TRIANGLE, nullptr},         /* open Settings */
    {1452, 0,                 "settings"},
    {1456, PSP_CTRL_CROSS,    nullptr},         /* into the panel */
    {1464, PSP_CTRL_DOWN,     nullptr},         /* Accent color */
    {1472, 0,                 "accent_picker"},
    {1480, PSP_CTRL_START,    nullptr},         /* return to Systems */
    {1500, PSP_CTRL_CROSS,    nullptr},         /* open Library */
    {1516, PSP_CTRL_CROSS,    nullptr},         /* Library: X relaunches */
    {1718, 0,                 "game_relaunch"},
    /* A state saved in the first session must load in a fresh one: the
     * common real-world case (save, quit, come back, load). */
    {1730, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT,
                                    nullptr},   /* open frontend menu */
    {1740, PSP_CTRL_DOWN,     nullptr},
    {1750, PSP_CTRL_DOWN,     nullptr},         /* Load state */
    {1760, PSP_CTRL_CROSS,    nullptr},
    {1900, 0,                 "relaunch_state"},
};
#if defined(RS_AUTOPILOT_SETUP)
/* First-run setup capture. Mode 1 has games and two emulators for GBC (the
 * dummy test core), so every step appears; mode 2 has an empty Memory Stick
 * and ends on Home's empty state. */
#if RS_AUTOPILOT_SETUP == 2
constexpr Step SETUP_TOUR[] = {
    {70,  0,                 "s01_scan"},
    {110, 0,                 "s02_systems_empty"},
    {120, PSP_CTRL_TRIANGLE, nullptr},          /* finish anyway */
    {180, 0,                 "s03_home_empty"},
    {190, PSP_CTRL_CROSS,    nullptr},          /* scan again */
    {200, 0,                 "s04_home_scanning"},
};
#else
constexpr Step SETUP_TOUR[] = {
    {70,  0,                 "s01_scan"},
    {110, 0,                 "s02_systems"},
    {120, PSP_CTRL_CROSS,    nullptr},
    {160, 0,                 "s03_emulators"},
    {170, PSP_CTRL_DOWN,     nullptr},
    {180, PSP_CTRL_RIGHT,    nullptr},
    {200, 0,                 "s04_emulators_changed"},
    {210, PSP_CTRL_CROSS,    nullptr},
    {250, 0,                 "s05_done"},
    {260, PSP_CTRL_CROSS,    nullptr},
    {320, 0,                 "s06_home"},
};
#endif
constexpr const Step* SCRIPT = SETUP_TOUR;
constexpr int STEPS = int(sizeof(SETUP_TOUR) / sizeof(SETUP_TOUR[0]));
#elif defined(RS_AUTOPILOT_TOUR)
/* UI tour: walks every Home layer with a pre-seeded library so each redesign
 * screen can be captured without launching a core. Gaps leave room for the
 * layer/slide animations (and PPSSPP's software renderer) to settle. */
constexpr Step TOUR[] = {
    {60,  0,                 "t01_systems"},
    {70,  PSP_CTRL_RIGHT,    nullptr},
    {110, 0,                 "t02_systems_gbc"},
    {120, PSP_CTRL_UP,       nullptr},          /* Continue Playing */
    {170, 0,                 "t03_continue"},
    {180, PSP_CTRL_RIGHT,    nullptr},
    {192, PSP_CTRL_RIGHT,    nullptr},
    {240, 0,                 "t04_continue_third"},
    {250, PSP_CTRL_DOWN,     nullptr},          /* back to Systems */
    {290, PSP_CTRL_CROSS,    nullptr},          /* GBC Library */
    {340, 0,                 "t05_library_gbc"},
    {350, PSP_CTRL_DOWN,     nullptr},
    {360, PSP_CTRL_DOWN,     nullptr},
    {410, 0,                 "t06_library_moved"},
    {420, PSP_CTRL_TRIANGLE, nullptr},          /* Options ... */
    {428, PSP_CTRL_DOWN,     nullptr},
    {436, PSP_CTRL_DOWN,     nullptr},
    {444, PSP_CTRL_CROSS,    nullptr},          /* ... Game Details */
    {465, 0,                 "t07_detail"},
    {470, PSP_CTRL_DOWN,     nullptr},
    {478, PSP_CTRL_DOWN,     nullptr},
    {484, PSP_CTRL_DOWN,     nullptr},
    {490, PSP_CTRL_CROSS,    nullptr},          /* expand details */
    {530, 0,                 "t08_detail_expanded"},
    {540, PSP_CTRL_CIRCLE,   nullptr},
    {550, PSP_CTRL_CIRCLE,   nullptr},          /* back to Library */
    {590, 0,                 "t09_library_back"},
    {600, PSP_CTRL_RIGHT,    nullptr},          /* GBA Library, in place */
    {640, 0,                 "t10_library_gba"},
    {650, PSP_CTRL_SQUARE,   nullptr},          /* favorite */
    {670, 0,                 "t11_favorite"},
    {680, PSP_CTRL_TRIANGLE, nullptr},          /* options */
    {710, 0,                 "t12_options"},
    {720, PSP_CTRL_DOWN,     nullptr},
    {730, PSP_CTRL_DOWN,     nullptr},
    {740, PSP_CTRL_DOWN,     nullptr},
    {750, PSP_CTRL_DOWN,     nullptr},
    {760, PSP_CTRL_DOWN,     nullptr},
    {770, PSP_CTRL_DOWN,     nullptr},
    {780, PSP_CTRL_DOWN,     nullptr},
    {820, 0,                 "t13_options_back_row"},
    {830, PSP_CTRL_CIRCLE,   nullptr},          /* close popup */
    {840, PSP_CTRL_LTRIGGER, nullptr},          /* shoulder: previous system */
    {880, 0,                 "t14_library_shoulder"},
    {890, PSP_CTRL_CIRCLE,   nullptr},          /* Systems */
    {930, 0,                 "t15_systems_back"},
    {940, PSP_CTRL_TRIANGLE, nullptr},          /* Settings */
    {980, 0,                 "t16_settings"},

    /* Maturity pass: error state, view menu, filters, sort, search. The
     * seeded GBC library puts a ROM that is deleted at frame 985 under the
     * cursor, so Play reaches the "ROM file not found" state. */
    {990,  PSP_CTRL_CIRCLE,   nullptr},         /* Systems (GBC) */
    {1030, PSP_CTRL_CROSS,    nullptr},         /* Library */
    {1060, PSP_CTRL_TRIANGLE, nullptr},         /* Options -> Game Details */
    {1066, PSP_CTRL_DOWN,     nullptr},
    {1072, PSP_CTRL_DOWN,     nullptr},
    {1078, PSP_CTRL_CROSS,    nullptr},
    {1096, 0,                 "t17_detail_clean"},
    {1100, PSP_CTRL_CROSS,    nullptr},         /* Play -> missing ROM */
    {1140, 0,                 "t18_error_rom_missing"},
    {1150, PSP_CTRL_TRIANGLE, nullptr},         /* Rescan Library */
    {1160, PSP_CTRL_CIRCLE,   nullptr},         /* Detail -> Library */
    {1220, 0,                 "t19_library_clean"},
    {1230, PSP_CTRL_SELECT,   nullptr},         /* View menu */
    {1260, 0,                 "t20_view_menu"},
    {1270, PSP_CTRL_RIGHT,    nullptr},         /* Show: Favorites */
    {1300, 0,                 "t21_view_favorites"},
    {1310, PSP_CTRL_CIRCLE,   nullptr},
    {1340, 0,                 "t22_favorites_list"},
    {1350, PSP_CTRL_SELECT,   nullptr},
    {1360, PSP_CTRL_RIGHT,    nullptr},         /* Show: Recently Added */
    {1370, PSP_CTRL_CIRCLE,   nullptr},
    {1400, 0,                 "t23_recently_added"},
    {1410, PSP_CTRL_SELECT,   nullptr},
    {1420, PSP_CTRL_RIGHT,    nullptr},         /* Show: All Games */
    {1430, PSP_CTRL_DOWN,     nullptr},         /* Sort by */
    {1440, PSP_CTRL_RIGHT,    nullptr},         /* Z-A */
    {1450, PSP_CTRL_RIGHT,    nullptr},         /* Recently Played */
    {1460, PSP_CTRL_RIGHT,    nullptr},         /* Most Played */
    {1490, 0,                 "t24_sort_menu"},
    {1500, PSP_CTRL_CIRCLE,   nullptr},
    {1530, 0,                 "t25_most_played"},
    {1540, PSP_CTRL_SELECT,   nullptr},
    {1550, PSP_CTRL_DOWN,     nullptr},
    {1560, PSP_CTRL_DOWN,     nullptr},         /* Search row */
    {1570, PSP_CTRL_CROSS,    nullptr},
    {1580, PSP_CTRL_UP,       nullptr},         /* B */
    {1590, PSP_CTRL_RIGHT,    nullptr},         /* lock B */
    {1600, PSP_CTRL_RTRIGGER, nullptr},
    {1610, PSP_CTRL_RTRIGGER, nullptr},         /* L */
    {1640, 0,                 "t26_search_entry"},
    {1650, PSP_CTRL_CROSS,    nullptr},         /* apply "BL" */
    {1690, 0,                 "t27_search_results"},
    {1700, PSP_CTRL_SELECT,   nullptr},
    {1710, PSP_CTRL_DOWN,     nullptr},
    {1720, PSP_CTRL_DOWN,     nullptr},
    {1730, PSP_CTRL_DOWN,     nullptr},         /* Clear search */
    {1740, PSP_CTRL_CROSS,    nullptr},
    {1750, PSP_CTRL_CIRCLE,   nullptr},
    {1756, PSP_CTRL_TRIANGLE, nullptr},         /* Detail of top game */
    {1762, PSP_CTRL_DOWN,     nullptr},
    {1768, PSP_CTRL_DOWN,     nullptr},
    {1774, PSP_CTRL_CROSS,    nullptr},
    {1790, PSP_CTRL_DOWN,     nullptr},
    {1798, PSP_CTRL_DOWN,     nullptr},
    {1804, PSP_CTRL_DOWN,     nullptr},
    {1810, PSP_CTRL_CROSS,    nullptr},         /* expand */
    {1850, 0,                 "t28_detail_stats"},
    {1860, PSP_CTRL_CIRCLE,   nullptr},         /* collapse stats */
    {1870, PSP_CTRL_CIRCLE,   nullptr},         /* Library */
    /* Real-world names and the artwork fallbacks: search "POK" (sibling art
     * and metadata for Pokemon Crystal), then "MET" (no cover, but a player
     * screenshot exists). */
    {1880, PSP_CTRL_SELECT,   nullptr},
    {1890, PSP_CTRL_DOWN,     nullptr},
    {1900, PSP_CTRL_DOWN,     nullptr},
    {1910, PSP_CTRL_CROSS,    nullptr},
    {1920, PSP_CTRL_RTRIGGER, nullptr},
    {1930, PSP_CTRL_RTRIGGER, nullptr},
    {1940, PSP_CTRL_RTRIGGER, nullptr},         /* P */
    {1950, PSP_CTRL_RIGHT,    nullptr},
    {1960, PSP_CTRL_DOWN,     nullptr},         /* O */
    {1970, PSP_CTRL_RIGHT,    nullptr},
    {1980, PSP_CTRL_LTRIGGER, nullptr},
    {1990, PSP_CTRL_UP,       nullptr},         /* K */
    {2000, PSP_CTRL_CROSS,    nullptr},
    {2060, 0,                 "t29_pokemon_list"},
    {2064, PSP_CTRL_TRIANGLE, nullptr},         /* Detail */
    {2070, PSP_CTRL_DOWN,     nullptr},
    {2076, PSP_CTRL_DOWN,     nullptr},
    {2082, PSP_CTRL_CROSS,    nullptr},
    {2110, 0,                 "t30_pokemon_detail"},
    {2120, PSP_CTRL_CIRCLE,   nullptr},
    {2130, PSP_CTRL_SELECT,   nullptr},
    {2140, PSP_CTRL_DOWN,     nullptr},
    {2150, PSP_CTRL_DOWN,     nullptr},
    {2160, PSP_CTRL_CROSS,    nullptr},
    {2170, PSP_CTRL_SQUARE,   nullptr},         /* clear buffer */
    {2180, PSP_CTRL_RTRIGGER, nullptr},
    {2190, PSP_CTRL_RTRIGGER, nullptr},
    {2200, PSP_CTRL_UP,       nullptr},
    {2210, PSP_CTRL_UP,       nullptr},         /* M */
    {2220, PSP_CTRL_RIGHT,    nullptr},
    {2230, PSP_CTRL_LTRIGGER, nullptr},
    {2240, PSP_CTRL_DOWN,     nullptr},
    {2250, PSP_CTRL_DOWN,     nullptr},
    {2260, PSP_CTRL_DOWN,     nullptr},         /* E */
    {2270, PSP_CTRL_RIGHT,    nullptr},
    {2280, PSP_CTRL_RTRIGGER, nullptr},
    {2290, PSP_CTRL_RTRIGGER, nullptr},
    {2300, PSP_CTRL_RTRIGGER, nullptr},         /* T */
    {2310, PSP_CTRL_CROSS,    nullptr},
    {2380, 0,                 "t31_screenshot_fallback"},
    /* Polish pass: Settings categories, the Continue options popup, and
     * text-only mode. */
    {2390, PSP_CTRL_CIRCLE,   nullptr},         /* Systems */
    {2430, PSP_CTRL_TRIANGLE, nullptr},         /* Settings */
    {2440, PSP_CTRL_RTRIGGER, nullptr},         /* Systems */
    {2456, PSP_CTRL_RTRIGGER, nullptr},         /* Performance */
    {2480, 0,                 "t32_settings_performance"},
    {2490, PSP_CTRL_RTRIGGER, nullptr},
    {2500, PSP_CTRL_RTRIGGER, nullptr},
    {2510, PSP_CTRL_RTRIGGER, nullptr},         /* About */
    {2540, 0,                 "t33_settings_about"},
    {2550, PSP_CTRL_LTRIGGER, nullptr},
    {2560, PSP_CTRL_LTRIGGER, nullptr},
    {2570, PSP_CTRL_LTRIGGER, nullptr},
    {2580, PSP_CTRL_LTRIGGER, nullptr},
    {2585, PSP_CTRL_LTRIGGER, nullptr},         /* Appearance */
    {2590, PSP_CTRL_CROSS,    nullptr},         /* into the panel */
    {2600, PSP_CTRL_DOWN,     nullptr},
    {2610, PSP_CTRL_DOWN,     nullptr},
    {2620, PSP_CTRL_DOWN,     nullptr},         /* Artwork */
    {2630, 0,                 "t34_settings_panel"},
    {2640, PSP_CTRL_RIGHT,    nullptr},         /* Text only */
    {2690, 0,                 "t35_settings_textonly"},
    {2700, PSP_CTRL_CIRCLE,   nullptr},         /* sidebar */
    {2710, PSP_CTRL_CIRCLE,   nullptr},         /* Systems */
    {2740, PSP_CTRL_CROSS,    nullptr},         /* Library, text only */
    {2780, 0,                 "t36_library_textonly"},
    {2790, PSP_CTRL_CIRCLE,   nullptr},
    {2800, PSP_CTRL_UP,       nullptr},         /* Continue */
    {2840, PSP_CTRL_TRIANGLE, nullptr},
    {2870, 0,                 "t37_continue_options"},
    {2880, PSP_CTRL_CIRCLE,   nullptr},
    {2890, PSP_CTRL_DOWN,     nullptr},         /* Systems */
    {2900, PSP_CTRL_TRIANGLE, nullptr},         /* Settings: artwork back on */
    {2930, PSP_CTRL_CROSS,    nullptr},
    {2940, PSP_CTRL_DOWN,     nullptr},
    {2950, PSP_CTRL_DOWN,     nullptr},
    {2960, PSP_CTRL_DOWN,     nullptr},
    {2970, PSP_CTRL_RIGHT,    nullptr},
    {2980, PSP_CTRL_START,    nullptr},
    /* Save States in Game Detail (the runner seeds slots 1 and 3, slot 3 in
     * the older thumbnail-only format), then Settings → Systems. */
    {3020, PSP_CTRL_CROSS,    nullptr},         /* Library */
    {3050, PSP_CTRL_TRIANGLE, nullptr},
    {3058, PSP_CTRL_DOWN,     nullptr},
    {3066, PSP_CTRL_DOWN,     nullptr},
    {3074, PSP_CTRL_CROSS,    nullptr},         /* Game Details */
    {3100, 0,                 "t38_detail_states_row"},
    {3110, PSP_CTRL_DOWN,     nullptr},         /* Save States */
    {3118, PSP_CTRL_CROSS,    nullptr},
    {3150, 0,                 "t39_states"},
    {3160, PSP_CTRL_DOWN,     nullptr},
    {3168, PSP_CTRL_DOWN,     nullptr},         /* slot 3: legacy thumbnail */
    {3200, 0,                 "t40_states_legacy"},
    {3210, PSP_CTRL_DOWN,     nullptr},         /* slot 4: empty */
    {3240, 0,                 "t41_states_empty"},
    {3250, PSP_CTRL_START,    nullptr},         /* Home */
    {3290, PSP_CTRL_TRIANGLE, nullptr},         /* Settings */
    {3300, PSP_CTRL_DOWN,     nullptr},         /* Systems */
    {3330, 0,                 "t42_settings_systems"},
    {3340, PSP_CTRL_CROSS,    nullptr},
    {3350, PSP_CTRL_RIGHT,    nullptr},         /* Game Boy: next emulator */
    {3380, 0,                 "t43_settings_systems_change"},
    {3390, PSP_CTRL_LEFT,     nullptr},
    {3400, PSP_CTRL_START,    nullptr},
};
constexpr const Step* SCRIPT = TOUR;
constexpr int STEPS = int(sizeof(TOUR) / sizeof(TOUR[0]));
#else
constexpr const Step* SCRIPT = GAME_SCRIPT;
constexpr int STEPS = int(sizeof(GAME_SCRIPT) / sizeof(GAME_SCRIPT[0]));
#endif

int  s_frame = 0;
bool s_dirReady = false;
bool s_done = false;
constexpr int TARGET_SYSTEM = RS_AUTOPILOT_SYSTEM_INDEX;
constexpr int NAV_FIRST_FRAME = 165;
constexpr int NAV_FRAME_GAP = 8;
int s_railPresses = -1;   /* Right presses from the first rail system */

/* The rail lists only systems that have games, so the target's slot is the
 * number of populated systems before it. Counted once, when navigation
 * begins, because the background scan may still be filling the index. */
int railPressesFor(App& app, int target) {
    int slot = 0;
    for (int s = 0; s < target; s++)
        if (!app.index().games(db::System(s)).empty()) slot++;
    return slot;
}

#ifdef RS_AUTOPILOT_TOUR
/* Civil date from days since 1970-01-01 (Howard Hinnant). */
void civilFromDays(long long z, int& y, int& m, int& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    d = int(doy - (153 * mp + 2) / 5 + 1);
    m = int(mp < 10 ? mp + 3 : mp - 9);
    y = int(yoe + era * 400 + (m <= 2));
}

/* YYYYMMDDHHMM stamp `minutesBack` before `now`. */
u64 stampBefore(u64 now, long long minutesBack) {
    s64 total = 0;
    ui::detail::stampMinutes(now, total);
    total -= minutesBack;
    s64 days = total / (24 * 60);
    s64 rem = total % (24 * 60);
    if (rem < 0) { rem += 24 * 60; days--; }
    int y, m, d;
    civilFromDays(days, y, m, d);
    return u64(y) * 100000000ull + u64(m) * 1000000ull + u64(d) * 10000ull +
           u64(rem / 60) * 100ull + u64(rem % 60);
}

/* Gives the seeded GBC library a history so the sort modes and the detail
 * view have something to show: two favorites and three played games. */
void seedHistory(App& app) {
    const auto& games = app.index().games(db::System::GameBoyColor);
    if (games.size() < 8) return;
    const u64 now = power::localTimestamp();
    app.library().toggleFavorite(games[3].pathHash);
    app.library().toggleFavorite(games[6].pathHash);
    for (int i = 0; i < 4; i++)
        app.library().notePlayed(games[5].pathHash, stampBefore(now, 3000));
    for (int i = 0; i < 2; i++)
        app.library().notePlayed(games[7].pathHash, stampBefore(now, 900));
    app.library().notePlayed(games[4].pathHash, stampBefore(now, 60));
    app.library().addPlaytime(games[5].pathHash, 3 * 3600 + 5 * 60);
    app.library().addPlaytime(games[7].pathHash, 42 * 60);
    app.library().addPlaytime(games[4].pathHash, 25);
}

/* Seeds Continue Playing: the first game of five systems, most recent last
 * so notePlayed leaves GBC at the front. */
void seedRecents(App& app) {
    static const int ORDER[] = {4, 3, 2, 0, 1};                  /* SNES..GBC */
    static const long long AGO[] = {5 * 1440, 3 * 1440, 1440, 130, 20};
    const u64 now = power::localTimestamp();
    for (int i = 0; i < 5; i++) {
        const auto& games = app.index().games(db::System(ORDER[i]));
        if (games.empty()) continue;
        app.library().notePlayed(games[0].pathHash, stampBefore(now, AGO[i]));
    }
}
#endif

}  // namespace

void tick(App& app) {
    if (s_done) {
        s_frame++;
        if (s_frame > SCRIPT[STEPS - 1].frame + 90)
            g_exitRequested = true;
        return;
    }
    if (!s_dirReady) {
        sceIoMkdir(fs::ROOT, 0777);
        sceIoMkdir("ms0:/RETROSHELL/shots", 0777);
        s_dirReady = true;
        RS_LOGI("autopilot: engaged");
    }

#ifdef RS_AUTOPILOT_TOUR
    if (s_frame == 20) {
        seedHistory(app);
        seedRecents(app);
    }
    /* The ROM under the Library cursor vanishes after the scan, as if the
     * user had deleted it from the Memory Stick. */
    if (s_frame == 985) {
        for (const auto& g : app.index().games(db::System::GameBoyColor))
            if (g.name.rfind("Aaa Missing", 0) == 0) {
                fs::removeFile(g.path.c_str());
                RS_LOGI("autopilot: removed %s", g.path.c_str());
            }
    }
#endif
    for (int i = 0; i < STEPS; i++) {
        if (SCRIPT[i].frame != s_frame) continue;
        if (SCRIPT[i].press) app.padMutable().simulate(SCRIPT[i].press);
        if (SCRIPT[i].capture) {
            char path[128];
            std::snprintf(path, sizeof path, "ms0:/RETROSHELL/shots/%s.png",
                          SCRIPT[i].capture);
            app.renderer().requestCapture(path);
            RS_LOGI("autopilot: capture %s (app time %d ms)",
                    SCRIPT[i].capture, int(app.time() * 1000.f));
        }
    }

#ifndef RS_AUTOPILOT_TOUR
    /* Home starts on the first populated system. Move right once per
     * populated system before the target. Keeping releases between presses
     * makes this deterministic on both hardware and PPSSPP. */
    if (s_frame == NAV_FIRST_FRAME - 1)
        s_railPresses = railPressesFor(app, TARGET_SYSTEM);
    for (int i = 0; i < s_railPresses; i++) {
        if (s_frame == NAV_FIRST_FRAME + i * NAV_FRAME_GAP)
            app.padMutable().simulate(PSP_CTRL_RIGHT);
    }
#endif

    s_frame++;
    if (s_frame > SCRIPT[STEPS - 1].frame + 30) {
        s_done = true;
        RS_LOGI("autopilot: complete (system index %d)", TARGET_SYSTEM);
    }
}

}  // namespace rs::autopilot

#endif  /* RS_AUTOPILOT */
