#include <arpa/inet.h>
#include <assert.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <threads.h>
#include <unistd.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef _Float32 f32;

volatile bool quit;
void sigh(int) { quit = 1; }

int sockfd;
struct {
  u16 id, loss;
  u32 seq, t;
  u16 type;
  u8 data[1500];
} sbuf = {2, 0, 0, 0, 0x1919, "test"}, rbuf;

_Atomic u64 compcnt, sbytes, rbytes;
int comph(void *) {
  u64 c = 0;
  while (!quit) {
    u64 cnt = compcnt++;
    f32 t = cnt * 1e-3;
    f32 y = __builtin_sinf(t) + 1;
    sbuf.data[c] = y * 128;
    c = c == 1499 ? 0 : c + 1;
  }
  return 0;
}

int sendh(void *) {

  struct ifreq ifr = {{"lo"}, {}};
  assert(ioctl(sockfd, SIOCGIFINDEX, &ifr) != -1 || close(sockfd));
  struct sockaddr_ll addr = {AF_PACKET, 0, ifr.ifr_ifindex, 0, 0, 6, "\6"};
  while (!quit) {
    sbytes += sendto(sockfd, (void *)&sbuf, 1514, 0, (struct sockaddr *)&addr,
                     sizeof(addr));
    ++sbuf.seq;
  }
  return 0;
}
char data;
u32 loss, seq;
int recvh(void *) {
  while (!quit) {
    u32 r = recvfrom(sockfd, (void *)&rbuf, 1514, 0, 0, 0);
    if (rbuf.type != 0x9999)
      continue;
    rbytes += r;
    data = *rbuf.data;
    loss += rbuf.seq - seq - 1;
    seq = rbuf.seq;
  }
  return 0;
}
int main() {
  sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  assert(sockfd != -1);

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  thrd_t compt, sendt, recvt;
#define crthrd(fn) thrd_create(&fn##t, fn##h, 0)
  crthrd(comp);
  crthrd(send);
  crthrd(recv);

  struct timespec ts = {0, 100000000};
  while (!quit) {
    printf("\rsend seq: %10u rate: %10fMbps\trecv seq: %10u rate: %10fMbps\t "
           "data: %3hhx loss: %10u",
           sbuf.seq, sbytes * 8e-5, seq, rbytes * 8e-5, data, loss);
    compcnt = 0, sbytes = 0, rbytes = 0;
    fflush(stdout);
    thrd_sleep(&ts, 0);
  }
  close(sockfd);
  thrd_join(compt, 0);
  thrd_join(sendt, 0);
  thrd_join(recvt, 0);
}
