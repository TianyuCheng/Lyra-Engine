#ifndef LYRA_TESTLIB_HELPER_UNIFORM_H
#define LYRA_TESTLIB_HELPER_UNIFORM_H

#include "./common.h"

struct Uniform
{
    GPUBuffer ubuffer;

    static auto create(float fovy, float aspect, const glm::vec3& eye) -> Uniform;
};

#endif // LYRA_TESTLIB_HELPER_UNIFORM_H
