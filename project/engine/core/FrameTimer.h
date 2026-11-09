#pragma once

#include <chrono>

// 毎フレームの経過時間を秒単位で取得する。
class FrameTimer {
public:
	void Initialize();
	float Tick();

private:
	using Clock = std::chrono::steady_clock;

	Clock::time_point previous_{};
	bool initialized_ = false;
};
