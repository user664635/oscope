#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <threads.h>
#include <unistd.h>
#include <vulkan/vulkan_core.h>

#include "../inc/const.h"
#include "../inc/def.h"

volatile bool quit;
static void sigh(int) { quit = 1; }
u8 *pdata;
f32 pe = -I2_3 - .125, ne = -I2_3 + .125;
usize trig;
static constexpr usize n = FFT_N;
static constexpr usize logn = __builtin_ctz(n);
static usize rev[n];
static c32 rot[n];
void fft_init() {
  for (usize i = 0; i < n; ++i)
    rev[i] = __builtin_bitreverse32(i) >> (32 - logn),
    rot[i] = __builtin_cexp(-2i * PI * i / n);
}
void fft(u8 const *restrict in, c32 *restrict out) {
  for (usize i = 0; i < n; ++i)
    out[rev[i]] = in[i];
  for (usize i = 0; i < logn; ++i) {
    int m = 1 << i;
    for (usize j = 0; j < n; j += m << 1)
      for (int k = 0; k < m; ++k) {
        c32 t = rot[k << (logn - i - 1)] * out[j + k + m];
        c32 u = out[j + k];
        out[j + k] = u + t;
        out[j + k + m] = u - t;
      }
  }
}
Line *linedata;
static sem_t fftsem;
static int ffth(void *) {
  fft_init();
  while (!quit) {
    auto v = sem_timedwait(&fftsem, &ms1);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        continue;
      else {
        perror("sem");
        break;
      }
    }
    Line *lfp = linedata + 1024;
    c32 out[n];
    fft(pdata + trig, out);
    const f32 a = SQRT2_2 / n / 32;
    for (usize i = 0; i < n; ++i) {
      f32 x = i * I2_3 / n - 1;
      f32 y = __builtin_cabs(out[i]) * -I_3 * a;
      f32 r = __builtin_creal(out[i]) * a;
      f32 g = __builtin_cimag(out[i]) * a;
      lfp[i] = (Line){{x, 0, x, y}, {r, g, 1, 1}};
    }
  }
  return 0;
}
static int recvh(void *p) {
  Smem *sm = p;
  usize i = 0, stat = 0;
  while (!quit) {
    auto v = sem_timedwait(&sm->semrc, &ms1);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        continue;
      else {
        perror("sem");
        break;
      }
    }
    u16 pt = -128 - pe * 384;
    u16 nt = -128 - ne * 384;
    for (usize j = 0; j < 1024; ++j) {
      u16 val = sm->bufr[j];
      pdata[i + j] = val;
      switch (stat) {
      case 0:
        stat += val < nt;
        break;
      case 1:
        stat += val > pt;
        break;
      case 2:
        ++stat;
        trig = i + j;
      }
    }
    sem_post(&fftsem);
    sem_post(&sm->semrp);
    i += 1024;
    if (i > 1048575) {
      i = 0;
      stat = 0;
    }
  }
  return 0;
}

static sem_t sendsem;
vec2 mousepos;
usize mscnt;
static bool sendm;
static usize codecnt;
typedef struct {
  u32 op : 16, imm0 : 1, imm1 : 1;
  u32 dst;
  union {
    u32 r0;
    f32 i0;
  };
  union {
    u32 r1;
    f32 i1;
  };
} Code;
typedef struct {
  u32 imm;
  f32 num;
} Reg;
static Code code[64];
static f32 fnvart;
static f32 fnt(f32, f32) { return fnvart; }
static f32 fnsin(f32 x, f32) { return sin(x); }
static f32 (*fns[])(f32, f32) = {fnt, fnsin};
static f32 fun() {
  f32 reg[64];
  for (usize i = 0; i < codecnt; ++i) {
    Code c = code[i];
    f32 op0 = c.imm0 ? c.i0 : reg[c.r0];
    f32 op1 = c.imm1 ? c.i1 : reg[c.r1];
    reg[c.dst] = fns[c.op](op0, op1);
  }

  return reg[0];
}
static int sendh(void *p) {
  Smem *sm = p;
  while (!quit) {
    auto v = sem_timedwait(&sendsem, &ms1);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        continue;
      else {
        perror("sem");
        break;
      }
    }
    Line *lp = linedata;
    if (sendm) {
      lp += 2048;
      f32 y0 = 0, x0 = -I_3;
      for (usize i = 0; i < 8192; ++i) {
        f32 t = i / 4096. - 1;
        fnvart = t;
        f32 x = t * I_3;
        f32 y = fun() + 1;
        sm->bufs[i] = y * 128;
        y *= I_3;
        lp[i] = (Line){{x0, y0, x, y}, {0, 1, 1, 1}};
        x0 = x, y0 = y;
      }
    } else {
      lp += 10240;
      usize j = 0;
      for (usize i = 0; i < mscnt; ++i) {
        f32 jmax = (lp[i].pos.x + I_3) * 3 * 4096;
        u8 y = (lp[i].pos.y + 1) * 3 * 128;
        while (j < jmax)
          sm->bufs[j++] = y;
      }
    }
    sem_post(&sm->sems);
  }
  quit = 1;
  return 0;
}

u32 w, h;
f32 scale = 2048;
char ibuf[64];
void compile() {
  char *p0 = ibuf, *p1 = p0;
  char *fns[] = {"t", "sin"};
  usize regcnt = 0;
  codecnt = 0;
  Reg reg[64];
  while (*p1) {
    f32 val = strtof(p0, &p1);
    if (p0 == p1) {
      while (*p1 && *p1 != ' ')
        ++p1;
      int op = -1;
      for (int i = 0; i < 2; ++i)
        if (!strncmp(p0, fns[i], p1 - p0))
          op = i;
      code[codecnt].op = 0;
      switch (op) {
      case 0:
        code[codecnt++].dst = regcnt++;
        break;
      case 1:
        u32 imm = reg[regcnt].imm;
        code[codecnt].imm0 = imm;
        if (imm)
          code[codecnt].i0 = reg[regcnt].num;
        else
          code[codecnt].r0 = regcnt - 1;
        code[codecnt++].dst = regcnt - 1;
        break;
      }
    } else {
      reg[regcnt++] = (Reg){1, val};
    }
    p0 = p1;
  }
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
  sem_init(&p->semrp, 1, 1);
  sem_init(&p->semrc, 1, 0);
  sem_init(&sendsem, 0, 0);
  sem_init(&fftsem, 0, 0);
  p->hs.p = 0x1919;
  p->hs.src = local;
  p->hs.dst = -1;
  p->hs.seq = 1;

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  extern void crtwin(), crtinst(), crtsrf();
  extern int gpu(void *);
  crtwin();
  crtinst();
  crtsrf();
  thrd_t gputhrd, recvt, sendt, fftt;
  thrd_create(&gputhrd, gpu, p);
  thrd_create(&fftt, ffth, 0);
  thrd_create(&recvt, recvh, p);
  thrd_create(&sendt, sendh, p);
  int res;

  bool ctrl = 0;
  vec2 pms = 0;
  usize icnt = 0;
  while (!quit) {
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
      Line *lp = linedata + 10240;
      if (ctrl)
        lp[mscnt++] = (Line){{pms.x, pms.y, ms.x, ms.y}, {1, 0, 1, 1}};
      pms = ms;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      scale *= 1 + event.wheel.y;
      scale = max(1.f, scale);
      p->hs.seq += event.wheel.x * 256;
      sem_post(&sendsem);
      break;
    case SDL_EVENT_KEY_UP:
      switch (event.key.key) {
      case SDLK_LCTRL:
        ctrl = 0;
        sendm = 0;
        sem_post(&sendsem);
        break;
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      auto key = event.key.key;
      if (key == '\b' && icnt > 0)
        ibuf[--icnt] = 0;
      if (key > 31 && key < 127 && icnt < 62) {
        ibuf[icnt++] = key;
        ibuf[icnt] = 0;
      }
      if (event.key.repeat)
        break;
      switch (key) {
      case '\r':
        sendm = 1;
        compile();
        ibuf[icnt = 0] = 0;
        sem_post(&sendsem);
        break;
      case SDLK_LCTRL:
        ctrl = 1;
        pms.x = -I_3;
        mscnt = 0;
        break;
      }
      break;
    }
  }
  thrd_join(gputhrd, &res);
  thrd_join(recvt, &res);
  thrd_join(sendt, &res);
  thrd_join(fftt, &res);
  sem_destroy(&p->sems);
  sem_destroy(&p->semrp);
  sem_destroy(&p->semrc);
  sem_destroy(&sendsem);
  sem_destroy(&fftsem);
  munmap(p, sizeof(Smem));
  shm_unlink("oscope");
}
