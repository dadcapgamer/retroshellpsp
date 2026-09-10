/* Shared RetroShell pause-menu contract.
 *
 * Keep this header C-compatible: native emulator adapters copy it into their
 * own builds, while the in-process frontend includes it directly. The order
 * is part of the user-facing adapter ABI and must not be rearranged per core.
 */
#ifndef RS_PAUSE_MENU_H
#define RS_PAUSE_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RSPauseMenuItem {
    RS_PAUSE_RESUME = 0,
    RS_PAUSE_SAVE_STATE,
    RS_PAUSE_LOAD_STATE,
    RS_PAUSE_RESET,
    RS_PAUSE_ASPECT_RATIO,
    RS_PAUSE_FILTER,
    RS_PAUSE_SCREENSHOT,
    RS_PAUSE_EMULATOR_SETTINGS,
    RS_PAUSE_EXIT,
    RS_PAUSE_ITEM_COUNT
} RSPauseMenuItem;

#define RS_PAUSE_LABEL_RESUME "Resume"
#define RS_PAUSE_LABEL_SAVE_STATE "Save State"
#define RS_PAUSE_LABEL_LOAD_STATE "Load State"
#define RS_PAUSE_LABEL_RESET "Reset"
#define RS_PAUSE_LABEL_ASPECT_RATIO "Aspect Ratio"
#define RS_PAUSE_LABEL_FILTER "Filter"
#define RS_PAUSE_LABEL_SCREENSHOT "Screenshot"
#define RS_PAUSE_LABEL_EMULATOR_SETTINGS "Emulator Settings"
#define RS_PAUSE_LABEL_EXIT "Exit"

static inline const char* rs_pause_menu_label(RSPauseMenuItem item) {
    static const char* const labels[RS_PAUSE_ITEM_COUNT] = {
        RS_PAUSE_LABEL_RESUME,
        RS_PAUSE_LABEL_SAVE_STATE,
        RS_PAUSE_LABEL_LOAD_STATE,
        RS_PAUSE_LABEL_RESET,
        RS_PAUSE_LABEL_ASPECT_RATIO,
        RS_PAUSE_LABEL_FILTER,
        RS_PAUSE_LABEL_SCREENSHOT,
        RS_PAUSE_LABEL_EMULATOR_SETTINGS,
        RS_PAUSE_LABEL_EXIT,
    };
    return (item >= RS_PAUSE_RESUME && item < RS_PAUSE_ITEM_COUNT)
               ? labels[(int)item]
               : "";
}

#ifdef __cplusplus
}
#endif
#endif
