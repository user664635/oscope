#include "../inc/def.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <threads.h>
#include <vulkan/vulkan_core.h>

static vec3 dir;
static f32 scale;
vec2 mousepos;
bool quit;
#define inputdir(d)                                                            \
  case 'w':                                                                    \
    dir.z += d;                                                                \
    break;                                                                     \
  case 's':                                                                    \
    dir.z -= d;                                                                \
    break;                                                                     \
  case 'a':                                                                    \
    dir.x -= d;                                                                \
    break;                                                                     \
  case 'd':                                                                    \
    dir.x += d;                                                                \
    break;                                                                     \
  case ' ':                                                                    \
    dir.y -= d;                                                                \
    break;                                                                     \
  case 'c':                                                                    \
    dir.y += d;                                                                \
    break;
u32 w, h;
static void iter() {
  SDL_Event event;
  SDL_WaitEvent(&event);
  switch (event.type) {
  case SDL_EVENT_QUIT:
    quit = 1;
    return;
  case SDL_EVENT_MOUSE_MOTION:
    mousepos.x += event.motion.xrel;
    mousepos.y += event.motion.yrel;
    mousepos = min(mousepos, (vec2){w, h});
    mousepos = max(mousepos, (vec2){});
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    scale += event.wheel.y * 50;
    break;
  case SDL_EVENT_KEY_DOWN:
    if (event.key.repeat)
      break;
    switch (event.key.key) {
      inputdir(1);
    case 'h':
      quit = 1;
    }
    break;
  case SDL_EVENT_KEY_UP:
    if (event.key.repeat)
      break;
    switch (event.key.key) { inputdir(-1); }
    break;
  }
}

#include <spawn.h>
int main() {
  pid_t pid;
  char *argv[] = {"sudo","/bin/fish", 0};
  //posix_spawnp(&pid, "sudo", 0, 0, argv, 0);
  extern void crtwin(), crtinst(), crtsrf();
  extern int gpu(void *);
  crtwin();
  crtinst();
  crtsrf();
  thrd_t gputhrd;
  thrd_create(&gputhrd, gpu, 0);
  int res;
  while (!quit)
    iter();
  thrd_join(gputhrd, &res);
}
