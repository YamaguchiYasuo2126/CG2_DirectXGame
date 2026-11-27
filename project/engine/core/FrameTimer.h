#pragma once

#include <chrono>

// 毎フレームの経過時間を秒単位で取得する。
class FrameTimer {
public:
	// 初期化処理にかかった時間を次のTickへ含めないよう、ループ開始前に呼ぶ。
	void Initialize();
	// 前回呼び出しからの秒数を返す（0～0.1秒）。FPS制限の待機は行わない。
	float Tick();

private:
	using Clock = std::chrono::steady_clock; // OSの時刻修正で逆行しない経過時間用時計。

	Clock::time_point previous_{};
	bool initialized_ = false;
};
