#include "sc_gamepad.h"
#include "sc_launcher.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void mapping(const char *path) {
  FILE *f = fopen(path, "w"); assert(f);
  fputs("[Controller.abc]\nControls=, A, R2\nDeadzone=25\n"
        "[GamepadMap]\nControls=DpadUp,DpadDown,DpadLeft,DpadRight,Back,Start,B,A,Y,X,Lb,Rb\n", f);
  fclose(f);
  ScGamepad p = {0};
  snprintf(p.config_path, sizeof p.config_path, "%s", path);
  strcpy(p.guid, "abc"); p.configured_deadzone = 15;
  ScGamepadLoadMapping(&p);
  assert(!p.controls[0][0]);
  assert(!strcmp(p.controls[1], "A") && !strcmp(p.controls[2], "R2"));
  assert(!strcmp(p.controls[3], "DpadRight") && p.deadzone == 25);
  strcpy(p.guid, "other"); ScGamepadLoadMapping(&p);
  assert(!strcmp(p.controls[0], "DpadUp") && p.deadzone == 15);
  remove(path);
}
static void settings(const char *path) {
  ScSettings s, loaded;
  ScSettingsDefaults(&s);
  assert(s.player1_source == 1);
  s.player1_source = 2; s.player1_deadzone = 23;
  strcpy(s.player1_guid, "030000004c050000e60c000000000000");
  assert(ScSettingsSave(&s, path) && ScSettingsLoad(&loaded, path));
  assert(loaded.player1_source == 2 && loaded.player1_deadzone == 23);
  assert(!strcmp(loaded.player1_guid, s.player1_guid));
  assert(loaded.sylt == s.sylt && loaded.widescreen == s.widescreen);
  FILE *legacy = fopen(path, "w"); assert(legacy);
  fputs("[Game]\nsylt=0\n", legacy); fclose(legacy);
  assert(ScSettingsLoad(&loaded, path));
  assert(loaded.player1_source == 1 && loaded.player1_guid[0] == 0);
  assert(loaded.player1_deadzone == 15 && loaded.sylt == 0);
  remove(path);
}
static void pump(void) { SDL_PumpEvents(); }
int main(int argc, char **argv) {
  assert(argc == 2);
  mapping(argv[1]); settings(argv[1]);
#if SNESRECOMP_SDL3
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = 6; desc.nbuttons = 21;
  SDL_JoystickID device = SDL_AttachVirtualJoystick(&desc);
  assert(device != 0);
  SDL_Joystick *joystick = SDL_OpenJoystick(device);
#define SET_BUTTON(b,v) SDL_SetJoystickVirtualButton(joystick,b,v)
#define SET_AXIS(a,v) SDL_SetJoystickVirtualAxis(joystick,a,v)
#else
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0);
  int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
  assert(device >= 0);
  SDL_Joystick *joystick = SDL_JoystickOpen(device);
#define SET_BUTTON(b,v) SDL_JoystickSetVirtualButton(joystick,b,v)
#define SET_AXIS(a,v) SDL_JoystickSetVirtualAxis(joystick,a,v)
#endif
  assert(joystick);
  ScGamepad p;
  ScGamepadInit(&p, "", 15, argv[1]);
  pump(); assert(ScGamepadRead(&p) == 0 && p.handle);
  const unsigned expected[] = {1,0x100,2,0x200,4,0,8,0,0,0x400,0x800,0x10,0x20,0x40,0x80};
  for (int b=0; b<15; ++b) {
    SET_BUTTON(b, 1); pump(); assert(ScGamepadRead(&p) == expected[b]);
    SET_BUTTON(b, 0); pump(); assert(ScGamepadRead(&p) == 0);
  }
  SET_AXIS(0, -32768); p.deadzone = 100;
  pump(); assert(ScGamepadRead(&p) == 0);
  p.deadzone = 15;
  SET_AXIS(0, -24000); SET_AXIS(1, 24000);
  pump(); assert(ScGamepadRead(&p) == (0x40 | 0x20));
  SET_AXIS(0, 1000); SET_AXIS(1, 0); pump(); assert(ScGamepadRead(&p) == 0);
  /* The actual launcher's captured trigger tokens, including cleared slots. */
  FILE *f = fopen(argv[1], "w"); assert(f);
  fputs("[GamepadMap]\nControls=,,,,,,R2,L2,,,,\n", f); fclose(f);
  ScGamepadLoadMapping(&p);
  SET_AXIS(4, 32767); pump(); assert(ScGamepadRead(&p) == 0x01);
  SET_AXIS(5, 32767); pump(); assert(ScGamepadRead(&p) == 0x101);
#if SNESRECOMP_SDL3
  SDL_CloseJoystick(joystick);
  assert(SDL_DetachVirtualJoystick(device));
#else
  SDL_JoystickClose(joystick);
  assert(SDL_JoystickDetachVirtual(device) == 0);
#endif
  pump(); assert(ScGamepadRead(&p) == 0 && !p.handle);
  /* A new connection has a new instance id but the same hardware GUID. */
  strcpy(p.wanted_guid, p.guid);
  remove(argv[1]);
#if SNESRECOMP_SDL3
  device = SDL_AttachVirtualJoystick(&desc);
  assert(device != 0);
  joystick = SDL_OpenJoystick(device);
#else
  device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
  assert(device >= 0);
  joystick = SDL_JoystickOpen(device);
#endif
  assert(joystick);
  pump(); assert(ScGamepadRead(&p) == 0 && p.handle);
  SET_BUTTON(1, 1); pump(); assert(ScGamepadRead(&p) == 0x100);
  ScGamepadClose(&p);
#if SNESRECOMP_SDL3
  SDL_CloseJoystick(joystick); SDL_DetachVirtualJoystick(device);
#else
  SDL_JoystickClose(joystick); SDL_JoystickDetachVirtual(device);
#endif
  ScGamepadClose(&p); SDL_Quit(); remove(argv[1]);
  puts("PASS: settings, GUID profile precedence, empty slots, virtual pad, stick, triggers, disconnect/reconnect");
  return 0;
}
