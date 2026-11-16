#pragma once

#include <chrono>

// 描画フレームの間隔が約1/60秒より短い場合に待機する。
class FrameRateLimiter {
public:
	void Initialize();
	void Wait();

private:
	using Clock = std::chrono::steady_clock;
	Clock::time_point reference_{};
};
