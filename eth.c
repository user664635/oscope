#include "inc/def.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <threads.h>
#include <unistd.h>

static volatile bool quit;
static void sigh(int) { quit = 1; }

static int socketh;

static int sendh(void *p) {
  struct sockaddr_ll addr = {AF_PACKET, 0, 2, 0, 0, 6, "\6"};
  Smem *sm = p;
  struct timespec ts = {0, 1000000};
  while (!quit) {
    auto v = sem_timedwait(&sm->sems, &ts);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        continue;
      else {
        perror("sem");
        break;
      }
    }
    auto l = sendto(socketh, &sm->hs, sizeof(Head) + 8192, 0,
                    (struct sockaddr *)&addr, sizeof(addr));
    if (l == -1) {
      perror("sendto");
      break;
    }
  }
  quit = 1;
  puts("send exit");
  return 0;
}
static int recvh(void *p) {
  u16 seq = 0;
  u64 loss = 0;
  Smem *sm = p;
  struct {
    Head h;
    u8 d[1024];
  } buf;
  struct timespec ts = {0, 1000000};
  while (!quit) {
    auto l = recvfrom(socketh, &buf, 1514, 0, 0, 0);
    if (l == -1) {
      perror("recvfrom");
      break;
    }
    if (buf.h.p != 0x1919 || buf.h.src == local)
      continue;
    if (l < 1040) {
      puts("l");
      continue;
    }
    u16 s = buf.h.seq;
    u16 lo = s - seq - 1;
    seq = s;
    loss += lo;
    if (lo)
      printf("loss:%ld\n", loss);

  wait:
    if (quit)
      break;
    auto v = sem_timedwait(&sm->semrp, &ts);
    if (v == -1) {
      if (errno == ETIMEDOUT)
        goto wait;
      else {
        perror("sem");
        break;
      }
    }
    memcpy(&sm->bufr, buf.d, 1024);
    sem_post(&sm->semrc);
  }
  quit = 1;
  puts("recv exit");
  return 0;
}
int main() {
  socketh = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (socketh == -1) {
    perror("socket");
    return 1;
  }
  puts("eth");
  int fd = shm_open("oscope", O_RDWR, 0);
  if (fd == -1) {
    perror("shm_open");
    return 1;
  }
  Smem *p = mmap(NULL, sizeof(Smem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) {
    perror("mmap");
    return 1;
  }

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  thrd_t sendt, recvt;
  thrd_create(&sendt, sendh, p);
  thrd_create(&recvt, recvh, p);

  thrd_join(sendt, 0);
  thrd_join(recvt, 0);
  close(socketh);
}
