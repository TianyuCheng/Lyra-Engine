#pragma once

#ifndef LYRA_LIBRARY_VENDOR_MATH_H
#define LYRA_LIBRARY_VENDOR_MATH_H

// THIS IS PURELY A HEADER WRAPPER FOR MATH, with specific macros defined.

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lyra
{
    // quaternion type alias
    using Quaternion = glm::quat;

    // vector type alias
    using Vector2 = glm::mediump_vec2;
    using Vector3 = glm::mediump_vec3;
    using Vector4 = glm::mediump_vec4;

    // matrix type alias
    using Matrix2x2 = glm::mediump_mat2x2;
    using Matrix2x3 = glm::mediump_mat2x3;
    using Matrix2x4 = glm::mediump_mat2x4;
    using Matrix3x2 = glm::mediump_mat3x2;
    using Matrix3x3 = glm::mediump_mat3x3;
    using Matrix3x4 = glm::mediump_mat3x4;
    using Matrix4x2 = glm::mediump_mat4x2;
    using Matrix4x3 = glm::mediump_mat4x3;
    using Matrix4x4 = glm::mediump_mat4x4;

    // type size sanity check
    static_assert(sizeof(Vector2) == sizeof(float) * 2);
    static_assert(sizeof(Vector3) == sizeof(float) * 3);
    static_assert(sizeof(Vector4) == sizeof(float) * 4);

    // type align sanity check
    static_assert(alignof(Vector2) == sizeof(float));
    static_assert(alignof(Vector3) == sizeof(float));
    static_assert(alignof(Vector4) == sizeof(float));

} // namespace lyra

#endif // LYRA_LIBRARY_VENDOR_MATH_H
