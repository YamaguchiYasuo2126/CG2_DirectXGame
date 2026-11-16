#include "FrameTimer.h"

#include <algorithm>

void FrameTimer::Initialize() {
	previous_ = Clock::now();
	initialized_ = true;
}

float FrameTimer::Tick() {
	const auto now = Clock::now();
	if (!initialized_) {
		previous_ = now;
		initialized_ = true;
		return 0.0f;
	}

	const float elapsed = std::chrono::duration<float>(now - previous_).count();
	previous_ = now;

	// 一時停止やウィンドウ操作の後に、1フレームで大きく動かないようにする。
	return std::clamp(elapsed, 0.0f, 0.1f);
}
