#pragma once
#include "Matrix4x4.h"

// Object3d用HLSLの定数バッファと同じ順序を保つGPU転送用データ。
struct TransformationMatrix
{
    Matrix4x4 WVP; // ワールド・ビュー・射影を合成し、頂点をクリップ座標へ変換。
    Matrix4x4 World; // モデル座標からワールド座標への変換。
};
