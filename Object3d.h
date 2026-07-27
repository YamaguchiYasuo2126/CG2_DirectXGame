#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <array>
#include <string>

#include "Matrix4x4.h"
#include "Transform.h"
#include "Vector4.h"
#include "VertexData.h"
#include "Material.h"
#include "TransformationMatrix.h"
#include "DirectionalLight.h"

class DirectXCommon;
class Logger;
class ShaderCompiler;


// テクスチャ付き3Dオブジェクトの初期化、更新、描画を担当します。
class Object3d {
public:
	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler, Logger* logger);
	void Finalize();
	void Update();
	void Draw();
	void DrawImGui();

private:
	// 描画に必要なDirectXリソースを用途ごとに作成します。
	void CreateRootSignature();
	void CreatePipelineState();
	void CreateConstantBuffers();
	void CreateTexture();
	void CreateVertexBuffer();

	DirectXCommon* dxCommon_ = nullptr;
	ShaderCompiler* shaderCompiler_ = nullptr;
	Logger* logger_ = nullptr;

	// Pipeline関連のリソースです。
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob_;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;

	// 描画するオブジェクトが使うバッファとテクスチャです。
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

	// 平行光源用のリソースとデータポインタ
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	TransformationMatrix* wvpData_ = nullptr;
	Material* materialData_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// テクスチャリソースとSRVハンドルを2つ持つように配列化
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> textureResources_;
	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 2> textureSrvHandleGPUs_{};

	// 切り替え用のbool変数を用意する
	bool useMonsterBall_ = true;

	// オブジェクト自身とカメラのTransform
	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };
};
