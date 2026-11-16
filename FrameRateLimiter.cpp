#include "FrameRateLimiter.h"

#include <thread>

void FrameRateLimiter::Initialize() {
	reference_ = Clock::now();
}

void FrameRateLimiter::Wait() {
	const auto kMinTime = std::chrono::microseconds(1'000'000 / 60);
	const auto kMinCheckTime = std::chrono::microseconds(1'000'000 / 65);

	// VSyncですでに約1フレーム分経過した場合は、追加の待機をしない。
	if (Clock::now() - reference_ < kMinCheckTime) {
		while (Clock::now() - reference_ < kMinTime) {
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

	reference_ = Clock::now();
}
