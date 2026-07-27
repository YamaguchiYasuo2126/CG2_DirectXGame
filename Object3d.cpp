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
	CreateTexture();
	CreateVertexBuffer();
}

void Object3d::Finalize() {
	// ComPtrで管理しているDirectXリソースを明示的に解放します。
	vertexResource_.Reset();
	indexResource_.Reset();
	// 配列化したテクスチャリソースをfor文で順番に解放する
	for (uint32_t i = 0; i < 2; ++i)
	{
		textureResources_[i].Reset();
	}
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
	// 毎フレーム少しずつ回転させ、WVP行列を定数バッファへ書き込みます。
	transform_.rotate.y += 0.03f;
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

	// 変数を見て利用するSRVを決める（trueなら1番(モンスターボール)、falseなら0番(uvChecker)）
	commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall_ ? textureSrvHandleGPUs_[1] : textureSrvHandleGPUs_[0]);
	commandList->DrawIndexedInstanced(1536, 1, 0, 0, 0);
}

void Object3d::DrawImGui() {
#ifdef USE_IMGUI
	// マテリアル色をImGuiから編集できるようにします。
	ImGui::Begin("Settings");
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
		// ※Vector3クラスにNormalize()のような関数があればそれを使ってください。
		// なければ以下のように手動で計算するか、Utility関数を作ると便利です。
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
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
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

	// 平行光源用の定数バッファ作成と初期値設定
	directionalLightResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// デフォルト値
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;
}

void Object3d::CreateTexture() {
	// 読み込みたいテクスチャのパスを配列にまとめます
	std::string filePaths[2] = {
		"resources/uvChecker.png",
		"resources/monsterBall.png"
	};

	// 2枚のテクスチャを順番に処理します
	for (uint32_t i = 0; i < 2; ++i) {
		// 画像を読み込み、MipMap付きTextureResourceとしてGPUが参照できる形
		DirectX::ScratchImage mipImages = DirectXResource::LoadTexture(filePaths[i]);
		const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
		textureResources_[i] = DirectXResource::CreateTextureResource(dxCommon_->GetDevice(), metadata);
		DirectXResource::UploadTextureData(textureResources_[i].Get(), mipImages);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = metadata.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

		// 0番はImGui用に使うため、Object3dのTexture SRVは 1番以降(i + 1) へ置きます。
		dxCommon_->GetDevice()->CreateShaderResourceView(textureResources_[i].Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(i + 1));
		textureSrvHandleGPUs_[i] = dxCommon_->GetSrvGpuHandle(i + 1);
	}
}
void Object3d::CreateVertexBuffer()
{
	// 分割数と総頂点数の定義
	const uint32_t kSubdivision = 16;
	// 頂点数は (経度分割数 + 1) * (緯度分割数 + 1) になります
	const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	// インデックス数は今までと同じ 16 * 16 * 6 = 1536個
	const uint32_t kIndexCount = kSubdivision * kSubdivision * 6;

	// 球に必要な頂点数分のバッファを作成
	vertexResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * kVertexCount);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// インデックスバッファの作成
	indexResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * kIndexCount);
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexData = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

	// 経度(lon)と緯度(lat)の1分割あたりの角度を求めます。
	const float pi = 3.141592654f;
	const float kLonEvery = pi * 2.0f / float(kSubdivision);
	const float kLatEvery = pi / float(kSubdivision);

	// 緯度の方向に分割 (<= にすることで端の頂点も含める)
	for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex)
	{
		float lat = -pi / 2.0f + kLatEvery * latIndex;
		float v = 1.0f - float(latIndex) / float(kSubdivision);

		// 経度の方向に分割
		for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) 
		{
			// 配列のインデックス計算
			uint32_t index = latIndex * (kSubdivision + 1) + lonIndex;
			float lon = lonIndex * kLonEvery;
			float u = float(lonIndex) / float(kSubdivision);

			vertexData[index].position = { std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon), 1.0f };
			vertexData[index].texcoord = { u, v };
			vertexData[index].normal = { vertexData[index].position.x, vertexData[index].position.y, vertexData[index].position.z };
		}
	}

	// インデックスデータを書き込む
	uint32_t indexOffset = 0;
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex)
	{
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex)
		{
			// 四角形を構成する4頂点のインデックス番号を計算
			uint32_t start = latIndex * (kSubdivision + 1) + lonIndex;
			uint32_t a = start;                                // 左下
			uint32_t b = start + (kSubdivision + 1);           // 左上
			uint32_t c = start + 1;                            // 右下
			uint32_t d = start + (kSubdivision + 1) + 1;       // 右上

			// 1枚目の三角形 (a, b, c)
			indexData[indexOffset++] = a;
			indexData[indexOffset++] = b;
			indexData[indexOffset++] = c;

			// 2枚目の三角形 (c, b, d)
			indexData[indexOffset++] = c;
			indexData[indexOffset++] = b;
			indexData[indexOffset++] = d;
		}
	}
}