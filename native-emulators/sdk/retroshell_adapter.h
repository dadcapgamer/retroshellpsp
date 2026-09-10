/* RetroShell native-emulator adapter protocol v1.
 *
 * The emulator remains a standalone PSP process. This tiny shim only owns
 * launch arguments, the standard pause chord, session receipts, and the
 * chain-load back to RetroShell. It never allocates during gameplay. */
#ifndef RETROSHELL_ADAPTER_H
#define RETROSHELL_ADAPTER_H

#include <stddef.h>
#include <pspctrl.h>
#include "rs_pause_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RS_ADAPTER_PROTOCOL_VERSION 1
#define RS_ADAPTER_PAUSE_MASK (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)

typedef enum RSAdapterEvent {
  RS_ADAPTER_LAUNCHED,
  RS_ADAPTER_PAUSED,
  RS_ADAPTER_RESUMED,
  RS_ADAPTER_RETURNED,
  RS_ADAPTER_RESET,
  RS_ADAPTER_FAILED
} RSAdapterEvent;

typedef struct RSAdapterContext {
  char adapter[49];
  char launcher[256];
  char session[128];
  char rom_hash[9];
  char save_directory[256];
  char state_directory[256];
  int active;
} RSAdapterContext;

/* RetroShell passes ROM, launcher, and receipt as argv[1..3]. Starting the
 * emulator directly remains supported and produces an inactive context. */
int rs_adapter_init(RSAdapterContext *context, int argc, char **argv,
                    const char *adapter_name);
int rs_adapter_pause_requested(unsigned int buttons,
                               unsigned int newly_pressed);
int rs_adapter_record(const RSAdapterContext *context, RSAdapterEvent event);

/* Creates and returns RetroShell-owned directories for this game. Battery
 * saves live in save_directory; emulator-specific states live beneath
 * states/<adapter>/ so incompatible formats can never overwrite each other.
 * Both returned paths include a trailing slash. */
int rs_adapter_prepare_save_paths(RSAdapterContext *context,
                                  const char *system_name);

/* Records `event` then replaces the emulator with RetroShell. Returns only
 * when no launcher was supplied or custom firmware rejects the loadexec. */
int rs_adapter_return(const RSAdapterContext *context, RSAdapterEvent event);

#ifdef __cplusplus
}
#endif
#endif
