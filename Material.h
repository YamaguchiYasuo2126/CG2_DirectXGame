#pragma once

#include "Vector4.h"
#include <cmath>
#include <cstdint>
#include "Matrix4x4.h"

enum class LightingMode : int32_t {
    None = 0,
    Lambert = 1,
    HalfLambert = 2,
};

struct Material {
    Vector4 color;
    int32_t lightingMode;
    float padding[3];
    Matrix4x4 uvTransform;
};
