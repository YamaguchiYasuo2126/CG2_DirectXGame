#pragma once
#include "Vector3.h"
#include "Vector4.h"

// 平行光源
struct DirectionalLight {
    Vector4 color;     //!< ライトの色
    Vector3 direction; //!< ライトの向き
    float intensity;   //!< 輝度
};