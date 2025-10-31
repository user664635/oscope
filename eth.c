#include <arpa/inet.h>
#include <assert.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <threads.h>
#include <unistd.h>

typedef __uint128_t u128;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef _Float32 f32;

static volatile bool run = 1;
static void sigh(int) { run = 0; }

static int socketh, ui;
static const u64 local = 0x222222222222;
static struct {
  struct {
    u128 dst : 48, src : 48, p : 16, seq : 16;
  };
  u8 data[1500];
} sbuf = {{2, local, 0x1919, 0}, "test"}, rbuf;

static int sendh(void *) {
  struct sockaddr_ll addr = {AF_PACKET, 0, 2, 0, 0, 6, "\6"};
  void *buf = &sbuf;
  while (run) {
    auto l = read(ui, buf, 1514);
    if (-1 == l || -1 == sendto(socketh, buf, l, 0, (struct sockaddr *)&addr,
                                sizeof(addr)))
      break;
  }
  return run = 0;
}
static int recvh(void *) {
  void *buf = &rbuf;
  u16 seq = 0;
  u64 loss = 0;
  while (run) {
    auto l = 32;//recvfrom(socketh, (void *)&rbuf, 1514, 0, 0, 0);
    if (-1 == l)
      break;
    //if (rbuf.p != 0x1919 && rbuf.src == local)
    //  continue;
    u16 s = rbuf.seq;
    u64 lo = s - seq - 1;
    loss += lo;
    if (lo)
      printf("loss:%ld\n", loss);
    seq = s;
    if (-1 == write(ui, buf + 16, l - 16))
      break;
  }
  return run = 0;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  ui = atoi(argv[1]);
  socketh = socket(AF_PACKET, SOCK_RAW, htons(0x1919));
  assert(socketh != -1);
  puts("eth");

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  thrd_t sendt, recvt;
#define crthrd(fn) thrd_create(&fn##t, fn##h, 0)
  crthrd(send);
  crthrd(recv);

  thrd_join(sendt, 0);
  thrd_join(recvt, 0);
  close(socketh);
}
