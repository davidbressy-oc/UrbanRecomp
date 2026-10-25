#include "sc_gamepad.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned kBits[12] = {
  0x10, 0x20, 0x40, 0x80, 0x04, 0x08,
  0x100, 0x01, 0x200, 0x02, 0x400, 0x800
};
static const char *const kDefaults[12] = {
  "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "Back", "Start",
  "B", "A", "Y", "X", "Lb", "Rb"
};
static char *trim(char *s) {
  while (isspace((unsigned char)*s)) ++s;
  char *end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1])) *--end = 0;
  return s;
}
static bool equal(const char *a, const char *b) {
  while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
  return !*a && !*b;
}
static void controls(ScGamepad *p, char *value) {
  /* Empty tokens are unbound. strtok would shift every following mapping. */
  for (int i = 0; i < 12; ++i) {
    char *comma = strchr(value, ',');
    if (comma) *comma = 0;
    snprintf(p->controls[i], sizeof p->controls[i], "%s", trim(value));
    if (!comma) break;
    value = comma + 1;
  }
}
void ScGamepadLoadMapping(ScGamepad *p) {
  for (int i = 0; i < 12; ++i)
    snprintf(p->controls[i], sizeof p->controls[i], "%s", kDefaults[i]);
  p->deadzone = p->configured_deadzone;
  /* This pinned launcher uses config.ini [GamepadMap] and
   * [Controller.<guid>], NOT the newer input.ini bridge. Its read API is
   * private, so consume its persisted format without changing the submodule. */
  for (int pass = 0; pass < 2; ++pass) {
    char wanted[96];
    if (pass && !p->guid[0]) break;
    snprintf(wanted, sizeof wanted, pass ? "Controller.%s" : "GamepadMap", p->guid);
    FILE *f = fopen(p->config_path, "r");
    if (!f) break;
    char line[1200]; bool active = false;
    while (fgets(line, sizeof line, f)) {
      char *s = trim(line);
      if (!*s || *s == '#' || *s == ';') continue;
      if (*s == '[') {
        char *end = strchr(s, ']');
        if (end) *end = 0;
        active = end && equal(s + 1, wanted);
        continue;
      }
      if (!active) continue;
      char *eq = strchr(s, '=');
      if (!eq) continue;
      *eq++ = 0;
      char *comment = strchr(eq, '#');
      if (comment) *comment = 0;
      if (equal(trim(s), "Controls")) controls(p, eq);
      else if (pass && equal(trim(s), "Deadzone")) {
        int dz = atoi(eq);
        p->deadzone = dz < 0 ? 0 : dz > 100 ? 100 : dz;
      }
    }
    fclose(f);
  }
}
void ScGamepadInit(ScGamepad *p, const char *guid, int deadzone, const char *config) {
  memset(p, 0, sizeof *p);
  snprintf(p->wanted_guid, sizeof p->wanted_guid, "%s", guid ? guid : "");
  snprintf(p->config_path, sizeof p->config_path, "%s", config ? config : "config.ini");
  p->configured_deadzone = deadzone < 0 ? 0 : deadzone > 100 ? 100 : deadzone;
#if SNESRECOMP_SDL3
  p->initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
#else
  p->initialized = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
#endif
  if (!p->initialized) fprintf(stderr, "gamepad: %s\n", SDL_GetError());
  ScGamepadLoadMapping(p);
}
void ScGamepadClose(ScGamepad *p) {
  if (p->handle) {
#if SNESRECOMP_SDL3
    SDL_CloseGamepad(p->handle);
#else
    SDL_GameControllerClose(p->handle);
#endif
    p->handle = NULL;
  }
}
static void discover(ScGamepad *p) {
#if SNESRECOMP_SDL3
  int count = 0;
  SDL_JoystickID *ids = SDL_GetGamepads(&count);
#else
  int count = SDL_NumJoysticks();
#endif
  for (int i = 0; i < count; ++i) {
    char guid[40];
#if SNESRECOMP_SDL3
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(ids[i]), guid, sizeof guid);
#else
    if (!SDL_IsGameController(i)) continue;
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof guid);
#endif
    if (p->wanted_guid[0] && !equal(guid, p->wanted_guid)) continue;
#if SNESRECOMP_SDL3
    p->handle = SDL_OpenGamepad(ids[i]);
#else
    p->handle = SDL_GameControllerOpen(i);
#endif
    if (!p->handle) continue;
    snprintf(p->guid, sizeof p->guid, "%s", guid);
    ScGamepadLoadMapping(p);
    fprintf(stderr, "gamepad: connected GUID %s\n", guid);
    break;
  }
#if SNESRECOMP_SDL3
  SDL_free(ids);
#endif
}
static int axis(ScGamepad *p, int a) {
#if SNESRECOMP_SDL3
  return SDL_GetGamepadAxis(p->handle, (SDL_GamepadAxis)a);
#else
  return SDL_GameControllerGetAxis(p->handle, (SDL_GameControllerAxis)a);
#endif
}
static bool button(ScGamepad *p, int b) {
#if SNESRECOMP_SDL3
  return SDL_GetGamepadButton(p->handle, (SDL_GamepadButton)b);
#else
  return SDL_GameControllerGetButton(p->handle, (SDL_GameControllerButton)b) != 0;
#endif
}
static bool pressed(ScGamepad *p, const char *token, int threshold) {
  /* SDL's standard button/axis enum indices agree across SDL2/3. The
   * vocabulary here is the pinned SNES launcher's capture vocabulary. */
  static const char *const names[] = {
    "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3",
    "Lb", "Rb", "DpadUp", "DpadDown", "DpadLeft", "DpadRight"
  };
  for (int b = 0; b < 15; ++b) if (equal(token, names[b])) return button(p, b);
  if (equal(token, "L1")) return button(p, 9);
  if (equal(token, "R1")) return button(p, 10);
  if (equal(token, "L2")) return axis(p, 4) > threshold;
  if (equal(token, "R2")) return axis(p, 5) > threshold;
  return false;
}
unsigned ScGamepadRead(ScGamepad *p) {
  if (!p->initialized) return 0;
  if (p->handle) {
#if SNESRECOMP_SDL3
    bool connected = SDL_GamepadConnected(p->handle);
#else
    bool connected = SDL_GameControllerGetAttached(p->handle) != 0;
#endif
    if (!connected) {
      ScGamepadClose(p);
      fprintf(stderr, "gamepad: disconnected\n");
    }
  }
  if (!p->handle) discover(p);
  if (!p->handle) return 0;
  unsigned bits = 0;
  int threshold = p->deadzone * 32767 / 100;
  for (int b = 0; b < 12; ++b)
    if (pressed(p, p->controls[b], threshold)) bits |= kBits[b];
  if (p->deadzone == 100) return bits;
  /* Left stick is a second D-pad, with a floor against centre noise. */
  if (threshold < 8000) threshold = 8000;
  int x = axis(p, 0), y = axis(p, 1);
  if (x < -threshold) bits |= 0x40;
  if (x > threshold) bits |= 0x80;
  if (y < -threshold) bits |= 0x10;
  if (y > threshold) bits |= 0x20;
  return bits;
}
