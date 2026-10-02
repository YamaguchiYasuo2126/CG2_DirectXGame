#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <array>

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



// テクスチャ付き3Dオブジェクトの初期化、更新、描画を担当。
class Object3d {
public:
	void Initialize(
		DirectXCommon* dxCommon,
		ShaderCompiler* shaderCompiler,
		Logger* logger,
		const std::string& modelName = "plane.obj",
		const std::string& objectName = "Plane",
		const std::string& modelDirectory = "resources");

	void Finalize();
	void Update();
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

	struct MeshGpuResource {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

	struct MaterialGpuResource {
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
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	std::vector<MeshGpuResource> meshResources_;
	std::vector<MaterialGpuResource> materialResources_;

	// インデックスバッファ用の変数

	// 平行光源用のリソースとデータポインタ
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	TransformationMatrix* wvpData_ = nullptr;

	// モデルディレクトリ
	std::string modelDirectory_ = "resources";

	// テクスチャリソースとSRVハンドルを2つ持つように配列化
	std::string modelName_ = "plane.obj";
	std::string objectName_ = "Plane";

	// 切り替え用のbool変数を用意する
	bool useMonsterBall_ = true;

	// オブジェクト自身とカメラのTransform
	// OBJ の Plane は +Z 側を表面としているため、起動時からカメラ側 (-Z) を向けます。
	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 3.14159265f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	// UVTransform用の変数

	// 読み込んだモデルデータを保持する変数
	ModelData modelData_;

	Matrix4x4 viewProjection_ = MatrixUtility::MakeIdentity4x4();
};
