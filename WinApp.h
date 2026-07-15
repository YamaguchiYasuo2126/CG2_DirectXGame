#pragma once

#include <Windows.h>
#include <cstdint>

// Windowsのウィンドウ作成とメッセージ処理を担当します。
class WinApp {
public:
	// 描画領域として使うクライアントサイズです。
	static constexpr int32_t kClientWidth = 1280;
	static constexpr int32_t kClientHeight = 720;

	void Initialize();
	void Finalize();
	bool ProcessMessage();

	HWND GetHwnd() const { return hwnd_; }

private:
	// Windowsから呼び出されるメッセージ処理関数です。
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	WNDCLASS windowClass_{};
	HWND hwnd_ = nullptr;
};
