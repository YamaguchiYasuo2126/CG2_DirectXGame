#pragma once

#include "Vector3.h"

// 天候が演出へ与える影響。風速はワールド単位/秒、雨の強さは0～1。
struct WeatherConditions {
	Vector3 windVelocity{};
	float rainIntensity = 0.0f;
};
