#include "GameApplication.h"

#include <Windows.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

void GameApplication::Initialize() {
	// COMを利用するDirectX関連APIのために、アプリ全体で一度だけ初期化します。
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// 依存される側から順番に初期化します。
	logger_.Initialize();
	winApp_.Initialize();
	dxCommon_.Initialize(winApp_.GetHwnd(), WinApp::kClientWidth, WinApp::kClientHeight, &logger_);
	shaderCompiler_.Initialize(&logger_);
	object3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_);
	sprite_.Initialize(&dxCommon_);
	InitializeImGui();
	initialized_ = true;
}

void GameApplication::Run() {
	while (winApp_.ProcessMessage()) {
		// 1フレーム分のUIとDirectX描画コマンドを積みます。
		BeginImGuiFrame();
		dxCommon_.BeginFrame();

		// ゲーム側の更新と描画です。描画対象が増えたらこの周辺に追加します。
		object3d_.Update();
		sprite_.Update();
		
		// ImGuiのUI更新
		object3d_.DrawImGui();
		sprite_.DrawImGui();

		// 描画
		object3d_.Draw();
		sprite_.Draw();

		EndImGuiFrame();
		dxCommon_.EndFrame();
	}
}

void GameApplication::Finalize() {
	if (!initialized_) {
		return;
	}

	// 初期化と逆順に解放します。参照先が先に消えないようにするためです。
	FinalizeImGui();
	sprite_.Finalize();
	object3d_.Finalize();
	shaderCompiler_.Finalize();
	dxCommon_.Finalize();
	winApp_.Finalize();
	logger_.Log("Hello,DirectX!\n");
	logger_.Finalize();
	CoUninitialize();
	initialized_ = false;
}

void GameApplication::InitializeImGui() {
#ifdef USE_IMGUI
	// ImGuiが描画に使うSRVは先頭0番を予約しています。
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp_.GetHwnd());
	ImGui_ImplDX12_Init(
		dxCommon_.GetDevice(),
		dxCommon_.GetBackBufferCount(),
		dxCommon_.GetRenderTargetFormat(),
		dxCommon_.GetSrvDescriptorHeap(),
		dxCommon_.GetSrvCpuHandle(0),
		dxCommon_.GetSrvGpuHandle(0));
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
#endif
}

void GameApplication::BeginImGuiFrame() {
#ifdef USE_IMGUI
	// ImGuiの各バックエンドへ、新しいフレームの開始を通知します。
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void GameApplication::EndImGuiFrame() {
#ifdef USE_IMGUI
	// 作成されたImGuiの描画データを、現在のDirectXコマンドリストへ積みます。
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_.GetCommandList());
#endif
}

void GameApplication::FinalizeImGui() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}
