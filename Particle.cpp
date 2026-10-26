#include "Particle.h"

#include <cassert>
#include <cmath>
#include <cstring>

#include "DirectXCommon.h"
#include "DirectXResource.h"
#include "Logger.h"
#include "ShaderCompiler.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Particle::Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler, Logger* logger,
	const std::string& modelName, const std::string& objectName, const std::string& modelDirectory)
{
	dxCommon_ = dxCommon;
	shaderCompiler_ = shaderCompiler;
	logger_ = logger;
	modelName_ = modelName;
	objectName_ = objectName;
	modelDirectory_ = modelDirectory;

	CreateRootSignature();
	CreatePipelineState();
	CreateVertexBuffer();	 // メッシュとマテリアル情報も生成
	CreateConstantBuffers(); // MaterialのConstantBufferを作成
	CreateTexture();
	CreateInstancingResource();
}

void Particle::Finalize()
{
	meshResources_.clear();
	materialResources_.clear();
	instancingResource_.Reset();
	graphicsPipelineState_.Reset();
	pixelShaderBlob_.Reset();
	vertexShaderBlob_.Reset();
	rootSignature_.Reset();
	errorBlob_.Reset();
	signatureBlob_.Reset();
	logger_ = nullptr;
	shaderCompiler_ = nullptr;
	dxCommon_ = nullptr;
	instancingData_ = nullptr;
}

void Particle::Update(bool moveParticles)
{
	assert(instanceCount_ <= kMaxInstanceCount);

	// Δtを定義
	constexpr float kDeltaTime = 1.0f / 60.0f;

	// 毎フレーム描画数を数え直す
	instanceCount_ = 0;

	for (uint32_t i = 0; i < kMaxInstanceCount; ++i)
	{

		ParticleData& particle = particles_[i];

		if (particle.currentTime >= particle.lifeTime)
		{
			continue; // 寿命が切れた粒は描画しない
		}

		if (moveParticles)
		{
			particle.currentTime += kDeltaTime;

			if (particle.currentTime >= particle.lifeTime)
			{
				continue;
			}

			// 速度を位置に反映
			particle.transform.translate.x += particle.velocity.x * kDeltaTime;
			particle.transform.translate.y += particle.velocity.y * kDeltaTime;
			particle.transform.translate.z += particle.velocity.z * kDeltaTime;
		}

		const Matrix4x4 world = MatrixUtility::MakeAffineMatrix(
			particles_[i].transform.scale,
			particles_[i].transform.rotate,
			particles_[i].transform.translate);

		instancingData_[instanceCount_].World = world;
		instancingData_[instanceCount_].WVP = MatrixUtility::Multiply(world, viewProjection_);
		instancingData_[instanceCount_].color = particle.color;
		instancingData_[instanceCount_].color.w = 1.0f - (particle.currentTime / particle.lifeTime);


		++instanceCount_;
	}

	// Object3dから流用したMaterialのUV更新
	for (MaterialGpuResource& material : materialResources_)
	{
		material.materialData->uvTransform = MatrixUtility::MakeAffineMatrix(
			material.uvTransform.scale,
			material.uvTransform.rotate,
			material.uvTransform.translate);
	}
}

void Particle::Draw()
{
	assert(instanceCount_ <= kMaxInstanceCount);

	if (instanceCount_ == 0)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const auto& mesh = meshResources_[0];
	const auto& material = materialResources_[0];

	commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);

	// [0] Material
	commandList->SetGraphicsRootConstantBufferView(0, material.materialResource->GetGPUVirtualAddress());

	// [1] 行列配列のSRV
	commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

	// [2] テクスチャのSRV
	commandList->SetGraphicsRootDescriptorTable(2, material.textureSrvHandleGPU);

	commandList->DrawInstanced(static_cast<UINT>(modelData_.meshes[0].vertices.size()), instanceCount_, 0, 0);
}

void Particle::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Settings");
	ImGui::PushID(objectName_.c_str());
	if (ImGui::CollapsingHeader(objectName_.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate (rad)", &transform_.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.01f, 100.0f);

		if (ImGui::TreeNode("Materials"))
		{
			static constexpr const char* kLightingModeNames[] = { "Lighting None", "Lambert", "Half Lambert" };
			for (uint32_t materialIndex = 0; materialIndex < materialResources_.size(); ++materialIndex) {
				MaterialGpuResource& material = materialResources_[materialIndex];
				const std::string& materialName = modelData_.materials[materialIndex].name;
				ImGui::PushID(static_cast<int>(materialIndex));
				if (ImGui::TreeNode(materialName.empty() ? "Material" : materialName.c_str())) 
				{
					int lightingMode = material.materialData->lightingMode;
					if (ImGui::Combo("Lighting", &lightingMode, kLightingModeNames, IM_ARRAYSIZE(kLightingModeNames)))
					{
						material.materialData->lightingMode = lightingMode;
					}

					ImGui::ColorEdit4("Color", &material.materialData->color.x);
					ImGui::DragFloat2("UV Translate", &material.uvTransform.translate.x, 0.01f);
					ImGui::DragFloat2("UV Scale", &material.uvTransform.scale.x, 0.01f, 0.01f, 100.0f);
					ImGui::SliderAngle("UV Rotate", &material.uvTransform.rotate.z);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	}

	ImGui::PopID();
	ImGui::End();
#endif
}

void Particle::CreateRootSignature()
{
	// PSが読むテクスチャ用SRV
	D3D12_DESCRIPTOR_RANGE textureRange{};
	textureRange.BaseShaderRegister = 0; // PSのt0
	textureRange.NumDescriptors = 1;
	textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// VSが読むインスタンス行列用SRV
	D3D12_DESCRIPTOR_RANGE instancingRange{};
	instancingRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	instancingRange.NumDescriptors = 1;
	instancingRange.BaseShaderRegister = 0; // VSのt0
	instancingRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};

	// [0] Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// [1] インスタンス行列
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &instancingRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2] テクスチャ
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSampler{};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.NumStaticSamplers = 1;

	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr))
	{
		logger_->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Particle::CreatePipelineState()
{
	D3D12_INPUT_ELEMENT_DESC inputElements[3] = {};
	inputElements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements, _countof(inputElements) };

	// ノーマルブレンド(アルファブレンド)用の設定
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 四角形を両面から表示する
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	vertexShaderBlob_ = shaderCompiler_->Compile(L"Particle.VS.hlsl", L"vs_6_0");
	pixelShaderBlob_ = shaderCompiler_->Compile(L"Particle.PS.hlsl", L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	// Particle自身のRootSignatureを指定する
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = inputLayout;
	pipelineDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	pipelineDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = dxCommon_->GetRenderTargetFormat();
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// 物体との前後関係は判定するが、パーティクル自身は深度を書かない
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void Particle::CreateConstantBuffers()
{

	materialResources_.resize(modelData_.materials.size());
	for (MaterialGpuResource& material : materialResources_)
	{
		material.materialResource = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
		material.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&material.materialData));
		material.materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.materialData->lightingMode = static_cast<int32_t>(LightingMode::None);
		material.materialData->uvTransform = MatrixUtility::MakeIdentity4x4();
	}

}

void Particle::CreateTexture() 
{
	for (uint32_t materialIndex = 0; materialIndex < modelData_.materials.size(); ++materialIndex)
	{
		const MaterialData& materialData = modelData_.materials[materialIndex];
		MaterialGpuResource& materialResource = materialResources_[materialIndex];
		DirectX::ScratchImage mipImages = DirectXResource::LoadTexture(materialData.textureFilePath);
		const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
		materialResource.textureResource = DirectXResource::CreateTextureResource(dxCommon_->GetDevice(), metadata);
		DirectXResource::UploadTextureData(materialResource.textureResource.Get(), mipImages);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = metadata.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
		const uint32_t srvIndex = dxCommon_->AllocateSrvIndex();
		dxCommon_->GetDevice()->CreateShaderResourceView(materialResource.textureResource.Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(srvIndex));
		materialResource.textureSrvHandleGPU = dxCommon_->GetSrvGpuHandle(srvIndex);
	}
}

void Particle::CreateVertexBuffer()
{

	CreateQuadVertices();

	meshResources_.resize(modelData_.meshes.size());
	for (uint32_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex)
	{
		const MeshData& mesh = modelData_.meshes[meshIndex];
		MeshGpuResource& meshResource = meshResources_[meshIndex];
		meshResource.vertexResource = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * mesh.vertices.size());
		meshResource.vertexBufferView.BufferLocation = meshResource.vertexResource->GetGPUVirtualAddress();
		meshResource.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * mesh.vertices.size());
		meshResource.vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* vertexData = nullptr;
		meshResource.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		std::memcpy(vertexData, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
	}
}

void Particle::CreateSphereVertices()
{
	constexpr uint32_t kSubdivisionX = 32;
	constexpr uint32_t kSubdivisionY = 16;
	constexpr float kPi = 3.14159265f;

	modelData_ = {};
	modelData_.materials.push_back({ "Sphere Material", "resources/uvChecker.png" });
	modelData_.meshes.push_back({ "Sphere", {}, 0 });
	MeshData& sphere = modelData_.meshes.back();
	sphere.vertices.reserve(kSubdivisionX * kSubdivisionY * 6);

	for (uint32_t y = 0; y < kSubdivisionY; ++y)
	{
		const float v0 = static_cast<float>(y) / static_cast<float>(kSubdivisionY);
		const float v1 = static_cast<float>(y + 1) / static_cast<float>(kSubdivisionY);
		const float phi0 = v0 * kPi;
		const float phi1 = v1 * kPi;
		for (uint32_t x = 0; x < kSubdivisionX; ++x)
		{
			const float u0 = static_cast<float>(x) / static_cast<float>(kSubdivisionX);
			const float u1 = static_cast<float>(x + 1) / static_cast<float>(kSubdivisionX);
			const float theta0 = u0 * 2.0f * kPi;
			const float theta1 = u1 * 2.0f * kPi;
			auto makeVertex = [](float phi, float theta, float u, float v) {
				const Vector3 normal = { std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta) };
				return VertexData{ { normal.x, normal.y, normal.z, 1.0f }, { u, v }, normal };
				};

			const VertexData topLeft = makeVertex(phi0, theta0, u0, v0);
			const VertexData topRight = makeVertex(phi0, theta1, u1, v0);
			const VertexData bottomLeft = makeVertex(phi1, theta0, u0, v1);
			const VertexData bottomRight = makeVertex(phi1, theta1, u1, v1);
			sphere.vertices.insert(sphere.vertices.end(), { topLeft, topRight, bottomLeft, topRight, bottomRight, bottomLeft });
		}
	}
}

void Particle::CreateQuadVertices()
{
	modelData_ = {};

	modelData_.materials.push_back({ "Particle Material", "resources/circle.png" });

	modelData_.meshes.push_back({ "Particle Quad", {}, 0 });

	auto& vertices = modelData_.meshes[0].vertices;

	vertices = 
	{
		{ { 1.0f,  1.0f, 0.0f, 1.0f},
		  {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },

		{ {-1.0f,  1.0f, 0.0f, 1.0f},
		  {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },

		{ { 1.0f, -1.0f, 0.0f, 1.0f},
		  {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },

		{ { 1.0f, -1.0f, 0.0f, 1.0f},
		  {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },

		{ {-1.0f,  1.0f, 0.0f, 1.0f},
		  {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },

		{ {-1.0f, -1.0f, 0.0f, 1.0f},
		  {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
	};
}

ParticleData Particle::MakeNewParticle(std::mt19937& randomEngine)
{

	ParticleData particle{};
	particle.transform.scale = { 1.0f, 1.0f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };

	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	// 位置と速度を[-1, 1]でランダムに初期化
	particle.transform.translate = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };

	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	// 色をランダムに初期化
	particle.color = { distColor(randomEngine), distColor(randomEngine) ,distColor(randomEngine), 1.0f };

	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	// 生存時間をランダムに初期化
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;

	return particle;
}

void Particle::CreateInstancingResource()
{
	instancingResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kMaxInstanceCount);

	const HRESULT hr = instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));
	assert(SUCCEEDED(hr));

	for (uint32_t i = 0; i < kMaxInstanceCount; ++i)
	{
		instancingData_[i].WVP = MatrixUtility::MakeIdentity4x4();
		instancingData_[i].World = MatrixUtility::MakeIdentity4x4();
		instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };

		particles_[i] = MakeNewParticle(randomEngine_);
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = kMaxInstanceCount;
	srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(ParticleForGPU));
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	const uint32_t srvIndex = dxCommon_->AllocateSrvIndex();

	dxCommon_->GetDevice()->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(srvIndex));

	instancingSrvHandleGPU_ = dxCommon_->GetSrvGpuHandle(srvIndex);
}
