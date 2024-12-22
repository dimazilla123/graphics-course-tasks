#version 450

layout(push_constant) uniform Parameters {
  uint iResolution_x;
  uint iResolution_y;
  float iTime;
} params;

float iTime;

layout(location = 0) out vec4 fragColor;

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec3 color = mix(vec3(0.7, 0.3, 0.7), vec3(0.8, 0.5, 0.2), sin(20.0 * 1.1 - iTime * 2.0) * 0.5 + 0.5);
    fragColor = vec4(color, 1.0);
}

void main()
{
  iTime = params.iTime;

  ivec2 iFragCoord = ivec2(gl_FragCoord.xy);
  mainImage(fragColor, iFragCoord);
}