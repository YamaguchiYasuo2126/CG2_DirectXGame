#include "WinApp.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

void WinApp::Initialize() {
	// Windowsへ登録するWindowClassを作成します。
	windowClass_.lpfnWndProc = WindowProc;
	windowClass_.lpszClassName = L"CG2WindowClass";
	windowClass_.hInstance = GetModuleHandle(nullptr);
	windowClass_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&windowClass_);

	// クライアント領域が指定サイズになるよう、外枠込みのウィンドウサイズへ変換します。
	RECT windowRect = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

	// 実際のWindowを作成して表示します。
	hwnd_ = CreateWindow(
		windowClass_.lpszClassName,
		L"CG",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		windowClass_.hInstance,
		nullptr);

	ShowWindow(hwnd_, SW_SHOW);
}

void WinApp::Finalize() {
	// Windowが残っている場合だけ閉じます。
	if (hwnd_ != nullptr) {
		CloseWindow(hwnd_);
		hwnd_ = nullptr;
	}
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	// 溜まっているWindowsメッセージを全て処理します。
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	// ImGuiが使う入力メッセージは先にImGuiへ渡します。
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	switch (msg) {
	case WM_DESTROY:
		// Windowが閉じられたら、メインループへ終了を知らせます。
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}
