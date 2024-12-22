#version 430
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

// layout(local_size_x = 32, local_size_y = 32) in;

#include "UniformParams.h"

layout(binding = 1) uniform sampler2D generatedTex;
layout(binding = 2) uniform sampler2D torusTex;
layout(binding = 3) uniform sampler2D skyboxTex;

layout(binding = 0, set = 0) uniform Uniform {
  UniformParams params;
};

vec2 iResolution;
vec2 iMouse;

float sphere_sdf(in vec3 p, in float r)
{
    return length(p) - r;
}

float torus ( in vec3 pos, in vec2 t )
{
    vec3 pt = vec3(pos.x, pos.z, pos.y);
    vec2 q  = vec2 ( length ( pt.xz) - t.x, pt.y );

    return length ( q ) - t.y;
}

float TORUS_R = 1.0;
float TORUS_THICKNESS = 0.3;

float sdf(in vec3 p)
{
    // return sphere_sdf(p, 1.0);
    return torus(p, vec2(TORUS_R, TORUS_THICKNESS));
}

#define TRACE_ITER_LIM 100
#define EPS 0.01

vec3 raytrace(vec3 from, vec3 dir, out bool hit)
{
    vec3 p = from;
    float dist = 0.0;
    hit = false;
    
    for (int steps = 0; steps <  TRACE_ITER_LIM; ++steps)
    {
        float d = sdf(p);
        if (d < EPS)
        {
            hit = true;
            break;
        }
        dist += d;
        p += d * dir;
    }
    return p;
}
vec3 get_normal(vec3 p)
{
    float h = EPS;
    vec3 n = vec3(
        sdf(p + vec3(h, 0, 0)) - sdf(p - vec3(h, 0, 0)),
        sdf(p + vec3(0, h, 0)) - sdf(p - vec3(0, h, 0)),
        sdf(p + vec3(0, 0, h)) - sdf(p - vec3(0, 0, h))
    );
    return normalize(n);
}

vec2 pixel_offset(vec2 coord)
{
    return (coord - iResolution.xy * 0.5) / iResolution.x;
}

#define AMBIENT_LIGHT 0.3
vec3 light_pos = vec3(0.0, 0.0, -4.0);
vec3 diffuse_color = vec3(1.0, 1.0, 1.0);

vec3 phong_lighting(vec3 p, vec3 normal, vec3 dir) {
    vec3 light_dir = normalize(light_pos - p);
    
    vec3 ambient = AMBIENT_LIGHT * vec3(0.5, 0.5, 0.5);
    
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 diffuse = diff * diffuse_color;
    
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(dir, reflect_dir), 0.0), 32.0);
    vec3 specular = spec * vec3(1.0);
    
    return (ambient + diffuse + specular);
}

vec3 triponal(vec3 p, vec3 normal, sampler2D tex)
{
    vec3 abs_normal = abs(normal);
    abs_normal /= (abs_normal.x + abs_normal.y + abs_normal.z);

    vec3 x_projection = texture(tex, p.yz * 0.5).rgb * abs_normal.x;
    vec3 y_projection = texture(tex, p.xz * 0.5).rgb * abs_normal.y;
    vec3 z_projection = texture(tex, p.xy * 0.5).rgb * abs_normal.z;

    return x_projection + y_projection + z_projection;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = pixel_offset(fragCoord);
    vec3 camera_offset = vec3(0, 0, -4);
    camera_offset += vec3(pixel_offset(iMouse.xy), 0);
    vec3 dir = normalize(vec3(uv.x, uv.y, 1));
    
    bool hit = false;
    vec3 col = triponal(dir, dir, skyboxTex);
    vec3 p = raytrace(camera_offset, dir, hit);
    if (hit)
    {
        col = 
            (triponal(p, get_normal(p), torusTex)
            +
            triponal(p, get_normal(p), generatedTex)) / 2.0
            // triponal(p, get_normal(p), generatedTex)
            * phong_lighting(p, get_normal(p), dir);
    }

    fragColor = vec4(col,1.0);
}

layout (location = 0) out vec4 fragColor;

void main()
{
  iResolution = vec2(params.iResolution_x, params.iResolution_y);
  iMouse = vec2(params.iMouse_x, params.iMouse_y);

  ivec2 iFragCoord = ivec2(gl_FragCoord.xy);
  mainImage(fragColor, iFragCoord);

  // if (iFragCoord.x < 1280 && iFragCoord.y < 720)
  //  imageStore(resultImage, iFragCoord, fragColor);
}