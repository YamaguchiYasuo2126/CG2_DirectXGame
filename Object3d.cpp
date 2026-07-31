#include "Object3d.h"

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

void Object3d::Initialize(
	DirectXCommon* dxCommon,
	ShaderCompiler* shaderCompiler,
	Logger* logger,
	const std::string& modelName,
	const std::string& objectName) {
	dxCommon_ = dxCommon;
	shaderCompiler_ = shaderCompiler;
	logger_ = logger;
	modelName_ = modelName;
	objectName_ = objectName;

	if (modelName_ == "sphere") {
		transform_.rotate = { 0.0f, 0.0f, 0.0f };
		transform_.translate = { 3.0f, 0.0f, 0.0f };
	}
	else if (modelName_ == "teapot.obj") {
		transform_.rotate = { 0.0f, 0.0f, 0.0f };
		transform_.translate = { 0.0f, -1.5f, 0.0f };
	}
	else if (modelName_ == "bunny.obj") {
		transform_.rotate = { 0.0f, 0.0f, 0.0f };
		transform_.translate = { -3.0f, -1.2f, 0.0f };
	}
	else if (modelName_ == "multiMesh.obj") {
		transform_.rotate = { 0.0f, 0.0f, 0.0f };
		transform_.translate = { 0.0f, 2.5f, 0.0f };
	}
	else if (modelName_ == "multiMaterial.obj") {
		transform_.rotate = { 0.0f, 0.0f, 0.0f };
		transform_.translate = { 4.5f, -1.5f, 0.0f };
	}

	CreateRootSignature();
	CreatePipelineState();
	CreateVertexBuffer();
	CreateConstantBuffers();
	CreateTexture();
}

void Object3d::Finalize() {
	meshResources_.clear();
	materialResources_.clear();
	directionalLightResource_.Reset();
	wvpResource_.Reset();
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
	const Matrix4x4 worldMatrix = MatrixUtility::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	wvpData_->WVP = MatrixUtility::Multiply(worldMatrix, viewProjection_);
	wvpData_->World = worldMatrix;

	for (MaterialGpuResource& material : materialResources_) {
		material.materialData->uvTransform = MatrixUtility::MakeAffineMatrix(
			material.uvTransform.scale,
			material.uvTransform.rotate,
			material.uvTransform.translate);
	}
}

void Object3d::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	for (uint32_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];
		assert(mesh.materialIndex < materialResources_.size());
		const MeshGpuResource& meshResource = meshResources_[meshIndex];
		const MaterialGpuResource& materialResource = materialResources_[mesh.materialIndex];

		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, materialResource.materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, materialResource.textureSrvHandleGPU);
		commandList->DrawInstanced(static_cast<UINT>(mesh.vertices.size()), 1, 0, 0);
	}
}

void Object3d::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Settings");
	ImGui::PushID(objectName_.c_str());
	if (ImGui::CollapsingHeader(objectName_.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate (rad)", &transform_.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.01f, 100.0f);

		if (ImGui::TreeNode("Materials")) {
			static constexpr const char* kLightingModeNames[] = { "Lighting None", "Lambert", "Half Lambert" };
			for (uint32_t materialIndex = 0; materialIndex < materialResources_.size(); ++materialIndex) {
				MaterialGpuResource& material = materialResources_[materialIndex];
				const std::string& materialName = modelData_.materials[materialIndex].name;
				ImGui::PushID(static_cast<int>(materialIndex));
				if (ImGui::TreeNode(materialName.empty() ? "Material" : materialName.c_str())) {
					int lightingMode = material.materialData->lightingMode;
					if (ImGui::Combo("Lighting", &lightingMode, kLightingModeNames, IM_ARRAYSIZE(kLightingModeNames))) {
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

	if (ImGui::CollapsingHeader("Light")) {
		ImGui::ColorEdit4("Color", &directionalLightData_->color.x);
		ImGui::DragFloat3("Direction", &directionalLightData_->direction.x, 0.01f, -1.0f, 1.0f);
		const float length = std::sqrt(
			directionalLightData_->direction.x * directionalLightData_->direction.x +
			directionalLightData_->direction.y * directionalLightData_->direction.y +
			directionalLightData_->direction.z * directionalLightData_->direction.z);
		if (length > 0.0f) {
			directionalLightData_->direction.x /= length;
			directionalLightData_->direction.y /= length;
			directionalLightData_->direction.z /= length;
		}
		ImGui::DragFloat("Intensity", &directionalLightData_->intensity, 0.01f);
	}
	ImGui::PopID();
	ImGui::End();
#endif
}

void Object3d::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.NumDescriptors = 1;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

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
	if (FAILED(hr)) {
		logger_->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3d::CreatePipelineState() {
	D3D12_INPUT_ELEMENT_DESC inputElements[3] = {};
	inputElements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements, _countof(inputElements) };

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	vertexShaderBlob_ = shaderCompiler_->Compile(L"Object3D.VS.hlsl", L"vs_6_0");
	pixelShaderBlob_ = shaderCompiler_->Compile(L"Object3D.PS.hlsl", L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
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
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void Object3d::CreateConstantBuffers() {
	wvpResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	wvpData_->WVP = MatrixUtility::MakeIdentity4x4();
	wvpData_->World = MatrixUtility::MakeIdentity4x4();

	materialResources_.resize(modelData_.materials.size());
	for (MaterialGpuResource& material : materialResources_) {
		material.materialResource = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
		material.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&material.materialData));
		material.materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.materialData->lightingMode = static_cast<int32_t>(LightingMode::HalfLambert);
		material.materialData->uvTransform = MatrixUtility::MakeIdentity4x4();
	}

	directionalLightResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;
}

void Object3d::CreateTexture() {
	for (uint32_t materialIndex = 0; materialIndex < modelData_.materials.size(); ++materialIndex) {
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

void Object3d::CreateVertexBuffer() {
	if (modelName_ == "sphere") {
		CreateSphereVertices();
	}
	else {
		modelData_ = ModelLoader::LoadObjFile("resources", modelName_);
	}

	meshResources_.resize(modelData_.meshes.size());
	for (uint32_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
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

void Object3d::CreateSphereVertices() {
	constexpr uint32_t kSubdivisionX = 32;
	constexpr uint32_t kSubdivisionY = 16;
	constexpr float kPi = 3.14159265f;

	modelData_ = {};
	modelData_.materials.push_back({ "Sphere Material", "resources/uvChecker.png" });
	modelData_.meshes.push_back({ "Sphere", {}, 0 });
	MeshData& sphere = modelData_.meshes.back();
	sphere.vertices.reserve(kSubdivisionX * kSubdivisionY * 6);

	for (uint32_t y = 0; y < kSubdivisionY; ++y) {
		const float v0 = static_cast<float>(y) / static_cast<float>(kSubdivisionY);
		const float v1 = static_cast<float>(y + 1) / static_cast<float>(kSubdivisionY);
		const float phi0 = v0 * kPi;
		const float phi1 = v1 * kPi;
		for (uint32_t x = 0; x < kSubdivisionX; ++x) {
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
