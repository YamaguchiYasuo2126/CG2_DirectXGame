#pragma once

#include "DirectXCommon.h"
#include "Logger.h"
#include "Object3d.h"
#include "ShaderCompiler.h"
#include "WinApp.h"

// アプリ全体の初期化、メインループ、終了処理をまとめるクラスです。
class GameApplication {
public:
	void Initialize();
	void Run();
	void Finalize();

private:
	void InitializeImGui();
	void BeginImGuiFrame();
	void EndImGuiFrame();
	void FinalizeImGui();

	// 各機能クラスをここで所有し、寿命をまとめて管理します。
	Logger logger_;
	WinApp winApp_;
	DirectXCommon dxCommon_;
	ShaderCompiler shaderCompiler_;
	Object3d object3d_;
	bool initialized_ = false;
};
