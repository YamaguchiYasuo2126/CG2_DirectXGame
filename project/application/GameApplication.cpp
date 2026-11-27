#include "GameApplication.h"
#include "DirectXResource.h"

#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
constexpr float kCrackerPitch = -1.04719755f; // 口がカメラ側を向きつつ約30度上を向く
constexpr float kCrackerMouthOffset = 0.58f; // モデルの口まで0.5、手前に0.08
constexpr float kObstacleStopDistance = 4.0f;
constexpr float kObstacleDrawDistance = 20.0f;
constexpr float kObstacleSpacing = 4.0f;
constexpr float kCrackerEffectHoldSeconds = 0.8f;
constexpr uint32_t kBoxShardCount = 28;
constexpr uint32_t kBoardShardCount = 20;
constexpr uint32_t kConfettiCount = 60;
constexpr uint32_t kLeafCount = 28;
constexpr uint32_t kLeavesToClearPath = 3;
constexpr float kFireExtinguishRate = 1.5f;
constexpr uint32_t kSmokeParticleCount = 28;
constexpr float kElectricEffectHoldSeconds = 0.82f;
constexpr float kMagnetEffectHoldSeconds = 0.75f;
const std::filesystem::path kEffectSettingsPath = "resources/effect-presets.json";
}

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

namespace
{
void ApplyEditorStyle()
{
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

void GameApplication::Initialize()
{
	// COMを利用するDirectX関連APIのために、アプリ全体で一度だけ初期化。
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// 依存される側から順番に初期化します。
	logger_.Initialize();
	winApp_.Initialize();

	input_.Initialize(&winApp_);

	dxCommon_.Initialize(winApp_.GetHwnd(), WinApp::kClientWidth, WinApp::kClientHeight, &logger_);
	shaderCompiler_.Initialize(&logger_);
	object3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "plane.obj", "Plane");
	// ブロック
	block3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "riverStone.obj", "Block", "resources/riverStone", false);
	// モデルの大きい口は+Y側。X軸回転で口を斜め上・カメラ側へ向ける。
	cracker3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"cracker.obj", "Cracker", "resources/cracker", false);
	// 球
	sphere3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "sphere", "Sphere");
	teapot3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "teapot.obj", "Utah Teapot");
	bunny3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "bunny.obj", "Stanford Bunny");
	fence3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,"fence.obj", "Fence", "resources/fence");
	multiMesh3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "multiMesh.obj", "Multi Mesh");
	multiMaterial3d_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "multiMaterial.obj", "Multi Material");
	sprite_.Initialize(&dxCommon_);
	particle_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "boxEffectGrain.obj", "Box Shards", "resources/boxEffectGrain");
	// 元の板モデルは厚みが約0.005なので、奥行きだけ拡大する。
	boardParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_, "squareBoardEffectGrain.obj", "Board Shards", "resources/squareBoardEffectGrain", { 1.0f, 1.0f, 40.0f });
	confettiParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"squareBoardEffectGrain.obj", "Confetti", "resources/squareBoardEffectGrain",
		{ 0.5f, 1.2f, 6.0f }, ParticleEffectType::Confetti);
	leafParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"leaf.obj", "Fallen Leaves", "resources/leaf",
		{ 1.0f, 1.0f, 1.0f }, ParticleEffectType::Leaf);
	fireParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"quad", "Fire", "resources", { 1.0f, 1.0f, 1.0f }, ParticleEffectType::Fire);
	smokeParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"quad", "Smoke", "resources", { 1.0f, 1.0f, 1.0f }, ParticleEffectType::Smoke);
	electricBarrierParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"quad", "Electric Barrier", "resources", { 1.0f, 1.0f, 1.0f },
		ParticleEffectType::ElectricBarrier);
	electricSparkParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"quad", "Electric Sparks", "resources", { 1.0f, 1.0f, 1.0f },
		ParticleEffectType::ElectricSpark);
	electricFlashParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"quad", "Electric Flash", "resources", { 1.0f, 1.0f, 1.0f },
		ParticleEffectType::ElectricFlash);
	magnetShardParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"boxEffectGrain.obj", "Magnetic Shards", "resources/boxEffectGrain",
		{ 1.0f, 1.0f, 1.0f }, ParticleEffectType::MagnetShards);
	magnetCoreParticles_.Initialize(&dxCommon_, &shaderCompiler_, &logger_,
		"quad", "Magnetic Core", "resources", { 1.0f, 1.0f, 1.0f },
		ParticleEffectType::MagnetCore);
	if (LoadEffectSettings(kEffectSettingsPath, effectSettings_, effectEditorStatus_)) {
		effectEditorStatus_ = "Loaded resources/effect-presets.json";
		logger_.Log(effectEditorStatus_);
	} else {
		logger_.Log("Effect presets: " + effectEditorStatus_ + "; using built-in defaults");
	}
	fireParticles_.SetEffectSettings(&effectSettings_);
	electricBarrierParticles_.SetEffectSettings(&effectSettings_);
	electricSparkParticles_.SetEffectSettings(&effectSettings_);
	electricFlashParticles_.SetEffectSettings(&effectSettings_);
	magnetShardParticles_.SetEffectSettings(&effectSettings_);
	magnetCoreParticles_.SetEffectSettings(&effectSettings_);
	weather_.Initialize(&dxCommon_, &shaderCompiler_);
	MaintainObstacles();
	debugCamera_.Initialize();
	InitializeImGui();
	frameTimer_.Initialize();
	initialized_ = true;
}

void GameApplication::SpawnNextObstacle()
{
	if (obstacleBagIndex_ == obstacleBag_.size())
	{
		std::shuffle(obstacleBag_.begin(), obstacleBag_.end(), obstacleRandomEngine_);
		obstacleBagIndex_ = 0;
	}

	obstacles_.emplace_back();
	Obstacle& obstacle = obstacles_.back();
	obstacle.type = obstacleBag_[obstacleBagIndex_++];
	obstacle.position = { 0.0f, 0.0f, nextObstacleZ_ };
	obstacle.groupId = nextObstacleId_++;
	if (obstacle.type == ObstacleType::Leaf)
	{
		leafParticles_.EmitLeafPile(obstacle.position, obstacle.groupId, kLeafCount);
		nextObstacleZ_ += kObstacleSpacing;
		return;
	}
	if (obstacle.type == ObstacleType::Fire)
	{
		nextObstacleZ_ += kObstacleSpacing;
		return;
	}
	if (obstacle.type == ObstacleType::Electric)
	{
		electricBarrierParticles_.EmitElectricBarrier(obstacle.position, obstacle.groupId);
		nextObstacleZ_ += kObstacleSpacing;
		return;
	}
	if (obstacle.type == ObstacleType::Magnet)
	{
		magnetShardParticles_.EmitMagnetVortex(obstacle.position, obstacle.groupId,
			effectSettings_.magnet.shardCount);
		magnetCoreParticles_.EmitMagnetCore(obstacle.position, obstacle.groupId);
		nextObstacleZ_ += kObstacleSpacing;
		return;
	}
	obstacle.transformationResource = DirectXResource::CreateBufferResource(
		dxCommon_.GetDevice(), sizeof(TransformationMatrix));
	const HRESULT hr = obstacle.transformationResource->Map(
		0, nullptr, reinterpret_cast<void**>(&obstacle.transformationData));
	assert(SUCCEEDED(hr));
	const Vector3 scale{ 0.5f, 0.5f, 0.5f };
	const Vector3 rotation = obstacle.type == ObstacleType::Cracker
		? Vector3{ kCrackerPitch, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 0.0f };
	obstacle.transformationData->World = MatrixUtility::MakeAffineMatrix(
		scale, rotation, obstacle.position);
	nextObstacleZ_ += kObstacleSpacing;
}

void GameApplication::MaintainObstacles()
{
	// 描画距離より少し先まで配置し、次の障害物も常に用意しておく。
	while (nextObstacleZ_ <= normalCameraTransform_.translate.z + kObstacleDrawDistance + kObstacleSpacing)
	{
		SpawnNextObstacle();
	}

	// 進行済みで描画範囲からも外れた障害物の行列バッファを解放する。
	while (currentObstacleIndex_ > 0 &&
		obstacles_.front().position.z < normalCameraTransform_.translate.z - kObstacleDrawDistance)
	{
		if (obstacles_.front().type == ObstacleType::Leaf)
		{
			leafParticles_.RemoveLeafGroup(obstacles_.front().groupId);
		}
		else if (obstacles_.front().type == ObstacleType::Electric)
		{
			electricBarrierParticles_.RemoveElectricBarrier(obstacles_.front().groupId);
		}
		else if (obstacles_.front().type == ObstacleType::Magnet)
		{
			magnetShardParticles_.RemoveMagnetGroup(obstacles_.front().groupId);
			magnetCoreParticles_.RemoveMagnetCore(obstacles_.front().groupId);
		}
		else if (obstacles_.front().transformationResource)
		{
			obstacles_.front().transformationResource->Unmap(0, nullptr);
		}
		obstacles_.pop_front();
		--currentObstacleIndex_;
	}
}

void GameApplication::SetBgmControls(BgmControlCallback play, BgmControlCallback stop, void* context)
{
	playBgm_ = play;
	stopBgm_ = stop;
	bgmContext_ = context;
	isBgmPlaying_ = false;
}

void GameApplication::Run() 
{
	constexpr float kCameraSpeed = 4.0f;

	while (winApp_.ProcessMessage())
	{
		// 秒単位の同じ時間差を、カメラ・演出・発生頻度の更新すべてへ渡す。
		const float deltaTime = frameTimer_.Tick();

		// 1フレーム分のUIとDirectX描画コマンドを積みます。
		BeginImGuiFrame();
		dxCommon_.BeginFrame();

		input_.Update();

		// ImGuiのウィンドウでカメラを切り替えられるようにする
#ifdef USE_IMGUI
		ImGui::SetNextWindowPos(ImVec2(750.0f, 74.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(350.0f, 0.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Settings");
		const float fps = ImGui::GetIO().Framerate;
		ImGui::Text("FPS: %.1f (%.2f ms/frame)", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Debug Camera Active", &isDebugCameraActive_);
		}
	if (ImGui::CollapsingHeader("Weather", ImGuiTreeNodeFlags_DefaultOpen))
		{
			weather_.DrawImGuiControls();
		}

		if (demoState_ == DemoState::WaitingForSpace)
		{
			const ObstacleType type = obstacles_[currentObstacleIndex_].type;
			ImGui::TextUnformatted(type == ObstacleType::Block
				? "SPACE: Break the block"
				: type == ObstacleType::Cracker
				? "SPACE: Fire the cracker"
				: type == ObstacleType::Electric
				? "SPACE: Discharge the electric barrier" : "SPACE: Release the magnetic vortex");
		}
		else if (demoState_ == DemoState::WaitingForLeaves)
		{
			const Obstacle& obstacle = obstacles_[currentObstacleIndex_];
			ImGui::Text("Use Wind X / Z to clear leaves: %u remain",
				leafParticles_.CountLeavesInPath(obstacle.groupId, obstacle.position));
		}
		else if (demoState_ == DemoState::WaitingForRain)
		{
			ImGui::Text("Set Weather to Rain to extinguish fire: %.0f%%",
				obstacles_[currentObstacleIndex_].fireWetness * 100.0f);
		}

		if (ImGui::CollapsingHeader("BGM", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Play") && playBgm_ != nullptr)
			{
				playBgm_(bgmContext_);
				isBgmPlaying_ = true;
			}

			ImGui::SameLine();
			if (ImGui::Button("Stop") && stopBgm_ != nullptr)
			{
				stopBgm_(bgmContext_);
				isBgmPlaying_ = false;
			}

			ImGui::Text("Status: %s", isBgmPlaying_ ? "Playing" : "Stopped");
		}

		ImGui::End();
		DrawEffectEditor();
#endif
		const WeatherConditions weatherConditions = weather_.GetConditions();

		if (!isDebugCameraActive_)
		{
			Obstacle& currentObstacle = obstacles_[currentObstacleIndex_];
			// 障害物ごとに待つ条件を切り替える。演出の完了時だけインデックスを進める。
			switch (demoState_)
			{
			case DemoState::ApproachingObstacle:
				normalCameraTransform_.translate.z = (std::min)(
					normalCameraTransform_.translate.z + kCameraSpeed * deltaTime,
					currentObstacle.position.z - kObstacleStopDistance);
				if (normalCameraTransform_.translate.z >= currentObstacle.position.z - kObstacleStopDistance)
				{
					demoState_ = currentObstacle.type == ObstacleType::Leaf
						? DemoState::WaitingForLeaves
						: currentObstacle.type == ObstacleType::Fire
						? DemoState::WaitingForRain : DemoState::WaitingForSpace;
				}
				break;
			case DemoState::WaitingForSpace:
				if (input_.TriggerKey(DIK_SPACE))
				{
					if (currentObstacle.type == ObstacleType::Block)
					{
						particle_.EmitBurst(currentObstacle.position, kBoxShardCount);
						boardParticles_.EmitBurst(currentObstacle.position, kBoardShardCount);
						currentObstacle.visible = false;
						++currentObstacleIndex_;
						demoState_ = DemoState::ApproachingObstacle;
					}
					else if (currentObstacle.type == ObstacleType::Cracker)
					{
						const Vector3 mouthDirection{ 0.0f, std::cos(kCrackerPitch), std::sin(kCrackerPitch) };
						const Vector3 mouthPosition{
							currentObstacle.position.x,
							currentObstacle.position.y + mouthDirection.y * kCrackerMouthOffset,
							currentObstacle.position.z + mouthDirection.z * kCrackerMouthOffset
						};
						confettiParticles_.EmitBurst(mouthPosition, kConfettiCount, mouthDirection);
						crackerEffectTime_ = 0.0f;
						demoState_ = DemoState::PlayingCracker;
					}
					else if (currentObstacle.type == ObstacleType::Electric)
					{
						electricBarrierParticles_.RemoveElectricBarrier(currentObstacle.groupId);
						electricSparkParticles_.EmitElectricBurst(currentObstacle.position, effectSettings_.electric.firstSparkCount);
						electricFlashParticles_.EmitElectricFlash(currentObstacle.position);
						currentObstacle.visible = false;
						electricEffectTime_ = 0.0f;
						electricBurstStage_ = 0;
						demoState_ = DemoState::PlayingElectric;
					}
					else if (currentObstacle.type == ObstacleType::Magnet)
					{
						magnetShardParticles_.ReleaseMagnetGroup(currentObstacle.groupId);
						magnetCoreParticles_.RemoveMagnetCore(currentObstacle.groupId);
						currentObstacle.visible = false;
						magnetEffectTime_ = 0.0f;
						demoState_ = DemoState::PlayingMagnet;
					}
				}
				break;
			case DemoState::PlayingCracker:
				crackerEffectTime_ += deltaTime;
				if (crackerEffectTime_ >= kCrackerEffectHoldSeconds)
				{
					++currentObstacleIndex_;
					demoState_ = DemoState::ApproachingObstacle;
				}
				break;
			case DemoState::WaitingForLeaves:
				if (leafParticles_.CountLeavesInPath(currentObstacle.groupId,
					currentObstacle.position) <= kLeavesToClearPath)
				{
					++currentObstacleIndex_;
					demoState_ = DemoState::ApproachingObstacle;
				}
				break;
			case DemoState::WaitingForRain:
				// 雨が強いほど速く消火する。雨を止めると徐々に乾く。
				currentObstacle.fireWetness = (std::clamp)(
					currentObstacle.fireWetness +
					(weatherConditions.rainIntensity * kFireExtinguishRate -
						(weatherConditions.rainIntensity == 0.0f ? 0.15f : 0.0f)) * deltaTime,
					0.0f, 1.0f);
				if (currentObstacle.fireWetness >= 1.0f)
				{
					fireParticles_.ExtinguishFire(currentObstacle.groupId);
					smokeParticles_.EmitSmoke(currentObstacle.position, kSmokeParticleCount);
					currentObstacle.visible = false;
					++currentObstacleIndex_;
					demoState_ = DemoState::ApproachingObstacle;
				}
				break;
			case DemoState::PlayingElectric:
				electricEffectTime_ += deltaTime;
				// 閾値を超えたかで判定し、フレームが閾値と一致しなくても各段階を一度ずつ発火。
				if (electricBurstStage_ == 0 && electricEffectTime_ >= 0.14f)
				{
					electricSparkParticles_.EmitElectricBurst(
						currentObstacle.position, effectSettings_.electric.secondSparkCount);
					electricFlashParticles_.EmitElectricFlash(currentObstacle.position);
					electricBurstStage_ = 1;
				}
				if (electricBurstStage_ == 1 && electricEffectTime_ >= 0.31f)
				{
					electricSparkParticles_.EmitElectricBurst(
						currentObstacle.position, effectSettings_.electric.finalSparkCount);
					electricBurstStage_ = 2;
				}
				if (electricEffectTime_ >= kElectricEffectHoldSeconds)
				{
					++currentObstacleIndex_;
					demoState_ = DemoState::ApproachingObstacle;
				}
				break;
			case DemoState::PlayingMagnet:
				magnetEffectTime_ += deltaTime;
				if (magnetEffectTime_ >= kMagnetEffectHoldSeconds)
				{
					++currentObstacleIndex_;
					demoState_ = DemoState::ApproachingObstacle;
				}
				break;
			}
		}
		MaintainObstacles();

		Matrix4x4 viewProjection;
		Matrix4x4 cameraWorld;

		if (isDebugCameraActive_)
		{
			// デバッグカメラがONのとき
			debugCamera_.Update(&input_, deltaTime);
			cameraWorld = MatrixUtility::Inverse(debugCamera_.GetMatView());
			viewProjection = debugCamera_.GetViewProjectionMatrix();
		}
		else
		{
			// デバッグカメラがOFFのとき（通常のカメラ）
			// 通常カメラの行列を計算する
			cameraWorld = MatrixUtility::MakeAffineMatrix(
				normalCameraTransform_.scale,
				normalCameraTransform_.rotate,
				normalCameraTransform_.translate
			);

			Matrix4x4 viewMatrix = MatrixUtility::Inverse(cameraWorld);
			Matrix4x4 projectionMatrix = MatrixUtility::MakePerspectiveFovMatrix(
				0.45f,
				float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
				0.1f,
				100.0f
			);
			viewProjection = MatrixUtility::Multiply(viewMatrix, projectionMatrix);
		}
		const Vector3& cameraPosition = isDebugCameraActive_
			? debugCamera_.GetTranslation() : normalCameraTransform_.translate;
		weather_.Update(deltaTime, cameraPosition, cameraWorld, viewProjection);

		// 決定した行列をオブジェクトに渡す
		object3d_.SetViewProjectionMatrix(viewProjection);
		sphere3d_.SetViewProjectionMatrix(viewProjection);
		teapot3d_.SetViewProjectionMatrix(viewProjection);
		bunny3d_.SetViewProjectionMatrix(viewProjection);
		fence3d_.SetViewProjectionMatrix(viewProjection);
		multiMesh3d_.SetViewProjectionMatrix(viewProjection);
		multiMaterial3d_.SetViewProjectionMatrix(viewProjection);
		particle_.SetViewProjectionMatrix(viewProjection);
		boardParticles_.SetViewProjectionMatrix(viewProjection);
		confettiParticles_.SetViewProjectionMatrix(viewProjection);
		leafParticles_.SetViewProjectionMatrix(viewProjection);
		fireParticles_.SetViewProjectionMatrix(viewProjection);
		smokeParticles_.SetViewProjectionMatrix(viewProjection);
		electricBarrierParticles_.SetViewProjectionMatrix(viewProjection);
		electricSparkParticles_.SetViewProjectionMatrix(viewProjection);
		electricFlashParticles_.SetViewProjectionMatrix(viewProjection);
		magnetShardParticles_.SetViewProjectionMatrix(viewProjection);
		magnetCoreParticles_.SetViewProjectionMatrix(viewProjection);

		// ゲーム側の更新と描画です。描画対象が増えたらこの周辺に追加します。
		object3d_.Update();
		sphere3d_.Update();
		teapot3d_.Update();
		bunny3d_.Update();
		fence3d_.Update();
		multiMesh3d_.Update();
		multiMaterial3d_.Update();
		sprite_.Update();
		particle_.Update(deltaTime);
		boardParticles_.Update(deltaTime);
		confettiParticles_.Update(deltaTime, weatherConditions);
		const uint64_t activeLeafGroup = !isDebugCameraActive_ &&
			demoState_ == DemoState::WaitingForLeaves
			? obstacles_[currentObstacleIndex_].groupId : 0;
		leafParticles_.Update(deltaTime, weatherConditions, cameraPosition,
			kObstacleDrawDistance, activeLeafGroup);
		// 描画範囲の炎だけを継続生成する。消火が進むと粒数も減る。
		for (Obstacle& obstacle : obstacles_)
		{
			if (obstacle.type != ObstacleType::Fire || !obstacle.visible)
			{
				continue;
			}
			const float dx = obstacle.position.x - cameraPosition.x;
			const float dz = obstacle.position.z - cameraPosition.z;
			if (dx * dx + dz * dz > kObstacleDrawDistance * kObstacleDrawDistance)
			{
				continue;
			}
			// 発生数の小数部分を保持し、FPSにかかわらず秒あたりの炎の量を揃える。
			obstacle.fireEmissionCredit += effectSettings_.fire.particlesPerSecond *
				(1.0f - obstacle.fireWetness) * deltaTime;
			const uint32_t count = static_cast<uint32_t>(obstacle.fireEmissionCredit);
			obstacle.fireEmissionCredit -= static_cast<float>(count);
			if (count > 0)
			{
				fireParticles_.EmitFire(obstacle.position, obstacle.groupId, count);
			}
		}
		fireParticles_.Update(deltaTime, weatherConditions, cameraPosition, kObstacleDrawDistance);
		smokeParticles_.Update(deltaTime, weatherConditions, cameraPosition, kObstacleDrawDistance);
		electricBarrierParticles_.Update(deltaTime, weatherConditions, cameraPosition,
			kObstacleDrawDistance);
		electricSparkParticles_.Update(deltaTime, weatherConditions, cameraPosition,
			kObstacleDrawDistance);
		electricFlashParticles_.Update(deltaTime, weatherConditions, cameraPosition,
			kObstacleDrawDistance);
		magnetShardParticles_.Update(deltaTime, weatherConditions, cameraPosition,
			kObstacleDrawDistance);
		magnetCoreParticles_.Update(deltaTime, weatherConditions, cameraPosition,
			kObstacleDrawDistance);

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
		// カメラの描画距離内にある障害物をすべて描く。
		for (Obstacle& obstacle : obstacles_)
		{
			if (!obstacle.visible || obstacle.type == ObstacleType::Leaf ||
				obstacle.type == ObstacleType::Fire || obstacle.type == ObstacleType::Electric ||
				obstacle.type == ObstacleType::Magnet)
			{
				continue;
			}
			const float dx = obstacle.position.x - cameraPosition.x;
			const float dy = obstacle.position.y - cameraPosition.y;
			const float dz = obstacle.position.z - cameraPosition.z;
			if (dx * dx + dy * dy + dz * dz > kObstacleDrawDistance * kObstacleDrawDistance)
			{
				continue;
			}
			obstacle.transformationData->WVP = MatrixUtility::Multiply(
				obstacle.transformationData->World, viewProjection);
			if (obstacle.type == ObstacleType::Block)
			{
				block3d_.Draw(obstacle.transformationResource->GetGPUVirtualAddress());
			}
			else if (obstacle.type == ObstacleType::Cracker)
			{
				cracker3d_.Draw(obstacle.transformationResource->GetGPUVirtualAddress());
			}
		}
		/*sphere3d_.Draw();
		teapot3d_.Draw();
		bunny3d_.Draw();
		multiMesh3d_.Draw();
		multiMaterial3d_.Draw();
		sprite_.Draw();*/
		//fence3d_.Draw();
		// 不透明なブロックの後に、立体の破片を描画する。
		particle_.Draw();
		boardParticles_.Draw();
		magnetShardParticles_.Draw();
		confettiParticles_.Draw();
		leafParticles_.Draw();
		fireParticles_.Draw();
		smokeParticles_.Draw();
		electricBarrierParticles_.Draw();
		electricSparkParticles_.Draw();
		electricFlashParticles_.Draw();
		magnetCoreParticles_.Draw();
		weather_.Draw();

		EndImGuiFrame();
		dxCommon_.EndFrame();
	}
}

void GameApplication::Finalize()
{
	if (!initialized_)
	{
		return;
	}

	// 初期化と逆順に解放します。参照先が先に消えないようにするためです。
	FinalizeImGui();
	weather_.Finalize();
	for (Obstacle& obstacle : obstacles_)
	{
		if (obstacle.transformationResource)
		{
			obstacle.transformationResource->Unmap(0, nullptr);
		}
	}
	obstacles_.clear();
	magnetCoreParticles_.Finalize();
	magnetShardParticles_.Finalize();
	electricFlashParticles_.Finalize();
	electricSparkParticles_.Finalize();
	electricBarrierParticles_.Finalize();
	smokeParticles_.Finalize();
	fireParticles_.Finalize();
	leafParticles_.Finalize();
	confettiParticles_.Finalize();
	boardParticles_.Finalize();
	particle_.Finalize();
	sprite_.Finalize();
	multiMaterial3d_.Finalize();
	multiMesh3d_.Finalize();
	fence3d_.Finalize();
	bunny3d_.Finalize();
	teapot3d_.Finalize();
	sphere3d_.Finalize();
	cracker3d_.Finalize();
	block3d_.Finalize();
	object3d_.Finalize();
	shaderCompiler_.Finalize();
	dxCommon_.Finalize();
	input_.Finalize();
	winApp_.Finalize();
	logger_.Log("Hello,DirectX!\n");
	logger_.Finalize();
	CoUninitialize();
	initialized_ = false;
}

#ifdef USE_IMGUI
void GameApplication::DrawEffectEditor()
{
	ImGui::SetNextWindowPos(ImVec2(20.0f, 74.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350.0f, 420.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Effect Editor");
	bool changed = false;
	if (ImGui::BeginTabBar("Effects")) {
		if (ImGui::BeginTabItem("Fire")) {
			changed |= ImGui::SliderFloat("Particles / sec", &effectSettings_.fire.particlesPerSecond, 0.0f, 160.0f, "%.0f");
			changed |= ImGui::SliderFloat("Size##Fire", &effectSettings_.fire.sizeScale, 0.25f, 3.0f);
			changed |= ImGui::SliderFloat("Rise speed", &effectSettings_.fire.riseSpeedScale, 0.2f, 3.0f);
			changed |= ImGui::SliderFloat("Lifetime##Fire", &effectSettings_.fire.lifetimeScale, 0.3f, 3.0f);
			changed |= ImGui::ColorEdit3("Tint##Fire", &effectSettings_.fire.tint.x);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Electric")) {
			int first = static_cast<int>(effectSettings_.electric.firstSparkCount);
			int second = static_cast<int>(effectSettings_.electric.secondSparkCount);
			int final = static_cast<int>(effectSettings_.electric.finalSparkCount);
			if (ImGui::SliderInt("First sparks", &first, 0, 128)) {
				effectSettings_.electric.firstSparkCount = static_cast<uint32_t>(first); changed = true;
			}
			if (ImGui::SliderInt("Second sparks", &second, 0, 96)) {
				effectSettings_.electric.secondSparkCount = static_cast<uint32_t>(second); changed = true;
			}
			if (ImGui::SliderInt("Final sparks", &final, 0, 96)) {
				effectSettings_.electric.finalSparkCount = static_cast<uint32_t>(final); changed = true;
			}
			changed |= ImGui::SliderFloat("Size##Electric", &effectSettings_.electric.sizeScale, 0.25f, 3.0f);
			changed |= ImGui::SliderFloat("Spark speed", &effectSettings_.electric.speedScale, 0.2f, 3.0f);
			changed |= ImGui::SliderFloat("Lifetime##Electric", &effectSettings_.electric.lifetimeScale, 0.3f, 3.0f);
			changed |= ImGui::ColorEdit3("Tint##Electric", &effectSettings_.electric.tint.x);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Magnet")) {
			int count = static_cast<int>(effectSettings_.magnet.shardCount);
			if (ImGui::SliderInt("Shards", &count, 0, 64)) {
				effectSettings_.magnet.shardCount = static_cast<uint32_t>(count); changed = true;
			}
			changed |= ImGui::SliderFloat("Size##Magnet", &effectSettings_.magnet.sizeScale, 0.25f, 3.0f);
			changed |= ImGui::SliderFloat("Orbit radius", &effectSettings_.magnet.orbitRadiusScale, 0.5f, 2.0f);
			changed |= ImGui::SliderFloat("Orbit speed", &effectSettings_.magnet.orbitSpeedScale, 0.2f, 3.0f);
			changed |= ImGui::SliderFloat("Release speed", &effectSettings_.magnet.releaseSpeedScale, 0.2f, 3.0f);
			changed |= ImGui::ColorEdit3("Tint##Magnet", &effectSettings_.magnet.tint.x);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	if (changed) { effectSettingsDirty_ = true; }
	ImGui::Separator();
	if (ImGui::Button("Save JSON")) {
		if (SaveEffectSettings(kEffectSettingsPath, effectSettings_, effectEditorStatus_)) {
			effectSettingsDirty_ = false;
			effectEditorStatus_ = "Saved resources/effect-presets.json";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload JSON")) {
		if (LoadEffectSettings(kEffectSettingsPath, effectSettings_, effectEditorStatus_)) {
			effectSettingsDirty_ = false;
			effectEditorStatus_ = "Reloaded resources/effect-presets.json";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Defaults")) {
		effectSettings_ = EffectSettings{};
		effectSettingsDirty_ = true;
		effectEditorStatus_ = "Default values applied (not saved)";
	}
	ImGui::Text("Status: %s", effectSettingsDirty_ ? "Unsaved changes" : "Saved / loaded");
	if (!effectEditorStatus_.empty()) { ImGui::TextWrapped("%s", effectEditorStatus_.c_str()); }
	ImGui::TextWrapped("Live: fire rate, size, tint, orbit. Counts and new-particle lifetime apply to future spawns.");
	ImGui::End();
}
#endif

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
