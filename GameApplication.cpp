#include "GameApplication.h"

#include <Windows.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

namespace {
void ApplyEditorStyle() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.WindowPadding = ImVec2(7.0f, 7.0f);
	style.FramePadding = ImVec2(6.0f, 3.0f);
	style.ItemSpacing = ImVec2(6.0f, 5.0f);

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.25f, 0.34f, 0.94f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.34f, 0.47f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.42f, 0.58f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.40f, 0.58f, 0.73f, 0.65f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.38f, 0.51f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.31f, 0.48f, 0.64f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.54f, 0.70f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.27f, 0.43f, 0.58f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.34f, 0.52f, 0.68f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.57f, 0.73f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.25f, 0.40f, 0.55f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.33f, 0.51f, 0.67f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.39f, 0.58f, 0.74f, 1.00f);
}
}
#endif

void GameApplication::Initialize() {
	// COMを利用するDirectX関連APIのために、アプリ全体で一度だけ初期化します。
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// 依存される側から順番に初期化します。
	logger_.Initialize();
	winApp_.Initialize();

	HINSTANCE hInstance = GetModuleHandle(nullptr); // HINSTANCEを取得
	HWND hwnd = winApp_.GetHwnd();                  // HWNDを取得
	input_.Initialize(hInstance, hwnd);             // Inputクラスの初期化

	dxCommon_.Initialize(winApp_.GetHwnd(), WinApp::kClientWidth, WinApp::kClientHeight, &logger_);
	shaderCompiler_.Initialize(&logger_);
	object3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "plane.obj", "Plane");
	sphere3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "sphere", "Sphere");
	teapot3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "teapot.obj", "Utah Teapot");
	bunny3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "bunny.obj", "Stanford Bunny");
	fence3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,"fence.obj", "Fence", "resources/fence");
	multiMesh3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "multiMesh.obj", "Multi Mesh");
	multiMaterial3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "multiMaterial.obj", "Multi Material");
	sprite_.Initialize(&dxCommon_);
	debugCamera_.Initialize();
	InitializeImGui();
	initialized_ = true;
}

void GameApplication::SetBgmControls(BgmControlCallback play, BgmControlCallback stop, void* context) {
	playBgm_ = play;
	stopBgm_ = stop;
	bgmContext_ = context;
	isBgmPlaying_ = false;
}

void GameApplication::Run() {
	while (winApp_.ProcessMessage()) {
		// 1フレーム分のUIとDirectX描画コマンドを積みます。
		BeginImGuiFrame();
		dxCommon_.BeginFrame();

		input_.Update();

		// ImGuiのウィンドウでカメラを切り替えられるようにする
#ifdef USE_IMGUI
		ImGui::SetNextWindowPos(ImVec2(750.0f, 74.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(350.0f, 0.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Settings");
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Debug Camera Active", &isDebugCameraActive_);
		}
		if (ImGui::CollapsingHeader("BGM", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Button("Play") && playBgm_ != nullptr) {
				playBgm_(bgmContext_);
				isBgmPlaying_ = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop") && stopBgm_ != nullptr) {
				stopBgm_(bgmContext_);
				isBgmPlaying_ = false;
			}
			ImGui::Text("Status: %s", isBgmPlaying_ ? "Playing" : "Stopped");
		}
		ImGui::End();
#endif

		Matrix4x4 viewProjection;

		if (isDebugCameraActive_) {
			// デバッグカメラがONのとき
			debugCamera_.Update(&input_);
			viewProjection = debugCamera_.GetViewProjectionMatrix();
		}
		else {
			// デバッグカメラがOFFのとき（通常のカメラ）
			// 通常カメラの行列を計算する
			Matrix4x4 cameraMatrix = MatrixUtility::MakeAffineMatrix(
				normalCameraTransform_.scale,
				normalCameraTransform_.rotate,
				normalCameraTransform_.translate
			);
			Matrix4x4 viewMatrix = MatrixUtility::Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MatrixUtility::MakePerspectiveFovMatrix(
				0.45f,
				float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
				0.1f,
				100.0f
			);
			viewProjection = MatrixUtility::Multiply(viewMatrix, projectionMatrix);
		}

		// 決定した行列をオブジェクトに渡す
		object3d_.SetViewProjectionMatrix(viewProjection);
		sphere3d_.SetViewProjectionMatrix(viewProjection);
		teapot3d_.SetViewProjectionMatrix(viewProjection);
		bunny3d_.SetViewProjectionMatrix(viewProjection);
		fence3d_.SetViewProjectionMatrix(viewProjection);
		multiMesh3d_.SetViewProjectionMatrix(viewProjection);
		multiMaterial3d_.SetViewProjectionMatrix(viewProjection);

		// ゲーム側の更新と描画です。描画対象が増えたらこの周辺に追加します。
		object3d_.Update();
		sphere3d_.Update();
		teapot3d_.Update();
		bunny3d_.Update();
		fence3d_.Update();
		multiMesh3d_.Update();
		multiMaterial3d_.Update();
		sprite_.Update();
		
		// ImGuiのUI更新
		sprite_.DrawImGui();
		object3d_.DrawImGui();
		sphere3d_.DrawImGui();
		teapot3d_.DrawImGui();
		bunny3d_.DrawImGui();
		fence3d_.DrawImGui();
		multiMesh3d_.DrawImGui();
		multiMaterial3d_.DrawImGui();

		// 描画
		object3d_.Draw();
		/*sphere3d_.Draw();
		teapot3d_.Draw();
		bunny3d_.Draw();
		multiMesh3d_.Draw();
		multiMaterial3d_.Draw();
		sprite_.Draw();*/
		fence3d_.Draw();

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
	multiMaterial3d_.Finalize();
	multiMesh3d_.Finalize();
	fence3d_.Finalize();
	bunny3d_.Finalize();
	teapot3d_.Finalize();
	sphere3d_.Finalize();
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
	ApplyEditorStyle();
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
