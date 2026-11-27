#pragma once

#include "Vector3.h"

// 拡大縮小、回転、平行移動をまとめて扱うための構造体
struct Transform {
	Vector3 scale;
	Vector3 rotate; // 各軸の回転角。単位はラジアン。
	Vector3 translate; // 3Dはワールド座標、Spriteは画面上のピクセル座標として使う。
};
