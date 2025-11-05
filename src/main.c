#include "../inc/const.h"
#include "../inc/def.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <errno.h>
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

volatile bool quit;
static void sigh(int) { quit = 1; }
u8 *pdata;
static int recvh(void *p) {
  Smem *sm = p;
  struct timespec ts = {0, 1000000};
  usize i = 0;
  while (!quit) {
    auto v = sem_timedwait(&sm->semr, &ts);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        continue;
      else {
        perror("sem");
        break;
      }
    }
    memcpy(pdata + i, sm->bufr, sizeof(sm->bufr));
    i = i + 1024 & 1048575;
  }
  return 0;
}

static sem_t sendsem;
vec2 mousepos;
usize mscnt;
Line *linedata;
static int sendh(void *p) {
  Smem *sm = p;
  struct timespec ts = {0, 1000000};
  while (!quit) {
    auto v = sem_timedwait(&sendsem, &ts);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        continue;
      else {
        perror("sem");
        break;
      }
    }
    Line *lp = linedata + 1024;
    usize j = 0;
    for (usize i = 0; i < mscnt; ++i) {
      f32 jmax = (lp[i].pos.x + I_3) * 3 * 4095;
      u8 y = (lp[i].pos.y + 1) * 3 * 127;
      while (j < jmax)
        sm->bufs[j++] = y;
    }
    sem_post(&sm->sems);
  }
  quit = 1;
  return 0;
}

f32 pe = -.75, ne = -.5;
u32 w, h;
f32 scale;
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
  sem_init(&sendsem, 0, 0);
  p->hs.p = 0x1919;
  p->hs.src = 0x2222;
  p->hs.dst = 0x6666;

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  extern void crtwin(), crtinst(), crtsrf();
  extern int gpu(void *);
  crtwin();
  crtinst();
  crtsrf();
  thrd_t gputhrd, recvt, sendt;
  thrd_create(&gputhrd, gpu, 0);
  thrd_create(&recvt, recvh, p);
  thrd_create(&sendt, sendh, p);
  int res;

  bool ctrl = 0;
  vec2 pms = 0;
  while (!quit) {
    Line *lp = linedata + 1024;
    SDL_Event event;
    SDL_WaitEvent(&event);
    switch (event.type) {
    case SDL_EVENT_QUIT:
      quit = 1;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      mousepos.x = event.motion.x / w * 2 - 1;
      mousepos.y = event.motion.y / h * 2 - 1;
      if (event.motion.state & 1) {
        if (mousepos.x < -I_3) {
          if (abs(mousepos.y - pe) < 1e-2)
            pe = mousepos.y;
          else if (abs(mousepos.y - ne) < 1e-2)
            ne = mousepos.y;
        }
      }
      vec2 ms = min(max((vec2){-I_3, -1}, mousepos), (vec2){I_3, -I_3});
      ms.x = max(pms.x, ms.x);
      if (ctrl)
        lp[mscnt++] = (Line){{pms.x, pms.y, ms.x, ms.y}, {1, 0, 1, 1}};
      pms = ms;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      scale += event.wheel.y * 50;
      break;
    case SDL_EVENT_KEY_UP:
      switch (event.key.key) {
      case SDLK_LCTRL:
        ctrl = 0;
        sem_post(&sendsem);
        break;
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (event.key.repeat)
        break;
      switch (event.key.key) {
      case '\r':
        sem_post(&sendsem);
        break;
      case SDLK_LCTRL:
        ctrl = 1;
        pms.x = -I_3;
        mscnt = 0;
        break;
      case 'h':
        quit = 1;
      }
      break;
    }
  }
  thrd_join(gputhrd, &res);
  thrd_join(recvt, &res);
  thrd_join(sendt, &res);
  sem_destroy(&p->sems);
  sem_destroy(&p->semr);
  munmap(p, sizeof(Smem));
  shm_unlink("oscope");
}
