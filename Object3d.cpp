#include "Object3d.h"

#include <cassert>
#include <dxcapi.h>
#include <cmath>

#include "DirectXCommon.h"
#include "DirectXResource.h"
#include "Logger.h"
#include "MatrixUtility.h"
#include "ShaderCompiler.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Object3d::Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler, Logger* logger) {
	dxCommon_ = dxCommon;
	shaderCompiler_ = shaderCompiler;
	logger_ = logger;

	// 描画に必要なPipeline、定数、テクスチャ、頂点をまとめて準備します。
	CreateRootSignature();
	CreatePipelineState();
	CreateConstantBuffers();
	CreateVertexBuffer();
	CreateTexture();
}

void Object3d::Finalize() {
	// ComPtrで管理しているDirectXリソースを明示的に解放します。
	vertexResource_.Reset();
	indexResource_.Reset();
	// 読み込んだテクスチャリソースを解放する
	textureResources_[0].Reset();
	materialResource_.Reset();
	wvpResource_.Reset();
	directionalLightResource_.Reset();
	graphicsPipelineState_.Reset();
	pixelShaderBlob_.Reset();
	vertexShaderBlob_.Reset();
	rootSignature_.Reset();
	errorBlob_.Reset();
	signatureBlob_.Reset();
	logger_ = nullptr;
	shaderCompiler_ = nullptr;
	dxCommon_ = nullptr;
}

void Object3d::Update() {
	// WVP行列を定数バッファへ書き込みます。

	Matrix4x4 worldMatrix = MatrixUtility::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 cameraMatrix = MatrixUtility::MakeAffineMatrix(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Matrix4x4 viewMatrix = MatrixUtility::Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = MatrixUtility::MakePerspectiveFovMatrix(
		0.45f,
		float(dxCommon_->GetWidth()) / float(dxCommon_->GetHeight()),
		0.1f,
		100.0f);
	Matrix4x4 worldViewProjectionMatrix = MatrixUtility::Multiply(worldMatrix, MatrixUtility::Multiply(viewMatrix, projectionMatrix));
	wvpData_->WVP = worldViewProjectionMatrix;
	wvpData_->World = worldMatrix;

	// UVTransformの行列計算と定数バッファへの代入
	Matrix4x4 uvTransformMatrix = MatrixUtility::MakeAffineMatrix(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);
	materialData_->uvTransform = uvTransformMatrix;
}



void Object3d::Draw() {
	// Pipelineと各種バッファを設定し、頂点6個分の三角形を描画します。
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	// インデックスバッファをセット
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// 3番のRootParameter（平行光源用）を設定
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// テクスチャを直接設定（0番のインデックスに格納されたSRVを使用）
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPUs_[0]);

	// 描画する頂点数はmodelData_.vertices.size()を指定
	commandList->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

void Object3d::DrawImGui() {
#ifdef USE_IMGUI
	// マテリアル色をImGuiから編集できるようにします。
	ImGui::Begin("Settings");

	// モデル（Sphere）の回転をImGuiで操作できるようにする
	ImGui::SliderAngle("SphereRotateX", &transform_.rotate.x);
	ImGui::SliderAngle("SphereRotateY", &transform_.rotate.y);
	ImGui::SliderAngle("SphereRotateZ", &transform_.rotate.z);

	ImGui::ColorEdit4("material", &materialData_->color.x);

	// 切り替え用のチェックボックス
	ImGui::Checkbox("useMonsterBall", &useMonsterBall_);

	// 最初から開かれていて、ハイライトされている状態
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected;

	// ライトの設定
	if (ImGui::TreeNodeEx("Directional Light", flags))
	{
		ImGui::ColorEdit4("Color", &directionalLightData_->color.x);

		// 向きの変更
		ImGui::DragFloat3("Direction", &directionalLightData_->direction.x, 0.01f, -1.0f, 1.0f);

		// 正規化（ベクトルの長さを1にする）
		float len = std::sqrt(
			directionalLightData_->direction.x * directionalLightData_->direction.x +
			directionalLightData_->direction.y * directionalLightData_->direction.y +
			directionalLightData_->direction.z * directionalLightData_->direction.z
		);
		if (len > 0.0f) {
			directionalLightData_->direction.x /= len;
			directionalLightData_->direction.y /= len;
			directionalLightData_->direction.z /= len;
		}

		// 輝度の変更
		ImGui::DragFloat("Intensity", &directionalLightData_->intensity, 0.01f);

		ImGui::TreePop();
	}

	ImGui::End();
#endif
}

void Object3d::CreateRootSignature() {
	// Texture SRVをPixelShaderへ渡すためのDescriptorTableです。
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	// 0番: PixelShader用マテリアル色、1番: VertexShader用WVP行列。
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;       // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;    // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;                       // レジスタ番号1を使

	// Textureのサンプリング方法をRootSignatureに固定で持たせます。
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// RootSignatureは一度シリアライズしてからGPU用オブジェクトを作成します。
	HRESULT hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob_,
		&errorBlob_);
	if (FAILED(hr)) {
		logger_->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob_->GetBufferPointer(),
		signatureBlob_->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3d::CreatePipelineState() {
	// 頂点データの並びをShaderの入力セマンティクスへ対応付けます。
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// 裏面カリングあり、三角形を塗りつぶしで描画します。
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// VertexShaderとPixelShaderをコンパイルしてPipelineへ組み込みます。
	vertexShaderBlob_ = shaderCompiler_->Compile(L"Object3D.VS.hlsl", L"vs_6_0");
	pixelShaderBlob_ = shaderCompiler_->Compile(L"Object3D.PS.hlsl", L"ps_6_0");

	// RootSignature、Shader、Blend、RasterizerなどをまとめてPSOを作成します。
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = dxCommon_->GetRenderTargetFormat();
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 奥行きが近いものを優先して描画するため、DepthTestを有効にします。
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void Object3d::CreateConstantBuffers() {
	// WVP行列用の定数バッファを作り、CPUから直接書き込めるようMapします。
	wvpResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	wvpData_->WVP = MatrixUtility::MakeIdentity4x4();
	wvpData_->World = MatrixUtility::MakeIdentity4x4();

	// マテリアル色用の定数バッファです。初期値は白にしています。
	materialResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// Vector4を直接代入するのではなく、メンバ変数(colorとenableLighting)それぞれに代入
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;

	// UVTransform用の行列を単位行列で初期化する
	materialData_->uvTransform = MatrixUtility::MakeIdentity4x4();

	// 平行光源用の定数バッファ作成と初期値設定
	directionalLightResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// デフォルト値
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;
}

void Object3d::CreateTexture() {
	// modelData_からテクスチャのパスを取得
	std::string filePath = modelData_.material.textureFilePath;

	// 画像を読み込み、MipMap付きTextureResourceとしてGPUが参照できる形にする
	DirectX::ScratchImage mipImages = DirectXResource::LoadTexture(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// textureResources_[0]に読み込んだテクスチャを保存
	textureResources_[0] = DirectXResource::CreateTextureResource(dxCommon_->GetDevice(), metadata);
	DirectXResource::UploadTextureData(textureResources_[0].Get(), mipImages);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// 0番はImGui用に使うため、Object3dのTextureSRVは1番へ置く
	dxCommon_->GetDevice()->CreateShaderResourceView(textureResources_[0].Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(1));
	textureSrvHandleGPUs_[0] = dxCommon_->GetSrvGpuHandle(1);
}

void Object3d::CreateVertexBuffer()
{
	// モデルファイルの読み込み
	modelData_ = ModelLoader::LoadObjFile("resources", "plane.obj");

	// 頂点バッファの作成 (頂点数 × VertexDataのサイズ)
	vertexResource_ = DirectXResource::CreateBufferResource(
		dxCommon_->GetDevice(),
		sizeof(VertexData) * modelData_.vertices.size()
	);

	// 頂点バッファビューの設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// memcpyを使って、modelDataの中身を一気にコピー
	std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}