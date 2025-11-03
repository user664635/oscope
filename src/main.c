#include "../inc/def.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <fcntl.h>
#include <semaphore.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <threads.h>
#include <unistd.h>
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

u8 *pdata;
static int recvh(void *p) {
  Smem *sm = p;
  struct timespec ts = {0, 1000000};
  while (!quit) {
    sem_timedwait(&sm->semr, &ts);
    int val;
    sem_getvalue(&sm->semr, &val);
    if (!val)
      continue;
    memcpy(pdata, sm->bufr, sizeof(sm->bufr));
  }
  return 0;
}
int main() {
  int fd = shm_open("oscope", O_CREAT | O_RDWR, 0o666);
  if (fd == -1) {
    perror("shm_open");
    return 1;
  }

  if (ftruncate(fd, sizeof(Smem)) == -1) {
    perror("ftruncate");
    return 1;
  }

  Smem *p = mmap(NULL, sizeof(Smem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) {
    perror("mmap");
    return 1;
  }
  close(fd);
  sem_init(&p->sems, 1, 0);
  sem_init(&p->semr, 1, 0);
  extern void crtwin(), crtinst(), crtsrf();
  extern int gpu(void *);
  crtwin();
  crtinst();
  crtsrf();
  thrd_t gputhrd, recvt;
  thrd_create(&gputhrd, gpu, 0);
  thrd_create(&recvt, recvh, p);
  int res;
  while (!quit)
    iter();
  thrd_join(gputhrd, &res);
  thrd_join(recvt, &res);
  sem_destroy(&p->sems);
  sem_destroy(&p->semr);
  munmap(p, sizeof(Smem));
  shm_unlink("oscope");
}
