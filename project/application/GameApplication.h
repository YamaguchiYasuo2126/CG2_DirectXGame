#pragma once

#include "DirectXCommon.h"
#include "Logger.h"
#include "Object3d.h"
#include "ShaderCompiler.h"
#include "WinApp.h"
#include "Sprite.h"
#include "SpriteRenderer.h"
#include "Input.h"
#include "DebugCamera.h"
#include "FrameTimer.h"
#include "Particle.h"
#include "WeatherSystem.h"
#include "EffectSettings.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <string>

// アプリ全体の初期化、メインループ、終了処理をまとめるクラス
class GameApplication {
public:
	using BgmControlCallback = void(*)(void*);

	// ウィンドウ、描画基盤、モデル、演出の順で準備し、初期障害物を配置する。
	void Initialize();
	// メッセージ処理が終了するまで、時間計測・入力・更新・描画を繰り返す。
	void Run();
	// GPUリソースと入力を、参照先のウィンドウより先に解放する。
	void Finalize();
	// main側の音声処理を登録する。contextの所有権は呼び出し元に残る。
	void SetBgmControls(BgmControlCallback play, BgmControlCallback stop, void* context);

private:
	enum class ObstacleType {
		Block,
		Cracker,
		Leaf,
		Fire,
		Electric,
		Magnet,
	};

	// 同じ種類のモデルを共有し、障害物ごとに位置と行列バッファだけを保持する。
	struct Obstacle {
		ObstacleType type = ObstacleType::Block;
		Vector3 position{};
		bool visible = true;
		uint64_t groupId = 0; // この障害物に属する粒をまとめて操作する識別子。
		float fireWetness = 0.0f; // 雨による消火の進行度。1に到達すると消火完了。
		float fireEmissionCredit = 0.0f; // 次のフレームへ繰り越す炎の発生数の小数部分。
		Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource;
		TransformationMatrix* transformationData = nullptr;
	};

	// 接近、入力待ち、演出中を分け、演出が終わるまで次の障害物へ進めない。
	enum class DemoState {
		ApproachingObstacle,
		WaitingForSpace,
		PlayingCracker,
		WaitingForLeaves,
		WaitingForRain,
		PlayingElectric,
		PlayingMagnet,
	};

	// シャッフルした種類の袋から1つ取り出し、進行方向の奥へ配置する。
	void SpawnNextObstacle();
	// 視界の先に障害物を補充し、通過済みの障害物と対応する粒を片付ける。
	void MaintainObstacles();
	// ImGuiの寿命と、各フレームのUI記録・描画を管理する。
	void InitializeImGui();
	void BeginImGuiFrame();
	void EndImGuiFrame();
	void FinalizeImGui();
#ifdef USE_IMGUI
	void DrawEffectEditor();
#endif

	// 各機能クラスをここで所有し、寿命をまとめて管理
	Logger logger_;
	WinApp winApp_;
	DirectXCommon dxCommon_;
	Input input_;
	bool isDebugCameraActive_ = false;
	DemoState demoState_ = DemoState::ApproachingObstacle;
	std::deque<Obstacle> obstacles_;
	std::size_t currentObstacleIndex_ = 0; // deque内で現在攻略中の障害物の位置。
	std::mt19937 obstacleRandomEngine_{ std::random_device{}() };
	std::array<ObstacleType, 12> obstacleBag_{ ObstacleType::Block, ObstacleType::Block,
		ObstacleType::Cracker, ObstacleType::Cracker, ObstacleType::Leaf, ObstacleType::Leaf,
		ObstacleType::Fire, ObstacleType::Fire, ObstacleType::Electric, ObstacleType::Electric,
		ObstacleType::Magnet, ObstacleType::Magnet };
	// 末尾に達するたび袋をシャッフルすることで、種類の偏りを抑える。
	std::size_t obstacleBagIndex_ = obstacleBag_.size();
	uint64_t nextObstacleId_ = 1;
	float nextObstacleZ_ = 0.0f;
	// 各演出の経過秒数。完了判定と段階的な発生タイミングに使う。
	float crackerEffectTime_ = 0.0f;
	float electricEffectTime_ = 0.0f;
	uint8_t electricBurstStage_ = 0;
	float magnetEffectTime_ = 0.0f;
	BgmControlCallback playBgm_ = nullptr;
	BgmControlCallback stopBgm_ = nullptr;
	void* bgmContext_ = nullptr;
	bool isBgmPlaying_ = false;
	// 自動カメラは少し高い位置から進行方向を見下ろす。
	Transform normalCameraTransform_ = {
		{ 1.0f, 1.0f, 1.0f },
		{ 0.22f, 0.0f, 0.0f },
		{ 0.0f, 1.2f, -10.0f }
	};
	DebugCamera debugCamera_;
	ShaderCompiler shaderCompiler_;
	FrameTimer frameTimer_; // 移動・寿命・発生頻度の基準となるフレーム経過秒数を供給。
	Object3d object3d_;
	Object3d block3d_;
	Object3d cracker3d_;
	Object3d sphere3d_;
	Object3d teapot3d_;
	Object3d bunny3d_;
	Object3d multiMesh3d_;
	Object3d multiMaterial3d_;
	Object3d fence3d_;
	SpriteRenderer spriteRenderer_; // 全スプライトで共有する描画設定を一つだけ所有。
	Sprite sprite_;
	// 演出の種類ごとに独立した粒プールと描画設定を持つ。
	Particle particle_;
	Particle boardParticles_;
	Particle confettiParticles_;
	Particle leafParticles_;
	Particle fireParticles_;
	Particle smokeParticles_;
	Particle electricBarrierParticles_;
	Particle electricSparkParticles_;
	Particle electricFlashParticles_;
	Particle magnetShardParticles_;
	Particle magnetCoreParticles_;
	WeatherSystem weather_;
	EffectSettings effectSettings_;
	bool effectSettingsDirty_ = false; // エディターで変更後、まだJSONへ保存していないか。
	std::string effectEditorStatus_; // 保存・読み込み結果をImGuiに表示する。
	bool initialized_ = false;
};
