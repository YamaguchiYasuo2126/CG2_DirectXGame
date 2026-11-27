#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <array>
#include <random>

#include "MatrixUtility.h"
#include "Matrix4x4.h"
#include "Transform.h"
#include "Vector4.h"
#include "VertexData.h"
#include "Material.h"
#include "TransformationMatrix.h"
#include "DirectionalLight.h"
#include "ModelData.h"
#include "ModelLoader.h"
#include "WeatherConditions.h"
#include "EffectSettings.h"

class DirectXCommon;
class Logger;
class ShaderCompiler;

// 1粒の位置・速度など、CPU側で動きを管理するパーティクルデータ
struct ParticleData
{
	Transform transform;
	Vector3 velocity; // ワールド単位/秒。UpdateでdeltaTimeを掛けて位置へ加算。
	Vector3 angularVelocity; // 各軸の回転速度。単位はラジアン/秒。
	Vector4 color;
	float lifeTime; // 寿命（秒）。currentTimeがこの値以上なら再利用可能。
	float currentTime; // 発生からの経過秒数。
	uint64_t groupId = 0; // 障害物単位で発生・除去するための識別子。
	float baseHeight = 0.0f;
	float flutterPhase = 0.0f; // 揺らぎ、または磁力の公転角の位相（ラジアン）。
	Vector3 orbitCenter{};
	float orbitRadius = 0.0f;
	float orbitSpeed = 0.0f;
	bool released = false; // 磁力の公転状態から、速度による飛散状態へ移行済みか。
};

// Particle.VS.hlslのStructuredBufferと同じ順序・レイアウトで転送する。
struct ParticleForGPU
{
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
	Vector4 color;
};

enum class ParticleEffectType
{
	Debris,
	Confetti,
	Leaf,
	Fire,
	Smoke,
	ElectricBarrier,
	ElectricSpark,
	ElectricFlash,
	MagnetShards,
	MagnetCore,
};

// 描画や更新を担当するパーティクルクラス
class Particle
{
public:

	// 外部クラスは借用する。モデル、粒の種類に対応するPSO、固定長の粒プールを準備。
	void Initialize(
		DirectXCommon* dxCommon,
		ShaderCompiler* shaderCompiler,
		Logger* logger,
		const std::string& modelName = "quad",
		const std::string& objectName = "Particle",
		const std::string& modelDirectory = "resources",
		const Vector3& modelScale = { 1.0f, 1.0f, 1.0f },
		ParticleEffectType effectType = ParticleEffectType::Debris);

	// Mapした領域とCOMリソースを解放する。借用した外部クラスは破棄しない。
	void Finalize();
	// deltaTimeは秒。drawDistanceが正の場合のみ対象演出を距離で間引く。
	// activeLeafGroupは風を受ける落ち葉のグループで、他の落ち葉はその場に残す。
	void Update(float deltaTime, const WeatherConditions& weather = {}, const Vector3& cameraPosition = {}, 
		float drawDistance = 0.0f, uint64_t activeLeafGroup = 0);
	// UpdateでGPU用に詰めた粒だけをインスタンシング描画する。
	void Draw();
	void DrawImGui();
	// 寿命切れのスロットへ生成する。空きが不足する場合は発生数がcount未満になる。
	void EmitBurst(const Vector3& center, uint32_t count, const Vector3& emissionDirection = { 0.0f, 0.0f, -1.0f });
	// グループ付き演出は、障害物の通過・消火時に対応する粒だけを操作できる。
	void EmitLeafPile(const Vector3& center, uint64_t groupId, uint32_t count);
	void EmitFire(const Vector3& center, uint64_t groupId, uint32_t count);
	void ExtinguishFire(uint64_t groupId);
	void EmitSmoke(const Vector3& center, uint32_t count);
	void EmitElectricBarrier(const Vector3& center, uint64_t groupId);
	void RemoveElectricBarrier(uint64_t groupId);
	void EmitElectricBurst(const Vector3& center, uint32_t count);
	void EmitElectricFlash(const Vector3& center);
	void EmitMagnetVortex(const Vector3& center, uint64_t groupId, uint32_t count);
	// 公転中の破片へ中心から外向きの速度を与え、飛散する状態へ切り替える。
	void ReleaseMagnetGroup(uint64_t groupId);
	void RemoveMagnetGroup(uint64_t groupId);
	void EmitMagnetCore(const Vector3& center, uint64_t groupId);
	void RemoveMagnetCore(uint64_t groupId);
	// 通り道に残る落ち葉だけを数え、風で道が開けたかの判定に使う。
	uint32_t CountLeavesInPath(uint64_t groupId, const Vector3& center) const;
	void RemoveLeafGroup(uint64_t groupId);

	void SetViewProjectionMatrix(const Matrix4x4& viewProjection) { viewProjection_ = viewProjection; }
	// 設定はコピーしない。参照先はこのParticleの利用中、有効である必要がある。
	void SetEffectSettings(const EffectSettings* settings) { effectSettings_ = settings; }

private:
	// 描画に必要なDirectXリソースを用途ごとに作成。
	void CreateRootSignature();
	void CreatePipelineState();
	void CreateConstantBuffers();
	void CreateTexture();
	void CreateVertexBuffer();
	void CreateSphereVertices();
	void CreateQuadVertices();

	// バッファとSRVを作る関数
	void CreateInstancingResource();
	// 生成関数
	ParticleData MakeNewParticle(std::mt19937& randomEngine, const Vector3& center, const Vector3& emissionDirection);
	ParticleData MakeNewLeaf(std::mt19937& randomEngine, const Vector3& center, uint64_t groupId);
	ParticleData MakeNewFire(std::mt19937& randomEngine, const Vector3& center, uint64_t groupId);
	ParticleData MakeNewSmoke(std::mt19937& randomEngine, const Vector3& center);
	ParticleData MakeNewElectricBarrier(const Vector3& center, uint64_t groupId);
	ParticleData MakeNewElectricSpark(std::mt19937& randomEngine, const Vector3& center);
	ParticleData MakeNewElectricFlash(const Vector3& center);
	ParticleData MakeNewMagnetShard(std::mt19937& randomEngine, const Vector3& center, uint64_t groupId);
	ParticleData MakeNewMagnetCore(const Vector3& center, uint64_t groupId);

	struct MeshGpuResource
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

	struct MaterialGpuResource
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU{};
		Material* materialData = nullptr;
		Transform uvTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	};

	DirectXCommon* dxCommon_ = nullptr;
	ShaderCompiler* shaderCompiler_ = nullptr;
	Logger* logger_ = nullptr;

	// Pipeline関連のリソース。
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob_;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;

	// 描画するオブジェクトが使うバッファとテクスチャ。
	std::vector<MeshGpuResource> meshResources_;
	std::vector<MaterialGpuResource> materialResources_;

	// 粒の生成に使う乱数エンジンを一度だけ初期化する
	std::mt19937 randomEngine_{ std::random_device{}() };

	// 複数の障害物の粒を同時に保持できる数
	static constexpr uint32_t kMaxInstanceCount = 256;

	// 今フレームにGPUへ送る描画対象数。距離で除外された生存粒は含まない。
	uint32_t instanceCount_ = 0;
	// 固定長プール。寿命切れの要素を再利用し、発生ごとのメモリ確保を避ける。
	std::array<ParticleData, kMaxInstanceCount> particles_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	ParticleForGPU* instancingData_ = nullptr; // Map先への借用ポインタ。resource側がメモリを所有。

	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

	// モデルディレクトリ
	std::string modelDirectory_ = "resources";
	Vector3 modelScale_{ 1.0f, 1.0f, 1.0f };
	ParticleEffectType effectType_ = ParticleEffectType::Debris;
	const EffectSettings* effectSettings_ = nullptr;

	// 描画するモデルとUI表示名
	std::string modelName_ = "quad";
	std::string objectName_ = "Particle";

	// 切り替え用のbool変数を用意する
	bool useMonsterBall_ = true;

	// オブジェクト自身とカメラのTransform
	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 3.14159265f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	// 読み込んだモデルデータを保持する変数
	ModelData modelData_;

	Matrix4x4 viewProjection_ = MatrixUtility::MakeIdentity4x4();
};
