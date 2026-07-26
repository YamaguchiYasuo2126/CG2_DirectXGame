#pragma once

#include "Vector2.h"
#include "Vector4.h"

// 1頂点分の位置とUV座標
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
};
