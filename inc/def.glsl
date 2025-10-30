layout(push_constant) uniform PushConstant {
  vec2 scl, mp;
  uint cnt;
}cmn;
layout(binding = 0) uniform sampler2D font;
struct Circle {
  vec4 col, pv, a;
};
struct Grid {
  uint cnt;
  Circle cir[8];
};
layout(binding = 1) 
#ifdef COMP
    coherent
#else
    readonly
#endif
	buffer ObjData { Circle cir[65536]; };
layout(binding = 2)
#ifdef COMP
    coherent
#else
    readonly
#endif
    buffer GridData {
  Grid grid[65536];
};
const ivec2 grid9[] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0},
                     {-1, 1},  {0, 1},  {1, 1}};
