#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <psploadexec_kernel.h>
#include <systemctrl.h>

#include <stdio.h>
#include <string.h>

#include "retroshell_adapter.h"

static int copy_field(char *destination, size_t capacity, const char *source) {
  size_t length;
  if (!destination || capacity < 2 || !source) return -1;
  length = strlen(source);
  if (length == 0 || length >= capacity) return -1;
  memcpy(destination, source, length + 1);
  return 0;
}

static const char *event_name(RSAdapterEvent event) {
  switch (event) {
    case RS_ADAPTER_LAUNCHED: return "launched";
    case RS_ADAPTER_PAUSED: return "paused";
    case RS_ADAPTER_RESUMED: return "resumed";
    case RS_ADAPTER_RETURNED: return "returned";
    case RS_ADAPTER_RESET: return "reset";
    case RS_ADAPTER_FAILED: return "failed";
  }
  return 0;
}

static int safe_path_field(const char *value, size_t max_length) {
  size_t i, length;
  if (!value) return 0;
  length = strlen(value);
  if (length == 0 || length > max_length) return 0;
  for (i = 0; i < length; ++i) {
    const unsigned char c = (unsigned char)value[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return 0;
  }
  return 1;
}

static int ensure_directory(const char *path) {
  SceUID directory = sceIoDopen(path);
  if (directory >= 0) {
    sceIoDclose(directory);
    return 0;
  }
  if (sceIoMkdir(path, 0777) < 0) return -1;
  directory = sceIoDopen(path);
  if (directory < 0) return -1;
  sceIoDclose(directory);
  return 0;
}

static void recover_rom_hash(RSAdapterContext *context) {
  char receipt[257];
  char *marker;
  FILE *file = fopen(context->session, "rb");
  size_t count;
  unsigned int i;
  memcpy(context->rom_hash, "00000000", 9);
  if (!file) return;
  count = fread(receipt, 1, sizeof(receipt) - 1, file);
  fclose(file);
  receipt[count] = 0;
  marker = strstr(receipt, "\nromHash=");
  if (!marker || strlen(marker + 9) < 8) return;
  marker += 9;
  for (i = 0; i < 8; ++i) {
    const char c = marker[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return;
  }
  memcpy(context->rom_hash, marker, 8);
  context->rom_hash[8] = 0;
}

int rs_adapter_init(RSAdapterContext *context, int argc, char **argv,
                    const char *adapter_name) {
  if (!context) return -1;
  memset(context, 0, sizeof(*context));
  if (copy_field(context->adapter, sizeof(context->adapter), adapter_name) < 0)
    return -1;
  if (argc <= 3 || !argv ||
      copy_field(context->launcher, sizeof(context->launcher), argv[2]) < 0 ||
      copy_field(context->session, sizeof(context->session), argv[3]) < 0)
    return 0;
  if (strcmp(context->session,
             "ms0:/RETROSHELL/session/native-session.ini") != 0) {
    memset(context->launcher, 0, sizeof(context->launcher));
    memset(context->session, 0, sizeof(context->session));
    return -1;
  }
  context->active = 1;
  recover_rom_hash(context);
  return rs_adapter_record(context, RS_ADAPTER_LAUNCHED);
}

int rs_adapter_pause_requested(unsigned int buttons,
                               unsigned int newly_pressed) {
  return (buttons & RS_ADAPTER_PAUSE_MASK) == RS_ADAPTER_PAUSE_MASK &&
         (newly_pressed & PSP_CTRL_SELECT) != 0;
}

int rs_adapter_prepare_save_paths(RSAdapterContext *context,
                                  const char *system_name) {
  char system_directory[160];
  char game_directory[192];
  char states_directory[224];
  int count;
  if (!context || !context->active ||
      !safe_path_field(system_name, 16) ||
      !safe_path_field(context->adapter, 48) ||
      !safe_path_field(context->rom_hash, 8) ||
      strcmp(context->rom_hash, "00000000") == 0)
    return -1;
  if (ensure_directory("ms0:/RETROSHELL") < 0 ||
      ensure_directory("ms0:/RETROSHELL/saves") < 0)
    return -1;
  count = snprintf(system_directory, sizeof(system_directory),
                   "ms0:/RETROSHELL/saves/%s", system_name);
  if (count <= 0 || count >= (int)sizeof(system_directory) ||
      ensure_directory(system_directory) < 0)
    return -1;
  count = snprintf(game_directory, sizeof(game_directory), "%s/%s",
                   system_directory, context->rom_hash);
  if (count <= 0 || count >= (int)sizeof(game_directory) ||
      ensure_directory(game_directory) < 0)
    return -1;
  count = snprintf(states_directory, sizeof(states_directory), "%s/states",
                   game_directory);
  if (count <= 0 || count >= (int)sizeof(states_directory) ||
      ensure_directory(states_directory) < 0)
    return -1;
  count = snprintf(context->save_directory,
                   sizeof(context->save_directory), "%s/", game_directory);
  if (count <= 0 || count >= (int)sizeof(context->save_directory)) return -1;
  count = snprintf(context->state_directory,
                   sizeof(context->state_directory), "%s/%s/",
                   states_directory, context->adapter);
  if (count <= 0 || count >= (int)sizeof(context->state_directory) ||
      ensure_directory(context->state_directory) < 0) {
    context->save_directory[0] = 0;
    context->state_directory[0] = 0;
    return -1;
  }
  return 0;
}

int rs_adapter_record(const RSAdapterContext *context, RSAdapterEvent event) {
  char temporary[160];
  char receipt[256];
  const char *state = event_name(event);
  FILE *file;
  int count;
  if (!context || !context->active || !state) return 0;
  count = snprintf(temporary, sizeof(temporary), "%s.tmp", context->session);
  if (count <= 0 || count >= (int)sizeof(temporary)) return -1;
  count = snprintf(receipt, sizeof(receipt),
      "RETROSHELL_ADAPTER_SESSION=%u\nadapter=%s\nromHash=%s\n"
      "state=%s\npauseHotkey=L+R+SELECT\n",
      (unsigned)RS_ADAPTER_PROTOCOL_VERSION, context->adapter,
      context->rom_hash, state);
  if (count <= 0 || count >= (int)sizeof(receipt)) return -1;
  file = fopen(temporary, "wb");
  if (!file) return -1;
  if (fwrite(receipt, 1, (size_t)count, file) != (size_t)count ||
      fflush(file) != 0 || fclose(file) != 0) {
    sceIoRemove(temporary);
    return -1;
  }
  sceIoRemove(context->session);
  if (sceIoRename(temporary, context->session) < 0) {
    sceIoRemove(temporary);
    return -1;
  }
  sceIoSync("ms0:", 0);
  return 0;
}

int rs_adapter_return(const RSAdapterContext *context, RSAdapterEvent event) {
  struct SceKernelLoadExecVSHParam parameters;
  if (!context || !context->active || context->launcher[0] == 0) return -1;
  rs_adapter_record(context, event);
  memset(&parameters, 0, sizeof(parameters));
  parameters.size = sizeof(parameters);
  parameters.args = strlen(context->launcher) + 1;
  parameters.argp = (void *)context->launcher;
  parameters.key = "game";
  return sctrlKernelLoadExecVSHMs2(context->launcher, &parameters);
}
