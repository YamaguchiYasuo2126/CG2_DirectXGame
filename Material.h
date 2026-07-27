#pragma once

#include "Vector4.h"
#include <cmath>
#include <cstdint>

struct Material {
    Vector4 color;
    int32_t enableLighting;
};