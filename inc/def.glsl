layout(push_constant) uniform PushConstant {
  vec2 scl;
  float scale, trig;
  uint cnt;
}cmn;
layout(binding = 0) uniform sampler2D font;
