#ifndef LYRA_TESTLIB_HELPER_CAMERA_H
#define LYRA_TESTLIB_HELPER_CAMERA_H

#include "./linmath.h"

struct Camera
{
    glm::mat4 proj;
    glm::mat4 view;
};

#endif // LYRA_TESTLIB_HELPER_CAMERA_H
