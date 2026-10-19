#pragma once

#include "DirectXCommon.h"
#include "Logger.h"
#include "Object3d.h"
#include "ShaderCompiler.h"
#include "WinApp.h"
#include "Sprite.h"
#include "Input.h"
#include "DebugCamera.h"

// アプリ全体の初期化、メインループ、終了処理をまとめるクラス。
class GameApplication
{
public:
	using BgmControlCallback = void(*)(void*);
	GameApplication() = default;
	GameApplication(const GameApplication&) = delete;
	GameApplication& operator=(const GameApplication&) = delete;

	void Initialize();
	void Run();
	void Finalize();
	void SetBgmControls(BgmControlCallback play, BgmControlCallback stop, void* context);

private:
	void InitializeImGui();
	void BeginImGuiFrame();
	void EndImGuiFrame();
	void FinalizeImGui();

	// 各機能クラスをここで所有し、寿命をまとめて管理。
	Logger logger_;
	WinApp winApp_;
	DirectXCommon dxCommon_;
	Input input_;
	bool isDebugCameraActive_ = false;
	BgmControlCallback playBgm_ = nullptr;
	BgmControlCallback stopBgm_ = nullptr;
	void* bgmContext_ = nullptr;
	bool isBgmPlaying_ = false;
	// 通常カメラ用のTransform
	Transform normalCameraTransform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };
	DebugCamera debugCamera_;
	ShaderCompiler shaderCompiler_;
	// 描画対象はInitializeで生成し、Finalizeで破棄する。
	Object3d* object3d_ = nullptr;
	Object3d* sphere3d_ = nullptr;
	Object3d* teapot3d_ = nullptr;
	Object3d* bunny3d_ = nullptr;
	Object3d* multiMesh3d_ = nullptr;
	Object3d* multiMaterial3d_ = nullptr;
	Sprite* sprite_ = nullptr;
	bool initialized_ = false;
};
