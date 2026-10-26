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

class DirectXCommon;
class Logger;
class ShaderCompiler;

// 1粒の位置・速度など、CPU側で動きを管理するパーティクルデータ
struct ParticleData
{
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

struct ParticleForGPU
{
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

// 描画や更新を担当するパーティクルクラス
class Particle
{
public:

	void Initialize(
		DirectXCommon* dxCommon,
		ShaderCompiler* shaderCompiler,
		Logger* logger,
		const std::string& modelName = "plane.obj",
		const std::string& objectName = "Plane",
		const std::string& modelDirectory = "resources");

	void Finalize();
	void Update(bool moveParticles);
	void Draw();
	void DrawImGui();

	void SetViewProjectionMatrix(const Matrix4x4& viewProjection) { viewProjection_ = viewProjection; }

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
	ParticleData MakeNewParticle(std::mt19937& randomEngine);

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

	// バッファに確保できるパーティクルの最大数
	static constexpr uint32_t kMaxInstanceCount = 10;

	// 生存中のパーティクルの数
	uint32_t instanceCount_ = kMaxInstanceCount;
	// パーティクルのメンバ変数
	std::array<ParticleData, kMaxInstanceCount> particles_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	ParticleForGPU* instancingData_ = nullptr;

	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

	// モデルディレクトリ
	std::string modelDirectory_ = "resources";

	// テクスチャリソースとSRVハンドルを2つ持つように配列化
	std::string modelName_ = "plane.obj";
	std::string objectName_ = "Plane";

	// 切り替え用のbool変数を用意する
	bool useMonsterBall_ = true;

	// オブジェクト自身とカメラのTransform
	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 3.14159265f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	// 読み込んだモデルデータを保持する変数
	ModelData modelData_;

	Matrix4x4 viewProjection_ = MatrixUtility::MakeIdentity4x4();

};

