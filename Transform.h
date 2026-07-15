#pragma once

#include "Vector3.h"

// 拡大縮小、回転、平行移動をまとめて扱うための構造体です。
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};
