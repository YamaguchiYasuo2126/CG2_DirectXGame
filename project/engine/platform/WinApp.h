#pragma once

#include <Windows.h>
#include <cstdint>

// Windowsのウィンドウ作成とメッセージ処理を担当します。
class WinApp {
public:
	// 描画領域として使うクライアントサイズです。
	static constexpr int32_t kClientWidth = 1280;
	static constexpr int32_t kClientHeight = 720;

	// ウィンドウクラスを登録し、指定したクライアントサイズのウィンドウを表示する。
	void Initialize();
	void Finalize();
	// メッセージを処理し、終了要求が来た場合はfalseを返す。
	bool ProcessMessage();

	HWND GetHwnd() const { return hwnd_; }
	HINSTANCE GetHInstance() const { return windowClass_.hInstance; }

private:
	// Windowsから呼び出されるメッセージ処理関数です。
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	WNDCLASS windowClass_{};
	HWND hwnd_ = nullptr;
};
