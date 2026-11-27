#pragma once

#include "Vector4.h"
#include <cmath>
#include <cstdint>
#include "Matrix4x4.h"

enum class LightingMode : int32_t
{
    None = 0,
    Lambert = 1,
    HalfLambert = 2,
};

// HLSL側のMaterial定義とメンバーの並び・サイズを一致させる。
struct Material {
    Vector4 color;
    int32_t lightingMode;
    float padding[3]; // 次の行列を16バイト境界へ揃えるための未使用領域。
    Matrix4x4 uvTransform;
};
