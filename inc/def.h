#pragma once
#include <semaphore.h>
#include <stdint.h>
#include <time.h>

typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;
typedef size_t usize;

typedef f32 [[clang::ext_vector_type(2)]] vec2;
typedef u32 [[clang::ext_vector_type(2)]] uvec2;
typedef f32 [[clang::ext_vector_type(3)]] vec3;
typedef f32 [[clang::ext_vector_type(4)]] vec4;

typedef struct {
  u128 dst : 48, src : 48, p : 16, seq : 16;
} Head;
typedef struct {
  sem_t sems, semr;
  Head hs;
  u8 bufs[8192];
  Head hr;
  u8 bufr[1024];
} Smem;
typedef struct {
  vec4 pos, col;
} Line;

#define exp __builtin_elementwise_exp
#define sin __builtin_elementwise_sin
#define cos __builtin_elementwise_cos
#define min __builtin_elementwise_min
#define max __builtin_elementwise_max
#define abs __builtin_elementwise_abs

#define dumps(x) __builtin_dump_struct(&x, printf)
#define bin(id, s) _binary_obj_##id##_spv_##s
