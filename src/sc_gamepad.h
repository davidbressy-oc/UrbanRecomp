#ifndef SC_GAMEPAD_H
#define SC_GAMEPAD_H
#include "sc_sdl_compat.h"
#include <stdbool.h>
/* Owns one SDL controller. No writes to launcher files or SDL mappings. */
typedef struct ScGamepad {
#if SNESRECOMP_SDL3
  SDL_Gamepad *handle;
#else
  SDL_GameController *handle;
#endif
  char wanted_guid[40], guid[40];
  char controls[12][48]; /* launcher's SNES config.ini order */
  char config_path[1024];
  int deadzone, configured_deadzone;
  bool initialized;
} ScGamepad;
void ScGamepadInit(ScGamepad *pad, const char *guid, int deadzone, const char *config);
unsigned ScGamepadRead(ScGamepad *pad);
void ScGamepadClose(ScGamepad *pad);
/* Also used by ROM-free tests; reloads global map, then the selected profile. */
void ScGamepadLoadMapping(ScGamepad *pad);
#endif
