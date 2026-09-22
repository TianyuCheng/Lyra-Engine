#ifndef LYRA_TESTLIB_HELPER_VERTEX_H
#define LYRA_TESTLIB_HELPER_VERTEX_H

#include "./linmath.h"

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
};

#endif // LYRA_TESTLIB_HELPER_VERTEX_H
