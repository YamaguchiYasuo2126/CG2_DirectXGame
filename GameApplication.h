#pragma once

#include "DirectXCommon.h"
#include "Logger.h"
#include "Object3d.h"
#include "ShaderCompiler.h"
#include "WinApp.h"
#include "Sprite.h"
#include "Input.h"
#include "DebugCamera.h"
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

	void Initialize();
	void Run();
	void Finalize();
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

	struct Obstacle {
		ObstacleType type = ObstacleType::Block;
		Vector3 position{};
		bool visible = true;
		uint64_t groupId = 0;
		float fireWetness = 0.0f;
		float fireEmissionCredit = 0.0f;
		Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource;
		TransformationMatrix* transformationData = nullptr;
	};

	enum class DemoState {
		ApproachingObstacle,
		WaitingForSpace,
		PlayingCracker,
		WaitingForLeaves,
		WaitingForRain,
		PlayingElectric,
		PlayingMagnet,
	};

	void SpawnNextObstacle();
	void MaintainObstacles();
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
	std::size_t currentObstacleIndex_ = 0;
	std::mt19937 obstacleRandomEngine_{ std::random_device{}() };
	std::array<ObstacleType, 12> obstacleBag_{ ObstacleType::Block, ObstacleType::Block,
		ObstacleType::Cracker, ObstacleType::Cracker, ObstacleType::Leaf, ObstacleType::Leaf,
		ObstacleType::Fire, ObstacleType::Fire, ObstacleType::Electric, ObstacleType::Electric,
		ObstacleType::Magnet, ObstacleType::Magnet };
	std::size_t obstacleBagIndex_ = obstacleBag_.size();
	uint64_t nextObstacleId_ = 1;
	float nextObstacleZ_ = 0.0f;
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
	Object3d object3d_;
	Object3d block3d_;
	Object3d cracker3d_;
	Object3d sphere3d_;
	Object3d teapot3d_;
	Object3d bunny3d_;
	Object3d multiMesh3d_;
	Object3d multiMaterial3d_;
	Object3d fence3d_;
	Sprite sprite_;
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
	bool effectSettingsDirty_ = false;
	std::string effectEditorStatus_;
	bool initialized_ = false;
};
