#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"

struct UniformParams
{
  struct
  {
    shader_uint x;
    shader_uint y;
  } resolution;
  shader_float time;
};


#endif // UNIFORM_PARAMS_H_INCLUDED
