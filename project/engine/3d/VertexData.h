#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

// 入力レイアウトと対応する1頂点分の位置・UV・法線。position.wは通常1。
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};
